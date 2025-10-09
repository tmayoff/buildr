project_dir := justfile_directory()

default: build_b

[no-cd]
build_f:
    nix run -L {{project_dir}}/flake.nix# -- build

[no-cd]
build_b:
    {{project_dir}}/buildr/build/buildr build

[no-cd]
bootstrap:
    make -C {{project_dir}}/bootstrap &&  cd {{project_dir}}/buildr && {{project_dir}}/bootstrap/bootstrapped 
