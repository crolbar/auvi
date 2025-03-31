{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = inputs: let
    system = "x86_64-linux";
    pkgs = import inputs.nixpkgs {inherit system;};
  in {
    devShells.${system}.default = let
      run = pkgs.writers.writeBashBin "run" ''
        # make clean
        rm ./auvi
        make -j$(nproc)

        exec ./auvi "$@"
      '';
    in
      pkgs.mkShell {
        packages = with pkgs; [
          openal
          raylib
          run
        ];
      };
    packages.${system}.default = pkgs.stdenv.mkDerivation rec {
      pname = "auvi";
      version = "0.1";

      src = ./.;

      buildInputs = with pkgs; [raylib openal];

      installPhase = ''
        runHook preInstall

        mkdir -p $out/bin
        cp ${pname} $out/bin

        mkdir -p $out/share/applications
        cp $src/${pname}.desktop $out/share/applications

        runHook postInstall
      '';
    };
  };
}
