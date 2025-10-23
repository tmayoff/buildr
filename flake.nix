{
  description = "Flake utils demo";

  inputs.nixpkgs = {
    type = "github";
    owner = "RossComputerGuy";
    repo = "nixpkgs";
    ref = "feat/better-llvm";
  };
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        overlays = [
          (final: prev: {
            llvmPackages_bloomberg = prev.llvmPackages_git.overrideScope (llvmFinal: llvmPrev: {
              monorepoSrc = prev.fetchFromGitHub {
                owner = "bloomberg";
                repo = "clang-p2996";
                rev = "d77eff1cbd78fd065668acf93b1f5f400d39134d";
                hash = "sha256-Jblcv53b7/1x06BsYafbNMIEFVJ3eZ9K8yVh47G5udE=";
              };

              # llvm = llvmPrev.llvm.overrideAttrs (old: {
                version = "22.0.0";
              #   src = prev.fetchFromGitHub {
              #     owner = "bloomberg";
              #     repo = "clang-p2996";
              #     rev = "d77eff1cbd78fd065668acf93b1f5f400d39134d";
              #     hash = "sha256-Jblcv53b7/1x06BsYafbNMIEFVJ3eZ9K8yVh47G5udE=";
              #   };

              #   # Force rebuild of tblgen from same sources
              #   nativeBuildInputs = old.nativeBuildInputs or [] ++ [llvmPrev.llvm-bootstrap-tblgen];

              #   # Disable some checks for speed
                doCheck = false;
              # });

              # # Clang built from same monorepo
              # clang = llvmPrev.clang.overrideAttrs (old: {
              #   src = llvmFinal.llvm.src;
              #   version = llvmFinal.llvm.version;
              # });
            });
          })
        ];

        pkgs = import nixpkgs {
          inherit system overlays;
        };

        boost = pkgs.boost188;
        llvm = pkgs.llvmPackages_bloomberg;

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
        packages.bootstrap = llvm.stdenv.mkDerivation {
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

        packages.bootstrapped-buildr = llvm.stdenv.mkDerivation {
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

        packages.buildr = llvm.stdenv.mkDerivation {
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

        devShell = pkgs.mkShell.override {stdenv = llvm.stdenv;} {
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
