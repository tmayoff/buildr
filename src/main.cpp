#ifndef BOOTSTRAP_ONLY
import compile_db;
#endif

#include <boost/asio/io_context.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <ranges>

#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

struct BuildOptions {
  fs::path build_root = "build";
  std::vector<fs::path> sources;
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

#ifndef BOOTSTRAP_ONLY
  // generate();
#endif
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

auto run(std::string const &cmd) {}

auto get_dep_options(std::vector<std::string> deps) {
  std::string opts;

  boost::asio::io_context ctx;

  for (const auto &dep : deps) {
    boost::process::ipstream stream;
    boost::process::child proc(
        std::format("pkg-config --cflags --libs {}", dep),
        boost::process::std_out > stream);

    std::string line;
    while (stream && getline(stream, line)) {
      opts = std::format(" {}", line);
    }

    proc.wait();
  }

  return opts;
}

auto compile_source_file(fs::path build_root, fs::path source) {
  fs::path out = build_root / fs::path(source.stem().string() + ".o");

  std::vector<std::string> args = {"-std=c++23"};
  args.push_back("-fprebuilt-module-path=build");
  args.push_back("-lboost_program_options");

  if (source.extension() == ".cppm") {
    std::println("Compiling module file");
    out = build_root / fs::path(source.stem().string() + ".pcm");
    args.push_back("--precompile");
  } else {
    // args.push_back("-r");
  }

  auto const cmd = std::format("{} {} {} -c -o {}", "clang++",
                               boost::algorithm::join(args, " "),
                               source.string(), out.string());
  std::println("{}", cmd);
  system(cmd.c_str());

  return out;
}

void build(BuildOptions const &opts) {
  namespace fs = std::filesystem;

  auto const &build_dir = opts.build_root;
  if (!fs::exists(build_dir))
    std::filesystem::create_directory(build_dir);

  const std::string compiler = "clang++";
  std::vector<std::filesystem::path> sources = {"src/compile_db.cppm",
                                                "src/main.cpp"};

  const auto deps = get_dep_options(opts.dependencies);

  std::vector<fs::path> objs;
  for (const auto &src : sources) {
    objs.push_back(compile_source_file(build_dir, src));
  }

  std::vector<std::string> const paths =
      objs |
      std::ranges::views::transform([](auto const &p) { return p.string(); }) |
      std::ranges::to<std::vector>();

  std::string args =
      "-lboost_program_options -fno-builtin-memset -fno-builtin-memcpy";
  args += " " + deps;
  auto const cmd = std::format("{} {} {} -o build/buildr", "clang++",
                               boost::algorithm::join(paths, " "), args);
  std::println("{}", cmd);

  system(cmd.c_str());
}
