{
  description = "Flake utils demo";

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
      in rec {
        buildr = pkgs.clangStdenv.mkDerivation {
          name = "buildr";
          src = ./.;

          buildInputs = with pkgs; [boost];

          buildPhase = ''
            ./bootstrap.sh
            ls -la

            ./buildr-bootstrap
          '';

          installPhase = ''
            mkdir -p $out/bin

            cp build/buildr $out/bin
          '';
        };

        packages.default = buildr;

        devShell = pkgs.mkShell.override {stdenv = pkgs.clangStdenv;} {
          nativeBuildInputs = with pkgs; [
            clang-tools

            pkg-config
            cmake

            meson
            muon
            ninja
          ];

          buildInputs = with pkgs; [
            boost
            tomlplusplus
          ];
        };
      }
    );
}
