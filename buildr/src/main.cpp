#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
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

struct CompileCommandsEntry {
  fs::path directory;
  std::string command;
  fs::path file;
  fs::path output;
};

BOOST_DESCRIBE_STRUCT(CompileCommandsEntry, (),
                      (directory, command, file, output));

void build() {
  const auto current_dir = fs::current_path();
  const auto &project_config = config::parse_project(current_dir);

  std::println("Project directory: {}", project_config.root_dir);
  std::println("Build directory: {}", project_config.build_dir);
  std::println("Targets: {}", project_config.targets.size());

  const auto &default_target = project_config.targets.front();
  std::println("building: {}", default_target.name);

  std::vector<CompileCommandsEntry> compile_commands;
  for (const auto &src : default_target.sources) {
    const auto [out, cmd] = builder::get_compiler_command(
        project_config.build_dir, src, default_target.compiler_args);

    compile_commands.push_back(CompileCommandsEntry{
        .directory = project_config.build_dir,
        .command = cmd,
        .file = project_config.root_dir / src,
        .output = out,
    });

    system(cmd.c_str());
  }

  std::ofstream c(project_config.build_dir / "compile_commands.json");
  c << boost::json::value_from(compile_commands);
  c.close();
}

void run() {}

void test() {}
