module;

#include <boost/algorithm/string.hpp>
#include <experimental/meta>
#include <optional>
#include <string_view>
#include <type_traits>

export module reflection_mod;

export template <typename E,
                 bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr auto string_to_enum(std::string_view name,
                              bool case_insensitive = false)
    -> std::optional<E> {
  if constexpr (Enumerable) {
    template for (constexpr auto kE :
                  std::define_static_array(std::meta::enumerators_of(^^E))) {
      if (case_insensitive &&
              boost::iequals(name, std::meta::identifier_of(kE)) ||
          (!case_insensitive && name == std::meta::identifier_of(kE)))
        return [:kE:];
    }
  }
  return std::nullopt;
}
