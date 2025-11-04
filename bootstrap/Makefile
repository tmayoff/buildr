MKFILE_PATH:=$(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR:=$(dir ${MKFILE_PATH})

COMPILER:=$(shell which clang++)
PKG_CONFIG:=$(shell which pkg-config)

CPP_STD:=c++26

COMMAND=${COMPILER} -g -DCLANG_PATH=\"${COMPILER}\" -DPKGCONFIG_PATH=\"${PKG_CONFIG}\" -DBOOTSTRAP_ONLY -lboost_program_options -lboost_process -std=${CPP_STD} main.cpp -o bootstrapped

bootstrapped: main.cpp compile_commands.json
	${COMMAND}

compile_commands.json: Makefile
	jq -n --arg cmd '${COMMAND}' --arg dir ${MKFILE_DIR} '[{"command":$$cmd, "directory": $$dir, "file": "main.cpp", "output": "bootstrapped"}]' > compile_commands.json
