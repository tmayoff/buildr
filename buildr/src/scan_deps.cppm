module;

#include <boost/describe.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/json.hpp>
#include <expected>
#include <filesystem>
#include <regex>

#include "format.hpp"
#include "proc.hpp"

export module scan_deps;

import logging;

namespace scanner {
namespace fs = std::filesystem;

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

  // NOLINTNEXTLINE
  std::optional<std::vector<Requires>> _requires_;
};
BOOST_DESCRIBE_STRUCT(Rule, (), (primary_output, provides, _requires_))

struct ScanDeps {
  int revision;
  int version;
  std::vector<Rule> rules;
};
BOOST_DESCRIBE_STRUCT(ScanDeps, (), (revision, version, rules))

export using graph_t = boost::adjacency_list<boost::listS, boost::vecS,
                                      boost::directedS, fs::path>;

enum class Error { Parse };

export auto build_graph(const fs::path& build_root)
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

  // Fix invalid keys
  std::regex regex("(\".*)-(.*\": )");
  out = std::regex_replace(out, regex, "$1_$2");

  regex = std::regex("\"requires\": ");
  out = std::regex_replace(out, regex, "\"_requires_\": ");

  boost::system::error_code ec;
  auto json = boost::json::parse(out, ec);
  if (ec) {
    log::error("Failed to parse scan deps: {}", ec.message());
    return std::unexpected(Error::Parse);
  }

  auto scanned = boost::json::value_to<ScanDeps>(json, ec);
  if (ec) {
    log::error("Failed to deserialize scan deps: {}", ec.message());
    return std::unexpected(Error::Parse);
  }
  std::vector<std::pair<fs::path, fs::path>> deps;

  for (const auto& rule : scanned.rules) {
    std::optional<fs::path> source;

    auto provides = rule.provides.value_or(std::vector<Provides>());
    for (const auto& p : provides) {
      source = p.source_path;
    }

    auto reqs = rule._requires_.value_or(std::vector<Requires>());
    for (const auto& req : reqs) {
      if (source.has_value()) {
        deps.emplace_back(*source, req.source_path);
      }
    }
  }

  graph_t graph(scanned.rules.size());
  std::map<fs::path, graph_t::vertex_descriptor> vertices;
  for (const auto& [source, req] : deps) {
    auto find_or_add = [&](const auto& v_path) -> graph_t::vertex_descriptor {
      if (!vertices.contains(v_path))
        vertices.emplace(v_path, boost::add_vertex(v_path, graph));
      return vertices.at(v_path);
    };

    boost::add_edge(find_or_add(source), find_or_add(req), graph);
  }

  return graph;
}
}  // namespace scanner
