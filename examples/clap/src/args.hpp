#pragma once
#include <boost/describe.hpp>

struct Args {
  int size;
  // std::variant<BuildArgs> subcommand;
};

BOOST_DESCRIBE_STRUCT(Args, (), (size));