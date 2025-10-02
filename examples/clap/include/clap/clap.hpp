#include <boost/core/demangle.hpp>
#include <boost/describe.hpp>
#include <print>
#include <ranges>
#include <vector>

#include "format.hpp"  // IWYU pragma: export

namespace clap {

class OptionsBuilder {};

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union_v<T>;

template <DescribeableStruct T>
auto parse_args(const std::vector<std::string>& input_args) -> T {
  const auto args =
      input_args | std::views::drop(1) | std::ranges::to<std::vector>();

  const auto type_name = boost::core::demangle(typeid(T).name());

  std::println("Parsing arguments: {} into type: {}", args, type_name);

  T out{};

  using describe_members =
      boost::describe::describe_members<T, boost::describe::mod_any_member>;

  using describe_bases =
      boost::describe::describe_bases<T, boost::describe::mod_any_access>;

  boost::mp11::mp_for_each<describe_members>(
      [&](auto d) { std::println(" .{}={}", d.name, out.*d.pointer); });

  return {};
}

}  // namespace clap
