{
  description = "Flake utils demo";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
        };

        boost = pkgs.boost188;

        llvm = pkgs.llvmPackages_21;
      in rec {
        buildr = llvm.stdenv.mkDerivation {
          name = "buildr";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            jq
            which
            pkg-config
          ];
          buildInputs = with pkgs; [
            libpkgconf
            pkg-config
            boost
            tomlplusplus
          ];

          buildPhase = ''
            make -C bootstrap

            cd ./buildr
            ../bootstrap/bootstrapped
            ./build/buildr build
          '';

          installPhase = ''
            mkdir -p $out/bin

            export BUILDR_LOG_LEVEL=debug
            cp build/buildr $out/bin
          '';
        };

        packages.default = buildr;

        devShell = pkgs.mkShell.override {stdenv = llvm.stdenv;} {
          nativeBuildInputs = with pkgs;
            buildr.nativeBuildInputs
            ++ [
              llvm.clang-tools
              llvm.libllvm

              pkg-config
              cmake

              lldb

              just

              meson
              muon
              ninja
            ];

          buildInputs = buildr.buildInputs;
        };
      }
    );
}
