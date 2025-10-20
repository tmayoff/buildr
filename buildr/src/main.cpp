#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <toml++/toml.hpp>

#include "format.hpp"

import logging;
import config_mod;
import build_mod;
import dependencies_mod;
import scan_deps;

namespace fs = std::filesystem;

// NOLINTNEXTLINE
BOOST_DEFINE_ENUM_CLASS(Subcommand, unknown, help, build, clean, run, test);

void print_help(const boost::program_options ::options_description& desc);
void build(const config::ProjectConfig& project_config);
void clean(const config::ProjectConfig& project_config);
void run();
void test();

auto main(int argc, char** argv) -> int {
  namespace po = boost::program_options;

  po::options_description opts("options");
  opts.add_options()("help", "Show help screen")(
      "directory,C", po::value<fs::path>(), "Working directory to use")(
      "command", po::value<std::string>(), "command to execute");

  po::positional_options_description pos;
  pos.add("command", 1);

  const auto& parsed = po::command_line_parser(argc, argv)
                           .options(opts)
                           .positional(pos)
                           .allow_unregistered()
                           .run();

  po::variables_map vm;
  po::store(parsed, vm);
  po::notify(vm);

  Subcommand subcommand = Subcommand::help;
  if (vm.contains("command"))
    boost::describe::enum_from_string(vm.at("command").as<std::string>(),
                                      subcommand);

  fs::path working_directory = vm.contains("directory")
                                   ? vm.at("directory").as<fs::path>()
                                   : fs::current_path();

  const auto& project_config = config::parse_project(working_directory);

  switch (subcommand) {
    case Subcommand::unknown:
    case Subcommand::help:
      print_help(opts);
      std::exit(1);
      return EXIT_FAILURE;
    case Subcommand::build:
      build(project_config);
      break;
    case Subcommand::clean:
      clean(project_config);
      break;
    case Subcommand::run:
      run();
      break;
    case Subcommand::test:
      test();
      break;
  }
}

void print_help(const boost::program_options ::options_description& desc) {
  std::stringstream ss;
  ss << desc;
  log::info("{}", ss.str());
}

void build(const config::ProjectConfig& project_config) {
  fs::create_directory(project_config.build_dir);

  log::info("Project directory: {}", project_config.root_dir);
  log::info("Build directory: {}", project_config.build_dir);

  log::debug("{} target(s)", project_config.targets.size());

  if (project_config.targets.empty()) {
    log::info("No targets to build");
    return;
  }

  const auto& default_target = project_config.targets.front();

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
  builder::build_target(graph.value(), project_config.root_dir,
                        project_config.build_dir, default_target);
}

void clean(const config::ProjectConfig& project_config) {
  fs::remove_all(project_config.build_dir);

  const auto default_target = project_config.targets.front();
  std::vector<std::string> compile_args =
      builder::get_target_compile_args(default_target);

  fs::create_directory(project_config.build_dir);

  builder::generate_compile_commands(project_config.root_dir,
                                     project_config.build_dir, compile_args,
                                     default_target.sources);
}

void run() {}

void test() {}
