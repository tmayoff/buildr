#pragma once

#include <boost/asio.hpp>
#include <boost/process.hpp>
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

  auto wait() { return proc_.wait(); }

  auto stdout() -> std::string;
  auto stderr() -> std::string;

 private:
  bp::process proc_;
  boost::asio::readable_pipe stdout_;
  boost::asio::readable_pipe stderr_;
};

auto run_process(boost::asio::io_context& ctx, const std::filesystem::path& cmd,
                 std::vector<std::string> args) -> Process;

}  // namespace buildr::proc
