module;

#include <boost/describe.hpp>
#include <filesystem>
#include <iostream>

export module compile_db;

struct CompileDatabase {
  std::filesystem::path directory;
  std::string command;
  std::filesystem::path file;
  std::filesystem::path output;
};

BOOST_DESCRIBE_STRUCT(CompileDatabase, (), (directory, command, file, output))

export void generate() { std::cout << "COMPILE_DB!!" << std::endl; }
