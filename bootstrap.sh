
echo "Bootstrap the build system"

clang++ src/main.cpp -lboost_program_options --std=c++23  -o buildr-bootstrap
