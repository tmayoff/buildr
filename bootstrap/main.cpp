#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
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
namespace r = std::ranges;
namespace rv = r::views;

constexpr auto kCompiler = CLANG_PATH;
constexpr auto kPkgConfig = PKGCONFIG_PATH;

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

void configure(const BuildOptions &opts);
void build(const BuildOptions &opts);
auto read_config(const fs::path &project_dir) -> BuildOptions;

auto main(int argc, char **argv) -> int {
  fs::path project_dir = fs::current_path();

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

  const auto opts = read_config(project_dir);
  build(opts);
}

auto read_config(const fs::path &project_dir) -> BuildOptions {
  toml::parse_result result =
      toml::parse_file((project_dir / "buildr.toml").string());
  if (!result) {
    std::cerr << "Parsing failed:\n" << result.error() << "\n";
    std::exit(1);
  }

  const auto &tbl = result.table();

  BuildOptions opts{};

  for (const auto &[key, val] : *tbl["dependencies"].as_table()) {
    opts.dependencies.emplace_back(key.str());
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
    opts.sources.emplace_back(src.as_string()->value_or(""));
  }

  tbl["compile_args"].as_array()->for_each([&](const auto &node) {
    opts.compile_args.push_back(node.as_string()->value_or(""));
  });
  tbl["link_args"].as_array()->for_each([&](const auto &node) {
    opts.link_args.push_back(node.as_string()->value_or(""));
  });

  return opts;
}

auto run(const std::string &exe, const std::vector<std::string> &args) {
  namespace bp = boost::process;
  boost::asio::io_context io;
  boost::asio::readable_pipe pout(io.get_executor());

  bp::process proc(io.get_executor(), exe, args,
                   boost::process::process_stdio{.out = pout});

  std::string out;
  boost::system::error_code ec;
  boost::asio::read(pout, boost::asio::dynamic_buffer(out), ec);

  proc.wait();

  boost::trim(out);
  return out;
}

auto get_dep_options(std::vector<std::string> deps) {
  Dependency dependency{};

  boost::asio::io_context ctx;

  for (const auto &dep : deps) {
    std::println("Looking for {}...", dep);

    if (dep == "boost") {
      dependency.compile_args.push_back(std::format(
          "-I{}", run(PKGCONFIG_PATH, {"--variable=includedir", dep})));

      dependency.link_args.push_back(
          std::format("-L{}", run(PKGCONFIG_PATH, {"--variable=libdir", dep})));
      continue;
    } else if (dep.starts_with("boost_")) {
      dependency.link_args.push_back("-l" + dep);
      continue;
    }

    const auto cflags = run(PKGCONFIG_PATH, {"--cflags", dep});
    const auto libs = run(PKGCONFIG_PATH, {"--libs", dep});
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
  const auto cpp_module = source.extension() == ".cppm";
  const std::string out_ext = cpp_module ? ".pcm" : ".o";
  const fs::path out = build_root / fs::path(source.stem().string() + out_ext);

  std::vector<std::vector<std::string>> a_args = std::vector{
      extra_compile_args,
      {
          std::format("-fprebuilt-module-path={}", build_root.string()),
          "-g",
      },
  };

  if (source.extension() == ".cppm") {
    std::println("Compiling module file: {}", source.string());
    a_args.push_back({"--precompile", "-gmodules"});
  } else {
    a_args.push_back({"-c"});
    std::println("Compiling cpp file: {}", source.string());
  }

  a_args.push_back({
      "-o",
      out.string(),
      source.string(),
  });

  const auto args = a_args | rv::join | r::to<std::vector>();

  const auto cmd =
      std::format("{} {}", kCompiler, boost::algorithm::join(args, " "));

  run(kCompiler, args);

  return std::pair(out, cmd);
}

void build(const BuildOptions &opts) {
  namespace fs = std::filesystem;

  const auto &build_dir = opts.build_root;
  if (!fs::exists(build_dir)) std::filesystem::create_directory(build_dir);

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
  toml::array compile_db;
  for (const auto &src : sources) {
    auto [out, cmd] =
        compile_source_file(build_dir, src, compile_args, link_args);
    objs.push_back(out);
    const auto &entry = toml::table{
        {"directory", "/home/tyler/src/buildr"},
        {"command", cmd},
        {"file", src.string()},
        {"output", out.string()},
    };

    compile_db.push_back(entry);
  }

  std::ofstream c("build/compile_commands.json");
  c << toml::json_formatter{compile_db};
  c.close();

  const std::vector<std::string> paths =
      objs | rv::transform([](const auto &p) { return p.string(); }) |
      r::to<std::vector>();

  std::vector<std::string> args =
      std::vector{
          {"-fprebuilt-module-path=build"},
          link_args,
          paths,
          {"-g", "-o", "build/buildr"},
      } |
      rv::join | r::to<std::vector>();

  args.emplace_back("-fprebuilt-module-path=build");

  std::println("LINKNING....");
  std::println("{} {}", kCompiler, boost::algorithm::join(args, " "));
  run(kCompiler, args);
}
