module;

#define BOOST_PROCESS_V2_SEPARATE_COMPILATION
#define BOOST_PROCESS_VERSION 2

#include <boost/algorithm/string/join.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/describe/class.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <vector>

#include "format.hpp"

export module build_mod;

import logging;
import config_mod;
import dependencies_mod;

namespace builder {

constexpr auto kCompiler = "clang++";

namespace fs = std::filesystem;
namespace bp = boost::process::v2;
namespace r = std::ranges;
namespace rv = r::views;

using error_code = int;

auto get_compile_command(const fs::path& build_root, const fs::path& source,
                         const std::vector<std::string>& extra_args) {
  const auto cpp_module = source.extension() == ".cppm";
  const std::string out_ext = cpp_module ? ".pcm" : ".o";
  const fs::path out = build_root / fs::path(source.stem().string() + out_ext);

  auto args =
      std::vector{
          extra_args,
          {std::format("-fprebuilt-module-path={}", build_root.string())}} |
      rv::join | r::to<std::vector>();

  if (cpp_module) args.emplace_back("--precompile");

  const bool debug = true;
  if (debug) args.emplace_back(cpp_module ? "-gmodules" : "-g");

  const auto cmd = std::format("{} {} -o {} -c {}", kCompiler,
                               boost::algorithm::join(args, " "), out.string(),
                               source.string());
  return std::tuple(out, cmd);
}

auto check_ts(const fs::path& original_path, const fs::path& build_path) {
  // The epoch of last_time_write is the year 2174, which means the timestamp is
  // most likely to be negative.
  // https://stackoverflow.com/questions/67142013/why-time-since-epoch-from-a-stdfilesystemfile-time-type-is-negative

  const auto ts_file = fs::path(build_path.string() + ".ts");
  const auto original_ts =
      fs::last_write_time(original_path).time_since_epoch();

  std::fstream f(ts_file);
  std::stringstream ss;
  ss << f.rdbuf();

  const auto out_ts = std::stol(
      ss.str().empty() ? std::to_string(original_ts.min().count()) : ss.str());

  return original_ts.count() > out_ts;
}

auto update_ts(const fs::path& original_path, const fs::path& build_path) {
  const auto new_ts =
      fs::last_write_time(original_path).time_since_epoch().count();

  const auto ts_file = fs::path(build_path.string() + ".ts");
  std::ofstream f(ts_file);
  f << new_ts;
}

auto compile_object(const fs::path& src, const std::string& cmd,
                    const fs::path& out) -> std::expected<bool, error_code> {
  if (check_ts(src, out)) {
    log::debug("{}", cmd);

    const auto ret = system(cmd.c_str());
    if (ret != 0) return std::unexpected(ret);

    update_ts(src, out);
    return true;
  }

  log::debug("Skipping build of {}", src.string());
  return false;
}

auto link_object(const std::vector<fs::path>& objs, const std::string& cmd,
                 const fs::path& out) {
  log::info("Linking: {}", out.string());

  const auto ret = system(cmd.c_str());
  if (ret != 0) {
    log::error("Link failed: {}", cmd);
    std::exit(1);
  }
}

export auto build_target(const fs::path& build_dir,
                         const config::BuildTarget& target) {
  const auto compile_args =
      std::vector{deps::get_compile_args(target.dependencies),
                  target.compile_args} |
      rv::join | r::to<std::vector>();

  const auto commands =
      target.sources |
      rv::transform([build_dir, args = compile_args](const fs::path& source) {
        return std::tuple_cat(std::make_tuple(source),
                              get_compile_command(build_dir, source, args));
      }) |
      r::to<std::vector>();

  bool built_obj = false;
  for (const auto& [src, out, cmd] : commands) {
    const auto result = compile_object(src, cmd, out);
    if (!result.has_value()) {
      log::error("Failed to build target: {}", target.name);
      std::exit(result.error());
    }

    if (result.value()) built_obj = true;
  }

  const auto& compiled_objs =
      commands |
      rv::transform([](const auto& tuple) { return std::get<1>(tuple); }) |
      r::to<std::vector>();

  const auto& link_args =
      std::vector{target.link_args, deps::get_link_args(target.dependencies)} |
      rv::join | r::to<std::vector>();

  const auto args =
      std::vector{
          compiled_objs | rv::transform([](const auto& path) {
            return path.string();
          }) | r::to<std::vector>(),
          link_args,
          {
              "-o",
              (build_dir / target.name).string(),
              std::format("-fprebuilt-module-path={}", build_dir.string()),
          },
      } |
      rv::join | r::to<std::vector>();

  const auto cmd =
      std::format("{} {}", kCompiler, boost::algorithm::join(args, " "));

  if (built_obj)
    link_object(compiled_objs, cmd, build_dir / target.name);
  else
    log::debug("Skipping link: {}", target.name);
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
        builder::get_compile_command(build_dir, src, compiler_args);

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
