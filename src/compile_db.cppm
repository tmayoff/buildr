module;

#include <boost/describe.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>

export module compile_db;

namespace compiledb {

struct CompileDatabase {
  std::filesystem::path directory;
  std::string command;
  std::filesystem::path file;
  std::filesystem::path output;
};

BOOST_DESCRIBE_STRUCT(CompileDatabase, (), (directory, command, file, output))

void generate() {
  CompileDatabase b{};

  // auto const json_str = boost::json::value_from(b);
  // std::ofstream f("build/compile_commands.json");
  // f << json_str;
  // f.close();
}

}  // namespace compiledb
