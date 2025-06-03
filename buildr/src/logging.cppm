module;

#include <boost/describe.hpp>
#include <format>
#include <iostream>
#include <print>

#include "format.hpp"  // IWYU pragma: export

export module logging;

namespace log {

BOOST_DEFINE_ENUM_CLASS(LogLevel, Trace, Debug, Info, Warn, Error, Fatal)

static auto get_log_level() {
  static std::optional<LogLevel> level = std::nullopt;
  if (!level.has_value()) {
    if (getenv("BUILDR_LOG_LEVEL") != nullptr) {
      LogLevel l{};
      boost::describe::enum_from_string(getenv("BUILDR_LOG_LEVEL"), l);
      level = l;
    } else {
      level = LogLevel::Info;
    }
  }

  return level.value();
}

export template <typename... Args>
void trace(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Trace >= get_log_level())
    std::println(std::cout, fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
void debug(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Debug >= get_log_level())
    std::println(std::cout, fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
void info(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Info >= get_log_level())
    std::println(std::cout, fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
void warn(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Warn >= get_log_level())
    std::println(std::cout, fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
void error(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Error >= get_log_level())
    std::println(std::cerr, fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
void fatal(const std::format_string<Args...>& fmt, Args&&... args) {
  if (LogLevel::Fatal >= get_log_level())
    std::println(std::cerr, fmt, std::forward<Args>(args)...);

  std::terminate();
}

}  // namespace log
