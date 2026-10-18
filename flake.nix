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

        # Listed as CC0 on raw.pixls.us, with no artist/copyright in its EXIF.
        sampleRaw = pkgs.fetchurl {
          name = "sony-ilce-7sm3-DSC01569.ARW";
          url = "https://raw.pixls.us/getfile.php/5418/nice/Sony%20-%20ILCE-7SM3%20-%2014bit%2014bit%20compressed%20(3:2).ARW";
          hash = "sha256-F+NdmNRItEFk7PQU+YDpJItYKSM+g6GtvuOWtsGhoMY=";
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
                ln -s ${sampleRaw} burst/frame0.arw
                ln -s ${sampleRaw} burst/frame1.arw
                hdrplus burst output.png frame0.arw frame1.arw
                file burst/output.png | tee /dev/stderr \
                  | grep -q "PNG image data, 4256 x 2848, 8-bit/color RGB"
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
