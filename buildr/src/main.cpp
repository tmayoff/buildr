#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <print>
#include <toml++/toml.hpp>

#include "format.hpp"

import config_mod;
import build_mod;

namespace fs = std::filesystem;

// NOLINTNEXTLINE
BOOST_DEFINE_ENUM_CLASS(Subcommand, unknown, help, build, run, test);

void print_help();
void build();
void run();
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

  Subcommand subcommand{};
  boost::describe::enum_from_string(vm.at("command").as<std::string>(),
                                    subcommand);

  switch (subcommand) {
    case Subcommand::unknown:
    case Subcommand::help:
      print_help();
      std::exit(1);
    case Subcommand::build:
      build();
    case Subcommand::run:
      run();
    case Subcommand::test:
      test();
      break;
  }
}

void print_help() { std::println("HELP"); }

void build() {
  const auto current_dir = fs::current_path();
  const auto &project_config = config::parse_project(current_dir);

  std::println("Project directory: {}", project_config.root_dir);
  std::println("Build directory: {}", project_config.build_dir);

  const auto &default_target = project_config.targets.front();
  std::println("building: {}", default_target);
  std::println("building: {}", default_target.name);
  std::println("{}", default_target.compile_args);

  builder::generate_compile_commands(
      project_config.build_dir, project_config.root_dir,
      default_target.compile_args, default_target.sources);

  builder::build_target(project_config.build_dir, default_target);
}

void run() {}

void test() {}
