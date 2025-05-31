namespace clap {

class OptionsBuilder {};

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union_v<T>;

template <DescribeableStruct T>
auto parse_args(std::ranges::input_range auto&& input_args, T& out) -> T {
  const auto args = input_args | std::views::drop(1);

  const auto type_name = boost::core::demangle(typeid(T).name());

  std::println("Parsing arguments: {} into type: {}", args.size(), type_name);

  using describe_members =
      boost::describe::describe_members<T, boost::describe::mod_any_member>;

  using describe_bases =
      boost::describe::describe_bases<T, boost::describe::mod_any_access>;

  boost::mp11::mp_for_each<describe_members>(
      [&](auto d) { std::println(" .{}={}", d.name, out.*d.pointer); });

  return {};
}

}  // namespace clap
