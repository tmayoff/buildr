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
  std::vector<std::string> link_args;
  std::vector<std::string> compile_args;
  std::vector<fs::path> sources;
  std::vector<std::string> dependencies;
};

struct Dependency {
  std::vector<std::string> link_args;
  std::vector<std::string> compile_args;
};

void build(const BuildOptions &opts);
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

  const auto opts = read_config();
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

  for (const auto &[key, val] : *tbl["dependencies"].as_table()) {
    opts.dependencies.push_back(std::string(key.str()));
    if (key == "boost") {
      const auto node = *val.as_table();
      const auto boost_modules = *node["modules"].as_array();
      for (const auto &mod : boost_modules) {
        opts.dependencies.push_back(std::string("boost_") +
                                    mod.as_string()->value_or(""));
      }
    }
  }

  for (const auto &src : *tbl["srcs"].as_array()) {
    opts.sources.push_back(fs::path(src.as_string()->value_or("")));
  }

  tbl["compile_args"].as_array()->for_each([&](const auto &node) {
    opts.compile_args.push_back(node.as_string()->value_or(""));
  });
  tbl["link_args"].as_array()->for_each([&](const auto &node) {
    opts.link_args.push_back(node.as_string()->value_or(""));
  });

  return opts;
}

auto run(const std::string &cmd) {
  boost::process::ipstream stream;
  boost::process::child proc(cmd, boost::process::std_out > stream);

  std::string out;

  std::string line;
  while (proc.running() && stream && getline(stream, line)) {
    out += line;
  }

  proc.wait();
  return out;
}

auto get_dep_options(std::vector<std::string> deps) {
  Dependency dependency{};

  boost::asio::io_context ctx;

  for (const auto &dep : deps) {
    std::println("Looking for {}...", dep);

    if (dep == "boost") {
      dependency.compile_args.push_back(std::format(
          "-L{}",
          run(std::format("pkg-config --variable=includedir {}", dep))));

      dependency.link_args.push_back(std::format(
          "-L{}", run(std::format("pkg-config --variable=libdir {}", dep))));
      continue;
    } else if (dep.starts_with("boost_")) {
      dependency.link_args.push_back("-l" + dep);
      continue;
    }

    const auto cflags = run(std::format("pkg-config --cflags {}", dep));
    const auto libs = run(std::format("pkg-config --libs {}", dep));
    std::vector<std::string> compile_args;
    std::vector<std::string> link_args;
    boost::split(compile_args, cflags, boost::is_any_of(" "));
    boost::split(link_args, libs, boost::is_any_of(" "));
    dependency.compile_args.insert(dependency.compile_args.end(),
                                   compile_args.begin(), compile_args.end());
    dependency.link_args.insert(dependency.link_args.end(), link_args.begin(),
                                link_args.end());
  }

  return dependency;
}

auto compile_source_file(fs::path build_root, fs::path source,
                         const std::vector<std::string> &extra_compile_args,
                         const std::vector<std::string> &extra_link_args) {
  fs::path out = build_root / fs::path(source.stem().string() + ".o");

  std::vector<std::string> args = extra_compile_args;
  // args.insert(args.end(), extra_link_args.begin(), extra_link_args.end());
  args.push_back("-fprebuilt-module-path=build");

  if (source.extension() == ".cppm") {
    std::println("Compiling module file");
    out = build_root / fs::path(source.stem().string() + ".pcm");
    args.push_back("--precompile");
  } else {
    std::println("Compiling cpp file");
  }

  const auto cmd = std::format("{} -c {} {} -o {}", "clang++",
                               boost::algorithm::join(args, " "),
                               source.string(), out.string());
  std::println("{}", cmd);
  system(cmd.c_str());

  return out;
}

void build(const BuildOptions &opts) {
  namespace fs = std::filesystem;

  const auto &build_dir = opts.build_root;
  if (!fs::exists(build_dir)) std::filesystem::create_directory(build_dir);

  const std::string compiler = "clang++";
  const std::vector<fs::path> &sources = opts.sources;

  const auto deps = get_dep_options(opts.dependencies);
  auto compile_args = deps.compile_args;
  compile_args.insert(compile_args.end(), opts.compile_args.begin(),
                      opts.compile_args.end());

  std::vector<std::string> link_args = {};
  link_args.insert(link_args.end(), deps.link_args.begin(),
                   deps.link_args.end());
  link_args.insert(link_args.end(), opts.link_args.begin(),
                   opts.link_args.end());

  std::vector<fs::path> objs;
  for (const auto &src : sources) {
    objs.push_back(
        compile_source_file(build_dir, src, compile_args, link_args));
  }

  const std::vector<std::string> paths =
      objs |
      std::ranges::views::transform([](const auto &p) { return p.string(); }) |
      std::ranges::to<std::vector>();

  std::println("LINKNING....");
  const auto cmd = std::format("{} {} {} -o build/buildr", "clang++ ",
                               boost::algorithm::join(link_args, " "),
                               boost::algorithm::join(paths, " "));
  std::println("{}", cmd);

  system(cmd.c_str());
}
