project_dir := justfile_directory()

default: build

[no-cd]
build:
    nix run -L {{project_dir}}/flake.nix# -- build
