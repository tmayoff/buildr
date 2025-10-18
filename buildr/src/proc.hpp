#pragma once

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <expected>
#include <filesystem>

namespace buildr::proc {

namespace bp = boost::process;

class Process {
 public:
  Process(bp::process proc, boost::asio::readable_pipe stdout,
          boost::asio::readable_pipe stderr)
      : proc_(std::move(proc)),
        stdout_(std::move(stdout)),
        stderr_(std::move(stderr)) {}

  auto wait(boost::system::error_code& ec) { return proc_.wait(ec); }

  auto stdout() -> std::string;
  auto stderr() -> std::string;

 private:
  bp::process proc_;
  boost::asio::readable_pipe stdout_;
  boost::asio::readable_pipe stderr_;
};

auto run_process(boost::asio::io_context& ctx, const std::filesystem::path& cmd,
                 std::vector<std::string> args) -> Process;

auto run_process_async(boost::asio::io_context& ctx,
                       const std::filesystem::path& cmd,
                       std::vector<std::string> args)
    -> std::expected<void, std::error_code>;

}  // namespace buildr::proc
