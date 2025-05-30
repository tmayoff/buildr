#pragma once

#include <boost/describe.hpp>
#include <filesystem>
#include <format>
#include <optional>
#include <vector>

template <typename T>
concept DescribeableStruct =
    boost::describe::has_describe_bases<T>::value &&
    boost::describe::has_describe_members<T>::value && !std::is_union<T>::value;

template <DescribeableStruct T>
struct std::formatter<T, char> {
  constexpr auto parse(format_parse_context& ctx) {
    auto it = ctx.begin(), end = ctx.end();

    if (it != end && *it != '}') {
      throw std::runtime_error("invalid format");
    }

    return it;
  }

  auto format(const T& t, format_context& ctx) const {
    using namespace boost::describe;

    using Bd = describe_bases<T, mod_any_access>;
    using Md = describe_members<T, mod_any_access>;

    auto out = ctx.out();

    *out++ = '{';

    bool first = true;

    boost::mp11::mp_for_each<Bd>([&](auto D) {
      if (!first) {
        *out++ = ',';
      }

      first = false;

      out = std::format_to(out, " {}", (typename decltype(D)::type const&)t);
    });

    boost::mp11::mp_for_each<Md>([&](auto D) {
      if (!first) {
        *out++ = ',';
      }

      first = false;

      out = std::format_to(out, " .{}={}", D.name, t.*D.pointer);
    });

    if (!first) {
      *out++ = ' ';
    }

    *out++ = '}';

    return out;
  }
};

template <>
struct std::formatter<std::filesystem::path, char> {
  bool quoted = false;

  template <class ParseContext>
  constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
    auto it = ctx.begin(), end = ctx.end();

    if (it != end && *it != '}') {
      throw std::runtime_error("invalid format");
    }

    return it;
  }

  template <class FmtContext>
  auto format(std::filesystem::path path, FmtContext& ctx) const
      -> FmtContext::iterator {
    return std::format_to(ctx.out(), "{}", path.string());
  }
};

template <typename T>
struct std::formatter<std::optional<T>, char> {
  bool quoted = false;

  template <class ParseContext>
  constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
    auto it = ctx.begin(), end = ctx.end();

    if (it != end && *it != '}') {
      throw std::runtime_error("invalid format");
    }

    return it;
  }

  template <class FmtContext>
  auto format(std::optional<T> opt, FmtContext& ctx) const
      -> FmtContext::iterator {
    if (opt.has_value()) {
      return std::format_to(ctx.out(), "{}", opt.value());
    }
    return std::format_to(ctx.out(), "none");
  }
};

template <typename T>
struct std::formatter<std::vector<T>, char> {
  bool quoted = false;

  template <class ParseContext>
  constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
    auto it = ctx.begin(), end = ctx.end();

    if (it != end && *it != '}') {
      throw std::runtime_error("invalid format");
    }

    return it;
  }

  auto format(const std::vector<T>& t, format_context& ctx) const {
    auto out = ctx.out();

    *out++ = '[';

    bool first = true;

    for (const auto& obj : t) {
      if (!first) {
        *out++ = ',';
        *out++ = ' ';
      }

      first = false;

      out = std::format_to(out, "{}", obj);
    }

    *out++ = ']';

    return out;
  }
};
