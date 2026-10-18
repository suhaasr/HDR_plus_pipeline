{
  description = "HDR+ Implementation";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        hdr-plus = pkgs.stdenv.mkDerivation {
          pname = "hdr-plus";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            gnumake
            pkg-config
          ];

          buildInputs = with pkgs; [
            halide
            libtiff
            libpng
            libjpeg
            zlib
            libraw
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          # The build produces two binaries: hdrplus and stack_frames
          installPhase = ''
            runHook preInstall

            mkdir -p $out/bin
            cp hdrplus $out/bin/
            cp stack_frames $out/bin/

            runHook postInstall
          '';

          meta = with pkgs.lib; {
            description = "HDR+ burst photography implementation";
            homepage = "https://github.com/timothybrooks/hdr-plus";
            license = licenses.mit;
            platforms = platforms.unix;
            mainProgram = "hdrplus";
          };
        };

        # CC0 sample from raw.pixls.us, used as a pseudo-burst by the e2e check.
        sampleRaw = pkgs.fetchurl {
          name = "nikon-z6ii-CZP_0303.NEF";
          url = "https://raw.pixls.us/data/Nikon/Z%206_2/CZP_0303.NEF";
          hash = "sha256-hentS85P60OengXDhF1Ve3bRAIDCOTWIIaAHWa/ZMUs=";
        };
      in
      {
        checks = {
          build = hdr-plus;

          e2e =
            pkgs.runCommand "hdr-plus-e2e"
              {
                nativeBuildInputs = [
                  hdr-plus
                  pkgs.file
                ];
              }
              ''
                mkdir burst
                ln -s ${sampleRaw} burst/frame0.nef
                ln -s ${sampleRaw} burst/frame1.nef
                hdrplus burst output.png frame0.nef frame1.nef
                file burst/output.png | tee /dev/stderr \
                  | grep -q "PNG image data, 6064 x 4040, 8-bit/color RGB"
                touch $out
              '';
        };

        packages = {
          default = hdr-plus;
          hdr-plus = hdr-plus;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ hdr-plus ];

          packages = with pkgs; [
            clang-tools
          ];
        };
      }
    );
}
