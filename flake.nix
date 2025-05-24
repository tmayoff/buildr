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
        # toml = pkgs.tomlplusplus.overrideAttrs (old: {
        #   src = pkgs.fetchFromGitHub {
        #     owner = "marzer";
        #     repo = "tomlplusplus";
        #     rev = "2f35c28a52fd0ada7600273de9aacb66550bcdcb";
        #     hash = "sha256-iO/lxKrhKF0ILnAHyGDjXOK5ShnnV84oGTNukTLMQQ4=";
        #   };
        #   patches = [];
        # });
      in rec {
        buildr = pkgs.llvmPackages_20.stdenv.mkDerivation {
          name = "buildr";
          src = ./.;

          nativeBuildInputs = with pkgs; [pkg-config];
          buildInputs = with pkgs; [
            boost
            tomlplusplus
            nlohmann_json
          ];

          buildPhase = ''
            cd bootstrap
            make buildr
            cd buildr
            ../bootstrap/bootstrapped 
          '';

          installPhase = ''
            mkdir -p $out/bin

            cp build/buildr $out/bin
          '';
        };

        packages.default = buildr;

        devShell = pkgs.mkShell.override {stdenv = pkgs.llvmPackages_20.stdenv;} {
          nativeBuildInputs = with pkgs; [
            llvmPackages_20.clang-tools

            pkg-config
            cmake

            lldb
            # libllvm

            meson
            muon
            ninja
          ];

          buildInputs = with pkgs; [
            boost
            tomlplusplus
            nlohmann_json
          ];
        };
      }
    );
}
