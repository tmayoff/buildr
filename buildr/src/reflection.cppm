module;

#include <experimental/meta>
#include <optional>
#include <string_view>
#include <type_traits>

export module reflection_mod;

template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr std::optional<E> string_to_enum(std::string_view name) {}
