#pragma once

#include <filesystem>
#include <format>
#include <optional>

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
