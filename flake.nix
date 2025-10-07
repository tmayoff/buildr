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
      in rec {
        bootstrap = pkgs.llvmPackages_20.stdenv.mkDerivation {
          name = "bootstrap";
          src = ./bootstrap;

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
            nlohmann_json
          ];

          buildPhase = ''
            make
          '';

          installPhase = ''
            mkdir -p $out/bin

            cp bootstrapped $out/bin
          '';
        };
        buildr = pkgs.llvmPackages_20.stdenv.mkDerivation {
          name = "buildr";
          src = ./buildr;

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
            nlohmann_json

            bootstrap
          ];

          buildPhase = ''
            bootstrapped
            build/buildr build
          '';

          installPhase = ''
            mkdir -p $out/bin

            export BUILDR_LOG_LEVEL=debug
            cp build/buildr $out/bin
          '';
        };

        packages.default = buildr;

        devShell = pkgs.mkShell.override {stdenv = pkgs.llvmPackages_20.stdenv;} {
          nativeBuildInputs = with pkgs;
            buildr.nativeBuildInputs
            ++ [
              bootstrap

              llvmPackages_20.clang-tools
              llvmPackages_20.libllvm

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
