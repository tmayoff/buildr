COMPILER=clang++
CPP_STD=c++26

COMMAND=${COMPILER} -g -DCLANG_PATH=\"$(shell which clang++)\" -DPKGCONFIG_PATH=\"$(shell which pkg-config)\" -DBOOTSTRAP_ONLY main.cpp -lboost_program_options -lboost_process -std=${CPP_STD} -o bootstrapped

bootstrapped: main.cpp compile_commands.json
	${COMMAND}

compile_commands.json: Makefile
	jq -n '{"command":"${COMMAND}", "directory": "${PWD}", "file": "main.cpp", "output": "bootstrapped"}' > compile_commands.json

