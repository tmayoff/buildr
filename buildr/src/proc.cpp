#include "proc.hpp"

#include <boost/process.hpp>
#include <cassert>
#include <expected>

import logging;

namespace buildr::proc {

namespace bp = boost::process;

auto Process::stdout() -> std::string {
  std::string out;

  boost::system::error_code ec;
  boost::asio::read(stdout_, boost::asio::dynamic_buffer(out), ec);

  return out;
}

auto Process::stderr() -> std::string {
  std::string out;

  boost::system::error_code ec;
  boost::asio::read(stderr_, boost::asio::dynamic_buffer(out), ec);

  return out;
}

auto run_process(boost::asio::io_context& ctx, const std::filesystem::path& cmd,
                 std::vector<std::string> args) -> Process {
  const auto exe = cmd.is_absolute()
                       ? cmd.string()
                       : bp::environment::find_executable(cmd.string());

  boost::asio::readable_pipe stdout{ctx};
  boost::asio::readable_pipe stderr{ctx};

  return {
      bp::process(ctx.get_executor(), exe, args,
                  bp::process_stdio{.in = {}, .out = stdout, .err = stderr}),
      std::move(stdout), std::move(stderr)};
}

auto run_process_async(boost::asio::io_context& ctx,
                       const std::filesystem::path& cmd,
                       std::vector<std::string> args)
    -> std::expected<void, std::error_code> {
  const auto exe = cmd.is_absolute()
                       ? cmd.string()
                       : bp::environment::find_executable(cmd.string());

  std::pair<bool, bool> got_io;
  std::mutex m;
  std::condition_variable cond;

  auto proc = bp::process(ctx, exe, args);

  boost::system::error_code ec;
  int ret = proc.wait(ec);
  if (ret != 0) {
    return std::unexpected(static_cast<std::error_code>(ec));
  }

  return {};
}

}  // namespace buildr::proc
