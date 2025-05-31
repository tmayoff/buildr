#include <boost/describe.hpp>
#include <filesystem>
#include <ranges>
#include <variant>

struct BuildArgs {
  std::filesystem::path build_dir;
};

auto main(int argc, char** argv) -> int {
  auto a = std::span(argv, argc) | std::views::transform([](const char* v) {
             return std::string_view(v);
           });

  Args args{};
  clap::parse_args(a, args);
}
