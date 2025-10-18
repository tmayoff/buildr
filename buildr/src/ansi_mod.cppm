module;

#include <print>
#include <ranges>
#include <string>
#include <vector>

export module ansi_mod;

namespace ansi {

namespace r = std::ranges;
namespace rv = std::ranges::views;

constexpr auto kAnsiEscape = "\033[";

constexpr auto make_ansi_code(const std::vector<std::string>& args) {
  const auto argv = args | rv::join_with(';') | r::to<std::string>();
  return kAnsiEscape + argv;
}

consteval auto make_ansi_colour_code(std::vector<std::string> args) {
  const auto argv = args | rv::join_with(';') | r::to<std::string>();
  return make_ansi_code({argv + "m"});
}

consteval auto make_fg_code(const std::string& colour) {
  return make_ansi_colour_code({"38", "5", colour});
}

export constexpr auto kReset = make_ansi_colour_code({"0"});
export constexpr auto kOrange = make_fg_code("3");
export constexpr auto kRed = make_fg_code("1");
export constexpr auto kGrey = make_fg_code("7");
export constexpr auto kBlue = make_fg_code("4");
export constexpr auto kGreen = make_fg_code("2");

export auto clear_to_end() { std::print("{}", make_ansi_code({"0J"})); }

export auto move_cursor_up(std::size_t lines) {
  std::print("{}", make_ansi_code({std::format("{}A", lines)}));
}

export auto reset_line() {
  std::print("{}{}\r", make_ansi_code({"1F"}), make_ansi_code({"2K"}));
}

}  // namespace ansi
