module;

#define BOOST_PROCESS_V2_SEPARATE_COMPILATION
#define BOOST_PROCESS_VERSION 2

#include <boost/algorithm/string/join.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/describe/class.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <vector>

#include "format.hpp"

export module build_mod;

import config_mod;

namespace builder {

constexpr auto kCompiler = "clang++";

namespace fs = std::filesystem;
namespace bp = boost::process::v2;

auto get_compiler_command(const fs::path& build_root, const fs::path& source,
                          const std::vector<std::string>& extra_args) {
  fs::path out = build_root / fs::path(source.stem().string() + ".o");

  std::vector<std::string> args = extra_args;

  args.emplace_back(
      std::format("-fprebuilt-module-path={}", build_root.string()));

  if (source.extension() == ".cppm") {
    out = build_root / fs::path(source.stem().string() + ".pcm");
    args.emplace_back("--precompile");
  }
  const auto cmd = std::format("{} {} -g -o {} -c {}", kCompiler,
                               boost::algorithm::join(args, " "), out.string(),
                               source.string());
  return std::pair(out, cmd);
}

export auto build_target(const fs::path& build_dir,
                         const config::BuildTarget& target) {
  namespace r = std::ranges;
  namespace rv = r::views;

  std::println(std::cout, "{}", target.compile_args);
  std::vector<std::pair<fs::path, std::string>> commands =
      target.sources |
      rv::transform(
          [build_dir, args = target.compile_args](const fs::path& source) {
            return get_compiler_command(build_dir, source, args);
          }) |
      r::to<std::vector>();

  std::println(std::cout, "Compiling");
  for (const auto& [out, cmd] : commands) {
    std::println(std::cout, "{}", cmd);
    auto ret = system(cmd.c_str());
    if (ret != 0) {
      std::println(std::cerr, "Failed to build target: {}", target.name);
      std::exit(ret);
    }
  }

  std::println(std::cout, "Linking");

  const auto& deps =
      commands |
      rv::transform([](const auto& pair) { return pair.first.string(); }) |
      r::to<std::vector>();

  std::vector<std::string> args = deps;
  args.insert(args.end(), target.link_args.begin(), target.link_args.end());
  args.emplace_back("-o");
  args.push_back((build_dir / target.name).string());

  boost::asio::io_context ctx;

  // bp::process proc(ctx.get_executor(), kCompiler, args);
  // const auto ret = proc.wait();
  // if (ret != 0) {
  //   std::println(std::cerr, "Link failed: {} {}", kCompiler,
  //                boost::algorithm::join(args, " "));
  // }
}

struct CompileCommandsEntry {
  fs::path directory;
  std::string command;
  fs::path file;
  fs::path output;
};
BOOST_DESCRIBE_STRUCT(CompileCommandsEntry, (),
                      (directory, command, file, output));

export auto generate_compile_commands(
    const fs::path& build_dir, const fs::path& root_dir,
    const std::vector<std::string>& compiler_args,
    const std::vector<fs::path>& sources) {
  std::vector<CompileCommandsEntry> compile_commands;
  for (const auto& src : sources) {
    const auto [out, cmd] =
        builder::get_compiler_command(build_dir, src, compiler_args);

    compile_commands.push_back(CompileCommandsEntry{
        .directory = build_dir,
        .command = cmd,
        .file = root_dir / src,
        .output = out,
    });
  }

  std::ofstream c(build_dir / "compile_commands.json");
  c << boost::json::value_from(compile_commands);
  c.close();
}

}  // namespace builder
