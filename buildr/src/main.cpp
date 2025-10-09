#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <toml++/toml.hpp>

#include "format.hpp"
#include "proc.hpp"

import logging;
import config_mod;
import build_mod;
import dependencies_mod;
import scan_deps;

namespace fs = std::filesystem;

// NOLINTNEXTLINE
BOOST_DEFINE_ENUM_CLASS(Subcommand, unknown, help, build, run, test);

void print_help();
auto build() -> std::optional<fs::path>;
void run(const fs::path &target);
void test();

auto main(int argc, char **argv) -> int {
  namespace po = boost::program_options;

  po::options_description opts("options");
  opts.add_options()("help", "Show help screen")(
      "command", po::value<std::string>(), "command to execute");

  po::positional_options_description pos;
  pos.add("command", 1);

  const auto &parsed = po::command_line_parser(argc, argv)
                           .options(opts)
                           .positional(pos)
                           .allow_unregistered()
                           .run();

  po::variables_map vm;
  po::store(parsed, vm);
  po::notify(vm);

  Subcommand subcommand = Subcommand::help;
  boost::describe::enum_from_string(vm.at("command").as<std::string>(),
                                    subcommand);

  switch (subcommand) {
    case Subcommand::unknown:
    case Subcommand::help:
      print_help();
      std::exit(1);
      return EXIT_SUCCESS;
    case Subcommand::build:
      build();
      break;
    case Subcommand::run: {
      const auto &target = build();
      if (target.has_value())
        run(*target);
      else
        log::error("No target built");
      return EXIT_SUCCESS;
    }
    case Subcommand::test:
      test();
      break;
  }
}

void print_help() { log::info("HELP"); }

auto build() -> std::optional<fs::path> {
  const auto current_dir = fs::current_path();
  const auto &project_config = config::parse_project(current_dir);

  fs::create_directory(project_config.build_dir);

  log::info("Project directory: {}", project_config.root_dir);
  log::info("Build directory: {}", project_config.build_dir);

  const auto &default_target = project_config.targets.front();

  deps::check_deps(default_target.dependencies);

  log::debug("building: {}", default_target.name);

  auto dep_compiler_args = deps::get_compile_args(default_target.dependencies);

  std::vector<std::string> compile_args =
      builder::get_target_compile_args(default_target);
  builder::generate_compile_commands(project_config.root_dir,
                                     project_config.build_dir, compile_args,
                                     default_target.sources);

  auto graph =
      scanner::build_graph(project_config.root_dir, project_config.build_dir);
  if (!graph.has_value()) {
    log::error("Failed to generate build graph");
    std::exit(1);
  }

  scanner::print_graph(graph.value());
  return builder::build_target(graph.value(), project_config.root_dir,
                               project_config.build_dir, default_target);
}

void run(const fs::path &target) {
  const auto current_dir = fs::current_path();
  const auto &project_config = config::parse_project(current_dir);

  boost::asio::io_context io;
  log::debug("running: {}", target);
  buildr::proc::run_process(io, target, {});
}

void test() {}
