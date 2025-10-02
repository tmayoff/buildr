#include <boost/describe.hpp>
#include <filesystem>
#include <ranges>
#include <variant>
#include <vector>

#include "clap/clap.hpp"
#include "clap/format.hpp"

namespace r = std::ranges;
namespace rv = r::views;

struct BuildArgs {
  std::filesystem::path build_dir;
};

struct Args {
  int size;
  std::variant<BuildArgs> subcommand;
};

BOOST_DESCRIBE_STRUCT(Args, (), (size));

auto main(int argc, char** argv) -> int {
  auto a = std::span(argv, argc) |
           rv::transform([](const char* v) { return std::string(v); }) |
           r::to<std::vector>();

  Args args = clap::parse_args<Args>(a);

  std::println("Output args: {}", args);
}
