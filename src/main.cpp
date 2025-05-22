#include <boost/program_options.hpp>
#include <filesystem>
#include <format>
#include <iostream>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

struct BuildOptions {
  std::vector<std::filesystem::path> sources;
  std::vector<std::string> dependencies;
};

void build(BuildOptions const &opts);
BuildOptions read_config();

int main(int argc, char **argv) {
  namespace po = boost::program_options;

  po::options_description desc("options");
  desc.add_options()("help", "Show help screen");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") > 0) {
    std::cout << "usage" << std::endl;
    return 1;
  }

  auto const opts = read_config();
  build(opts);
}

BuildOptions read_config() {
  toml::parse_result result = toml::parse_file("buildr.toml");
  if (!result) {
    std::cerr << "Parsing failed:\n" << result.error() << "\n";
    std::exit(1);
  }

  auto tbl = result.table();

  BuildOptions opts{};

  for (auto const &node : *tbl["dependencies"].as_array()) {
    auto const dep = node.value<std::string>().value_or("");
    opts.dependencies.push_back(dep);
  }

  return opts;
}

void build(BuildOptions const &opts) {
  namespace fs = std::filesystem;
  const fs::path build_dir = "build";
  if (!fs::exists(build_dir))
    std::filesystem::create_directory(build_dir);

  const std::string compiler = "clang++";
  std::vector<std::filesystem::path> sources = {"src/main.cpp"};

  for (const auto &src : sources) {
    auto const cmd = std::format(
        "{} {} -o build/buildr {} -g "
        "-I/nix/store/pvj7zs62mbfz6z77yq7f3896zj2qjafq-tomlplusplus-3.4.0/"
        "include -lboost_program_options",
        compiler, src.string(), "-std=c++23");
    std::cout << "compiling: " << cmd << std::endl;
    system(cmd.c_str());
  }
}
