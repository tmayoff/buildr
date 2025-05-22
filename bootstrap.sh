
echo "Bootstrap the build system"

clang++ src/main.cpp -g -lboost_program_options $(pkg-config --cflags-only-I tomlplusplus) --std=c++23 -o buildr-bootstrap
