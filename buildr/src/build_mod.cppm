module;

#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/describe/class.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/json.hpp>
#include <boost/process.hpp>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <vector>

#include "format.hpp"
#include "proc.hpp"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

export module build_mod;

import ansi_mod;
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

auto is_module(const fs::path& p) { return p.extension() == ".cppm"; }

export auto get_build_path(const fs::path& root, const fs::path& build_root,
                           const fs::path& src) {
  const auto cpp_module = is_module(src);
  const std::string out_ext = cpp_module ? ".pcm" : ".o";

  const auto out =
      (fs::relative(build_root, root) / src).replace_extension(out_ext);

  return out;
}

auto get_compile_command(const fs::path& root, const fs::path& build_root,
                         const std::set<fs::path>& module_paths,
                         const fs::path& source,
                         const std::vector<std::string>& extra_args) {
  const auto cpp_module = source.extension() == ".cppm";
  const auto out = get_build_path(root, build_root, source);

  const auto m_paths =
      module_paths | rv::transform([build_root](const auto& p) {
        return std::format("-fprebuilt-module-path={}", build_root / p);
      }) |
      r::to<std::vector>();

  auto args =
      std::vector{extra_args, m_paths} | rv::join | r::to<std::vector>();

  if (cpp_module) args.emplace_back("--precompile");

  const bool debug = true;
  if (debug) args.emplace_back(cpp_module ? "-gmodules" : "-g");

  args.emplace_back("-o");
  args.push_back(out);
  args.emplace_back("-c");
  args.push_back(source);

  return CompileCommand{.out_file = out, .compiler = kCompiler, .args = args};
}

auto get_prebuilt_module_path(const std::vector<fs::path> sources) {
  return sources | rv::filter(is_module) |
         rv::transform([](const auto& m) { return m.parent_path(); }) |
         r::to<std::set>();
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
      ss.str().empty()
          ? std::to_string(static_cast<int64_t>(original_ts.min().count()))
          : ss.str());

  return original_ts.count() > out_ts;
}

