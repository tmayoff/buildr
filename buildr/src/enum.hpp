#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/describe.hpp>
#include <optional>
#include <string>

#include "concepts.hpp"

template <DescribeabelEnum E>
inline auto enum_from_string(const std::string& name,
                             bool case_insensitive = false)
    -> std::optional<E> {
  bool found = false;

  std::optional<E> r{};

  boost::mp11::mp_for_each<boost::describe::describe_enumerators<E>>(
      [&](const auto d) {
        const std::string described = d.name;
        if (!r.has_value()) {
          if ((case_insensitive && boost::iequals(name, described)) ||
              (!case_insensitive && name == described))
            r = d.value;
        }
      });

  return r;
}
