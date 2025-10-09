module;
#include <unistd.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/describe/class.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
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

using namespace std::chrono_literals;
namespace fs = std::filesystem;
namespace r = std::ranges;
namespace rv = r::views;

using error_code = int;

struct CompileCommand {
  fs::path out_file;
  fs::path compiler;
  std::vector<std::string> args;
};

export auto get_build_path(const fs::path& root, const fs::path& build_root,
                           const fs::path& src) {
  const auto cpp_module = src.extension() == ".cppm";
  const std::string out_ext = cpp_module ? ".pcm" : ".o";

  auto out = fs::relative(build_root, root) / (cpp_module ? "modules" / src.stem() : src);
  out.replace_extension(out_ext);

  return out;
}

auto get_compile_command(const fs::path& root, const fs::path& build_root,
                         const fs::path& source,
                         const std::vector<std::string>& extra_args) {
  const auto cpp_module = source.extension() == ".cppm";
  const auto out = get_build_path(root, build_root, source);

  auto args =
      std::vector{
          extra_args,
          {std::format("-fprebuilt-module-path={}", build_root / "modules")}} |
      rv::join | r::to<std::vector>();

  if (cpp_module) args.emplace_back("--precompile");

  const bool debug = true;
  if (debug) args.emplace_back(cpp_module ? "-gmodules" : "-g");

  args.emplace_back("-o");
  args.push_back(out);
  args.emplace_back("-c");
  args.push_back(source);

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

export auto get_target_compile_args(const config::BuildTarget& target) {
  return std::vector{deps::get_compile_args(target.dependencies),
                     target.compile_args,
                     {target.include_dirs | rv::transform([](const auto& i) {
                        return std::format("-I{}", i);
                      }) |
                      r::to<std::vector>()}} |
         rv::join | r::to<std::vector>();
}

export auto build_target(const scanner::graph_t& graph, const fs::path& root,
                         const fs::path& build_root,
                         const config::BuildTarget& target) {
  if (boost::num_vertices(graph) == 0) return;

  const auto compile_args = get_target_compile_args(target);

  boost::asio::io_context ctx;
  const auto task_runner = [&ctx, root, build_root, compile_args](
                               fs::path src) -> std::optional<fs::path> {
    const auto& command =
        get_compile_command(root, build_root, src, compile_args);

    const auto& out = command.out_file;

    const auto& src_abs = root / src;
    const auto& out_abs = root / out;

    if (!fs::exists(out_abs.parent_path())) {
      fs::create_directories(out_abs.parent_path());
    }

    if (fs::exists(out_abs) && !check_ts(src_abs, out_abs)) {
      log::debug("skipping: {}", src);
      return out;
    }

    log::debug("Compiling: {}", src);
    log::debug("\t{}", command.args);
    auto proc = buildr::proc::run_process(ctx, command.compiler, command.args);
    const auto ret = proc.wait();
    if (ret != 0) {
      log::error("{}", proc.stderr());
      std::exit(1);
    }

    update_ts(src_abs, out_abs);

    log::debug("Finished: {}", build_root / out);

    return out;
  };

  auto compilation_graph = graph;

  bool built_obj = false;
  std::vector<fs::path> compiled_objs;
  compiled_objs.reserve(boost::num_vertices(compilation_graph));

  boost::asio::thread_pool thread_pool(5);

  // Take a task out of module_sources with 0 out degrees,
  // and queue the task
  using vertex_t = scanner::graph_t::vertex_descriptor;
  struct AsyncResult {
    fs::path src;
    std::optional<fs::path> out;
  };
  std::map<fs::path, std::future<AsyncResult>> futs;

  std::mutex graph_mutex;
  const auto& find_next_task = [&compilation_graph, &graph_mutex,
                                &futs]() -> std::optional<vertex_t> {
    std::lock_guard l(graph_mutex);
    for (auto v :
         boost::make_iterator_range(boost::vertices(compilation_graph))) {
      if (!futs.contains(compilation_graph[v]) &&
          boost::out_degree(v, compilation_graph) == 0)
        return v;
    }

    return std::nullopt;
  };

  const auto& remove_vertex = [&](const auto& src) {
    std::lock_guard l(graph_mutex);
    for (const auto& v :
         boost::make_iterator_range(boost::vertices(compilation_graph))) {
      if (compilation_graph[v] == src) {
        boost::clear_vertex(v, compilation_graph);
        boost::remove_vertex(v, compilation_graph);
        return;
      }
    }
  };

  while (boost::num_vertices(compilation_graph) > 0 || !futs.empty()) {
    std::this_thread::sleep_for(250ms);
    log::debug("tasks: {}, queued: {}", boost::num_vertices(compilation_graph),
               futs.size());
    scanner::print_graph(compilation_graph);

    for (auto it = futs.begin(); it != futs.end();) {
      auto& [src, f] = *it;
      if (f.wait_for(0s) == std::future_status::ready) {
        const auto& [src, out] = f.get();
        if (out.has_value()) compiled_objs.push_back(*out);
        it = futs.erase(it);

        remove_vertex(src);
      } else
        ++it;
    }

    const auto& vertex = find_next_task();
    if (!vertex.has_value()) continue;
    const auto src = compilation_graph[vertex.value()];

    std::promise<AsyncResult> promise;
    futs.emplace(src, promise.get_future());
    boost::asio::post(thread_pool, [&task_runner, src,
                                    promise = std::move(promise)]() mutable {
      const auto& out = task_runner(src);
      promise.set_value({.src = src, .out = out});
    });
  }

  log::debug("All tasks queued");

  thread_pool.wait();

  log::debug("All tasks completed");

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
              std::format("-fprebuilt-module-path={}", build_root / "modules"),
              "-o",
              (build_root / target.name).string(),
          },
      } |
      rv::join | r::to<std::vector>();
  log::debug("Linking: {}", target.name);
  log::info("{} {}", kCompiler, boost::algorithm::join(args, " "));
  auto proc = buildr::proc::run_process(ctx, kCompiler, args);
  const auto ret = proc.wait();
  if (ret != 0) {
    log::error("{}", proc.stderr());
  } else
    log::debug("Linked");
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
    const fs::path& root, const fs::path& build_root,
    const std::vector<std::string>& compiler_args,
    const std::vector<fs::path>& sources) {
  std::vector<CompileCommandsEntry> compile_commands;
  for (const auto& src : sources) {
    const auto [out, compiler, args] =
        builder::get_compile_command(root, build_root, src, compiler_args);

    const auto b_out = builder::get_build_path(root, build_root, src);

    compile_commands.push_back(CompileCommandsEntry{
        .directory = root,
        .command =
            std::format("{} {}", compiler, boost::algorithm::join(args, " ")),
        .file = src,
        .output = b_out,
    });
  }

  std::ofstream c(build_root / "compile_commands.json");
  c << boost::json::value_from(compile_commands);
  c.close();
}

}  // namespace builder
