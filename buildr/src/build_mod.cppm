module;

#include <unistd.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/describe/class.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <list>
#include <ranges>
#include <vector>

#include "format.hpp"
#include "proc.hpp"

export module build_mod;

import logging;
import config_mod;
import dependencies_mod;
import scan_deps;

namespace builder {

constexpr auto kCompiler = "clang++";

namespace fs = std::filesystem;
namespace r = std::ranges;
namespace rv = r::views;

using error_code = int;

struct CompileCommand {
  fs::path out_file;
  fs::path compiler;
  std::vector<std::string> args;
};

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

  args.emplace_back("-o");
  args.push_back(out.string());
  args.emplace_back("-c");
  args.push_back((build_root / ".." / source).string());

  return CompileCommand{.out_file = out, .compiler = kCompiler, .args = args};
}

auto check_ts(const fs::path& original_path, const fs::path& build_path) {
  // The epoch of last_time_write is the year 2174, which means the timestamp is
  // most likely to be negative.
  // https://stackoverflow.com/questions/67142013/why-time-since-epoch-from-a-stdfilesystemfile-time-type-is-negative

  if (!fs::exists(original_path)) {
    throw std::runtime_error(
        std::format("File is missing: {} {}", original_path, build_path));
  }

  if (!fs::exists(build_path)) {
    return true;
  }

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

auto link_object(const std::vector<fs::path>& objs, const std::string& cmd,
                 const fs::path& out) {
  log::info("Linking: {}", out.string());

  const auto ret = system(cmd.c_str());
  if (ret != 0) {
    log::error("Link failed: {}", cmd);
    std::exit(1);
  }
}

export auto build_target(const scanner::graph_t& graph,
                         const fs::path& build_dir,
                         const config::BuildTarget& target) {
  const auto compile_args =
      std::vector{deps::get_compile_args(target.dependencies),
                  target.compile_args} |
      rv::join | r::to<std::vector>();

  std::list<scanner::graph_t::vertex_descriptor> sorted_vertices;
  boost::topological_sort(graph, std::back_inserter(sorted_vertices));
  auto sources = sorted_vertices |
                 rv::transform([graph](const auto& v) { return graph[v]; }) |
                 rv::filter([](const auto& p) { return fs::exists(p); }) |
                 r::to<std::vector>();

  auto cpp_sources = target.sources | rv::filter([](const auto& p) {
                       return p.extension() == ".cpp";
                     });

  sources.insert(sources.end(), cpp_sources.begin(), cpp_sources.end());

  const auto commands =
      sources |
      rv::transform([build_dir, args = compile_args](const fs::path& source) {
        return std::tuple{source, get_compile_command(build_dir, source, args)};
      }) |
      r::to<std::vector>();

  boost::asio::io_context ctx;

  bool built_obj = false;
  namespace bp = boost::process;
  std::vector<bp::process> compile_processes;
  compile_processes.reserve(commands.size());

  for (const auto& [src, compile_commands] : commands) {
    if (fs::exists(compile_commands.out_file) ||
        check_ts(src, compile_commands.out_file)) {
      log::debug("Compiling: {}", src);
      auto proc = buildr::proc::run_process(ctx, compile_commands.compiler,
                                            compile_commands.args);

      built_obj = true;

      const auto ret = proc.wait();
      if (ret != 0) {
        log::error("{}", proc.stderr());
        std::exit(ret);
      }

      update_ts(src, compile_commands.out_file);
    } else {
      log::debug("Skipping compiling: {}", src);
    }
  }

  const auto& compiled_objs = commands | rv::transform([](const auto& tuple) {
                                return std::get<1>(tuple).out_file;
                              }) |
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
              std::format("-fprebuilt-module-path={}", build_dir.string()),
              "-o",
              (build_dir / target.name).string(),
          },
      } |
      rv::join | r::to<std::vector>();

  log::debug("Linking: {}", target.name);
  log::info("{} {}", kCompiler, boost::algorithm::join(args, " "));
  auto proc = buildr::proc::run_process(ctx, kCompiler, args);
  const auto ret = proc.wait();
  if (ret != 0) {
    log::error("{}", proc.stderr());
  }
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
    const auto [out, compiler, args] =
        builder::get_compile_command(build_dir, src, compiler_args);

    compile_commands.push_back(CompileCommandsEntry{
        .directory = build_dir,
        .command =
            std::format("{} {}", compiler, boost::algorithm::join(args, " ")),
        .file = root_dir / src,
        .output = out,
    });
  }

  std::ofstream c(build_dir / "compile_commands.json");
  c << boost::json::value_from(compile_commands);
  c.close();
}

}  // namespace builder
