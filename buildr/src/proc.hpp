#pragma once

#include <boost/process.hpp>
#include <filesystem>

namespace buildr::proc {

auto run_process(boost::asio::io_context& ctx, const std::filesystem::path& cmd,
                 std::vector<std::string> args) -> boost::process::process;

}