auto update_ts(const fs::path& original_path, const fs::path& build_path) {
  const auto new_ts =
      fs::last_write_time(original_path).time_since_epoch().count();

  const auto ts_file = fs::path(build_path.string() + ".ts");
  std::ofstream f(ts_file);
  f << static_cast<int64_t>(new_ts);
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
  const auto module_paths = get_prebuilt_module_path(target.sources);

  boost::asio::io_context ctx;
  const auto task_runner = [&ctx, root, build_root, compile_args,
                            module_paths](fs::path src)
      -> std::expected<std::pair<fs::path, bool>, std::string> {
    const auto& command =
        get_compile_command(root, build_root, module_paths, src, compile_args);

    const auto& out = command.out_file;

    const auto& src_abs = root / src;
    const auto& out_abs = root / out;

    if (!fs::exists(out_abs.parent_path())) {
      fs::create_directories(out_abs.parent_path());
    }

    if (fs::exists(out_abs) && !check_ts(src_abs, out_abs)) {
      log::debug("skipping: {}", src);
      return std::pair{out, false};
    }

    log::debug("Compiling: {}\n\targs: {} {}", src, command.compiler,
               command.args);

    const auto res =
        buildr::proc::run_process_async(ctx, command.compiler, command.args);

    update_ts(src_abs, out_abs);
    if (!res.has_value()) {
      log::error("{}", res.error().message());
      return std::pair{out, false};
    }

    return std::pair{out, true};
  };

  auto compilation_graph = graph;

  bool built_obj = false;
  std::vector<fs::path> compiled_objs;
  compiled_objs.reserve(boost::num_vertices(compilation_graph));

  boost::asio::thread_pool thread_pool(5);

  // Take a task out of module_sources with 0 out degrees,
  // and queue the task
  using vertex_t = scanner::graph_t::vertex_descriptor;
  std::unordered_set<fs::path> queued;

  std::mutex graph_mutex;
  const auto& find_next_task = [&compilation_graph, &graph_mutex,
                                &queued]() -> std::optional<vertex_t> {
    std::lock_guard l(graph_mutex);
    for (auto v :
         boost::make_iterator_range(boost::vertices(compilation_graph))) {
      if (!queued.contains(compilation_graph[v]) &&
          boost::out_degree(v, compilation_graph) == 0)
        return v;
    }

    return std::nullopt;
  };

  const auto& task_completed = [&](const auto& src) {
    std::lock_guard l(graph_mutex);
    queued.erase(src);
    for (const auto& v :
         boost::make_iterator_range(boost::vertices(compilation_graph))) {
      if (compilation_graph[v] == src) {
        boost::clear_vertex(v, compilation_graph);
        boost::remove_vertex(v, compilation_graph);
        return;
      }
    }
  };

  std::condition_variable cond;
  std::optional<std::string> error;
  while ((boost::num_vertices(compilation_graph) > 0 || !queued.empty()) &&
         !error.has_value()) {
    ansi::reset_line();
    log::info("tasks: {}, queued: {}", boost::num_vertices(compilation_graph),
              queued | r::to<std::vector>());

    std::optional<vertex_t> vertex = find_next_task();
    if (!vertex.has_value()) {
      // No tasks available wait for one to finish
      std::unique_lock l(graph_mutex);
      cond.wait_for(l, 5s);  // Wake up just in case
      continue;
    }

    std::lock_guard l(graph_mutex);
    const auto src = compilation_graph[vertex.value()];
    queued.emplace(src);

    boost::asio::post(thread_pool, [&, src]() {
      const auto& result = task_runner(src);
      task_completed(src);

      if (!result.has_value()) {
        error = result.error();
      } else {
        const auto [out, built] = result.value();
        std::lock_guard l(graph_mutex);
        built_obj = built;
        compiled_objs.push_back(out);
      }

      cond.notify_all();
    });
  }

  ansi::reset_line();
  log::info("All tasks queued");

  thread_pool.wait();

  ansi::reset_line();
  log::info("All tasks completed");

  const auto& link_args =
      std::vector{target.link_args, deps::get_link_args(target.dependencies)} |
      rv::join | r::to<std::vector>();

  const auto m_paths =
      module_paths | rv::transform([build_root](const auto& p) {
        return std::format("-fprebuilt-module-path={}", build_root / p);
      }) |
      r::to<std::vector>();

  const auto args =
      std::vector{
          compiled_objs | rv::transform([](const auto& path) {
            return path.string();
          }) | r::to<std::vector>(),
          link_args,
          m_paths,
          {
              "-o",
              (build_root / target.name).string(),
          },
      } |
      rv::join | r::to<std::vector>();

  if (built_obj) {
    log::info("Linking: {}", target.name);
    log::debug("{} {}", kCompiler, boost::algorithm::join(args, " "));
    auto proc = buildr::proc::run_process(ctx, kCompiler, args);

    boost::system::error_code ec;
    const auto ret = proc.wait(ec);
    if (ret != 0) {
      log::error("{}", proc.stderr());
    } else {
      ansi::reset_line();
      log::info("Linked");
    }
  }

  log::info("Finished");
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
  toml::array compile_commands;

  const auto module_paths = get_prebuilt_module_path(sources);

  for (const auto& src : sources) {
    const auto [out, compiler, args] = builder::get_compile_command(
        root, build_root, module_paths, src, compiler_args);

    const auto b_out = builder::get_build_path(root, build_root, src);

    compile_commands.push_back(toml::table{
        {"directory", root.string()},
        {"command",
         std::format("{} {}", compiler, boost::algorithm::join(args, " "))},
        {"file", src.string()},
        {"output", b_out.string()},
    });
  }

  std::ofstream c(build_root / "compile_commands.json");
  c << toml::json_formatter{compile_commands};
  c.close();
}

}  // namespace builder
