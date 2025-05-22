#include <boost/program_options.hpp>
#include <filesystem>
#include <format>
#include <iostream>

void build();

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

  build();
}

void build() {
  namespace fs = std::filesystem;
  const fs::path build_dir = "build";
  if (!fs::exists(build_dir))
    std::filesystem::create_directory(build_dir);

  const std::string compiler = "clang++";
  std::vector<std::filesystem::path> sources = {"src/main.cpp"};

  for (const auto &src : sources) {
    auto const cmd =
        std::format("{} {} -o build/buildr {} -lboost_program_options",
                    compiler, src.string(), "-std=c++23");
    std::cout << "compiling: " << cmd << std::endl;
    system(cmd.c_str());
  }
}
