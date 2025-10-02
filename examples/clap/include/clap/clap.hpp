#include <boost/describe.hpp>

namespace clap {

class OptionsBuilder {};

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union_v<T>;

template <DescribeableStruct T>
auto parse_args(int argc, char** argv) -> T {
  using boost_members =
      boost::describe::describe_members<T, boost::describe::mod_any_member>;

  boost::mp11::mp_for_each<boost_members>([](const auto& d) {

  });

  return {};
}

}  // namespace clap
