set -x

echo "Bootstrap the build system"

clang++ -DBOOTSTRAP_ONLY -g --std=c++26 -o buildr-bootstrap \
  $(pkg-config --cflags tomlplusplus) \
  src/main.cpp \
  -lboost_program_options

