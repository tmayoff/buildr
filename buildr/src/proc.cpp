#include "proc.hpp"

#include <boost/process.hpp>
#include <cassert>

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

}  // namespace buildr::proc
