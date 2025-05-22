set -x

echo "Bootstrap the build system"

clang++ -g --std=c++23 -o buildr-bootstrap \
  $(pkg-config --cflags tomlplusplus) \
  src/main.cpp \
  -lboost_program_options

