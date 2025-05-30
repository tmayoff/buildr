import clap_mod;

#include <boost/describe.hpp>
#include <filesystem>
#include <variant>

struct BuildArgs {
  std::filesystem::path build_dir;
};

struct Args {
  std::variant<BuildArgs> subcommand;
};

BOOST_DESCRIBE_STRUCT(Args, (), ());

auto main(int argc, char** argv) -> int {
  const auto args = clap::parse_args<Args>(argc, argv);
}
