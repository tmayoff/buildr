#include "proc.hpp"

#include <boost/process.hpp>
#include <cassert>

namespace buildr::proc {

auto run_process(boost::asio::io_context& ctx, const std::filesystem::path& cmd,
                 std::vector<std::string> args) -> boost::process::process {
  const auto exe =
      cmd.is_absolute()
          ? cmd.string()
          : boost::process::environment::find_executable(cmd.string());

  return boost::process::process(ctx.get_executor(), exe, args);
}

}  // namespace buildr::proc
