module;

#include <boost/describe.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/json.hpp>
#include <expected>
#include <filesystem>
#include <iostream>
#include <print>
#include <regex>

#include "format.hpp"
#include "proc.hpp"

export module scan_deps;

import logging;

namespace scanner {
namespace fs = std::filesystem;
namespace r = std::ranges;

struct Provides {
  std::string logical_name;
  bool is_interface;
  fs::path source_path;
};
BOOST_DESCRIBE_STRUCT(Provides, (), (logical_name, is_interface, source_path))

struct Requires {
  std::string logical_name;
  fs::path source_path;
};
BOOST_DESCRIBE_STRUCT(Requires, (), (logical_name, source_path))

struct Rule {
  fs::path primary_output;
  std::optional<std::vector<Provides>> provides;

  std::optional<std::vector<Requires>> required;
};
BOOST_DESCRIBE_STRUCT(Rule, (), (primary_output, provides, required))

struct ScanDeps {
  int revision;
  int version;
  std::vector<Rule> rules;
};
BOOST_DESCRIBE_STRUCT(ScanDeps, (), (revision, version, rules))

export using graph_t = boost::adjacency_list<boost::setS, boost::listS,
                                             boost::bidirectionalS, fs::path>;

enum class Error { Parse };

auto get_src_path(const fs::path& root, const fs::path& build_root,
                  const fs::path& build) {
  const auto cpp_module = build.extension() == ".pcm";
  const std::string src_ext = cpp_module ? ".cppm" : ".cpp";

  auto out = fs::relative(build, build_root);
  out.replace_extension(src_ext);
  return out;
}

export auto build_graph(const fs::path& root, const fs::path& build_root)
    -> std::expected<graph_t, Error> {
  const std::string cmd =
      std::format("clang-scan-deps -format=p1689 -compilation-database {}",
                  build_root / "compile_commands.json");

  const std::vector<std::string> args = {
      "-format=p1689", "-compilation-database",
      (build_root / "compile_commands.json").string()};

  constexpr std::string_view kScanDepsExe = "clang-scan-deps";

  namespace bp = boost::process;

  boost::asio::io_context ctx;
  boost::asio::readable_pipe pipe{ctx};
  auto proc = buildr::proc::run_process(ctx, kScanDepsExe, args);
  proc.wait();

  auto out = proc.stdout();

  std::println("{}", out);

  // Fix invalid keys
  std::regex regex("(\".*)-(.*\": )");
  out = std::regex_replace(out, regex, "$1_$2");

  regex = std::regex("\"requires\": ");
  out = std::regex_replace(out, regex, "\"required\": ");

  boost::system::error_code ec;
  const auto json = boost::json::parse(out, ec);
  if (ec) {
    log::error("Failed to parse scan deps: {}", ec.message());
    return std::unexpected(Error::Parse);
  }

  const auto scanned = boost::json::value_to<ScanDeps>(json, ec);
  if (ec) {
    log::error("Failed to deserialize scan deps: {}", ec.message());
    return std::unexpected(Error::Parse);
  }

  graph_t graph;
  std::map<fs::path, graph_t::vertex_descriptor> vertices;
  const auto find_or_add =
      [&](const auto& v_path) -> graph_t::vertex_descriptor {
    if (!vertices.contains(v_path))
      vertices.emplace(v_path, boost::add_vertex(v_path, graph));
    return vertices.at(v_path);
  };

  for (const auto& rule : scanned.rules) {
    const auto output = rule.primary_output;
    if (output.empty()) continue;

    const auto src = get_src_path(root, build_root, output);
    // if (src.empty()) continue;
    log::debug("found file: {} from: {}", src, output);

    const auto provides =
        rule.provides.value_or(std::vector{Provides{.source_path = src}});
    for (const auto& p : provides) {
      const auto v = find_or_add(p.source_path);

      if (rule.required.has_value()) {
        r::for_each(rule.required.value_or(std::vector<Requires>()),
                    [&](const auto& req) {
                      log::debug("connecting {} -> {}", p.source_path,
                                 req.source_path);

                      auto r = find_or_add(req.source_path);
                      boost::add_edge(v, r, graph);
                    });
      }
    }
  }

  return graph;
}

namespace color {
constexpr auto kReset = "\033[0m";
constexpr auto kGreen = "\033[32m";
constexpr auto kCyan = "\033[36m";
constexpr auto kYellow = "\033[33m";
constexpr auto kGray = "\033[90m";
}  // namespace color

export auto print_graph(const graph_t& graph) {
  std::string print;
  print += std::format("\033[2J");

  print += std::format("{}{}{} ({})\n", color::kCyan, "📦 Task Graph",
                       color::kReset, boost::num_vertices(graph));

  for (auto v : boost::make_iterator_range(boost::vertices(graph))) {
    const auto indeg = boost::in_degree(v, graph);
    const auto outdeg = boost::out_degree(v, graph);

    print += std::format("{}{}{}  {}(in={}, out={}){}\n", color::kGreen, "•",
                         color::kReset, graph[v], indeg, outdeg,
                         outdeg > 0 ? ":" : "");

    // Outgoing edges (dependencies)
    auto edges = boost::make_iterator_range(out_edges(v, graph));
    std::vector<std::string> targets;
    for (auto e : edges) {
      targets.push_back(graph[target(e, graph)]);
    }

    for (size_t i = 0; i < targets.size(); ++i) {
      bool last = (i == targets.size() - 1);
      print +=
          std::format("{}  {}{} {}{}\n", color::kGray, last ? "└──" : "├──",
                      color::kYellow, targets[i], color::kReset);
    }
  }

  std::cout << print;
}

}  // namespace scanner
