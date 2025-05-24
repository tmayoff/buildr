<<<<<<< HEAD:examples/clap/include/clap/clap.hpp
=======
module;

#include <boost/core/demangle.hpp>
<<<<<<< HEAD:examples/clap/include/clap/clap.hpp
>>>>>>> 2a1b081 (testing clap):examples/clap/src/clap_mod.cppm
#include <boost/describe.hpp>
#include <print>
=======
#include <boost/describe.hpp>
#include <print>

export module clap_mod;
>>>>>>> e7e3986 (testing clap):examples/clap/src/clap_mod.cppm

namespace clap {

class OptionsBuilder {};

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union_v<T>;

template <DescribeableStruct T>
auto parse_args(int argc, char** argv) -> T {
  const auto type_name = boost::core::demangle(typeid(T).name());

  std::println("Parsing arguments, {} into type: {}", argc, type_name);

  using boost_members =
      boost::describe::describe_members<T, boost::describe::mod_any_member>;

  boost::mp11::mp_for_each<boost_members>(

      [](const auto& d) { std::println("{}", d.name); });

  return {};
}

}  // namespace clap
