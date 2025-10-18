module;

#include <boost/describe.hpp>
#include <format>
#include <iostream>
#include <print>

#include "enum.hpp"
#include "format.hpp"  // IWYU pragma: export

export module logging;

import ansi_mod;

namespace log {

enum class LogLevel {
  Trace = 0,
  Debug,
  Info,
  Warn,
  Error,
  Fatal,
};

BOOST_DESCRIBE_ENUM(LogLevel, Trace, Debug, Info, Warn, Error, Fatal)

static auto get_log_level() {
  static std::optional<LogLevel> level = std::nullopt;
  if (!level.has_value()) {
    if (getenv("BUILDR_LOG_LEVEL") != nullptr) {
      const auto l_opt =
          enum_from_string<LogLevel>(getenv("BUILDR_LOG_LEVEL"), true);
      if (l_opt) level = l_opt.value_or(LogLevel::Info);
    } else {
      level = LogLevel::Info;
    }
  }

  return level.value();
}

export template <typename... Args>
void trace(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Trace) {
    std::println(std::cout, "{}[trace]{} {}", ansi::kGrey, ansi::kReset,
                 std::format(fmt, std::forward<Args>(args)...));
  }
}

export template <typename... Args>
void debug(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Debug) {
    std::println(std::cout, "{}[debug]{} {}", ansi::kBlue, ansi::kReset,
                 std::format(fmt, std::forward<Args>(args)...));
  }
}

export template <typename... Args>
void info(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Info) {
    std::println(std::cout, "{}[info]{} {}", ansi::kGreen, ansi::kReset,
                 std::format(fmt, std::forward<Args>(args)...));
  }
}

export template <typename... Args>
void warn(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Warn) {
    std::println(std::cout, "{}[warn]{} {}", ansi::kOrange, ansi::kReset,
                 std::format(fmt, std::forward<Args>(args)...));
  }
}

export template <typename... Args>
void error(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Error) {
    std::println(std::cerr, "{}[error]{} {}", ansi::kRed, ansi::kReset,
                 std::format(fmt, std::forward<Args>(args)...));
  }
}

export template <typename... Args>
void fatal(const std::format_string<Args...>& fmt, Args&&... args) {
  if (get_log_level() <= LogLevel::Fatal) {
    std::print(std::cerr, "{}[fatal]{} {}", ansi::kRed, ansi::kReset,
               std::format(fmt, std::forward<Args>(args)...));
  }
  std::terminate();
}

}  // namespace log
