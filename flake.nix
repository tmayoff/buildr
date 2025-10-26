{
  description = "Flake utils demo";

  inputs = {
    nixpkgs.url = "git+https://codeberg.org/tmayoff/nixpkgs?ref=clang-p2996";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        overlays = [
          (final: prev: {
            boost189 = prev.boost189.override {
              stdenv = prev.llvmPackages_git.libcxxStdenv;
            };

            # tomlplusplus = prev.tomlplusplus.overrideAttrs {
            #   stdenv = prev.llvmPackages_git.libcxxStdenv;
            #   # doCheck = false;
            # };
          })
        ];

        pkgs = import nixpkgs {
          inherit system overlays;
        };

        boost = pkgs.boost189;
        llvm = pkgs.llvmPackages_git;
        stdenv = llvm.libcxxStdenv;

        bootstrapInputs = with pkgs; [
          which
          pkg-config
          boost
          tomlplusplus
        ];

        nativeBuildInputs = with pkgs; [
          llvm.clang-tools
          llvm.libllvm
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
      in rec {
        packages.bootstrap = stdenv.mkDerivation {
          name = "bootstrapper";
          src = ./bootstrap;

          nativeBuildInputs = bootstrapInputs;
          buildInputs = bootstrapInputs;

          buildPhase = ''
            make
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp bootstrapped $out/bin
            ls -la $out/bin
          '';
        };

        packages.bootstrapped-buildr = stdenv.mkDerivation {
          name = "bootstrapped-buildr";
          src = ./buildr;

          nativeBuildInputs =
            [
              packages.bootstrap
            ]
            ++ nativeBuildInputs;

          buildInputs = buildInputs;

          buildPhase = ''
            ${packages.bootstrap}/bin/bootstrapped
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp build/buildr $out/bin
          '';
        };

        packages.buildr = stdenv.mkDerivation {
          name = "buildr";
          src = ./buildr;

          nativeBuildInputs =
            [
              packages.bootstrapped-buildr
            ]
            ++ nativeBuildInputs;

          buildInputs = buildInputs;

          buildPhase = ''
            export BUILDR_LOG_LEVEL=debug
            ${packages.bootstrapped-buildr}/bin/buildr build
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp build/buildr $out/bin
          '';
        };

        packages.default = packages.buildr;

        devShell = pkgs.mkShell.override {stdenv = stdenv;} {
          nativeBuildInputs = with pkgs;
            nativeBuildInputs
            ++ [
              llvm.clang-tools
              llvm.libllvm

              pkg-config
              lldb
              just
            ];

          buildInputs = buildInputs;

          shellHook = ''
            ${pkgs.cachix}/bin/cachix use tmayoff
          '';
        };
      }
    );
}
