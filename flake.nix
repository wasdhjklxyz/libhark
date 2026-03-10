{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    systems.url = "github:nix-systems/default";
  };

  outputs = { self, nixpkgs, systems }:
    let
      eachSystem = nixpkgs.lib.genAttrs (import systems);
      version = builtins.replaceStrings [ "\n" ] [ "" ]
        (builtins.readFile ./VERSION);
    in {
      packages = eachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "libhark";
            inherit version;
            src = ./.;
            nativeBuildInputs = with pkgs; [ cmake ];
            cmakeFlags = [ "-DHARK_BUILD_EXAMPLES=ON" ];
          };

          aarch64-static = let
            crossPkgs = pkgs.pkgsCross.aarch64-multiplatform-musl;
          in crossPkgs.stdenv.mkDerivation {
            pname = "libhark";
            inherit version;
            src = ./.;
            nativeBuildInputs = [ crossPkgs.buildPackages.cmake ];
            cmakeFlags = [
              "-DHARK_BUILD_SHARED=OFF"
              "-DHARK_BUILD_EXAMPLES=OFF"
              "-DCMAKE_C_FLAGS=-static"
            ];
          };
        });

      devShells = eachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            name = "libhark";

            packages = with pkgs; [
              # build
              cmake
              gnumake
              pkg-config

              # compilers
              gcc
              clang

              # debug
              gdb
              valgrind
              strace
              ltrace

              # analysis
              clang-tools  # clangd, clang-format, clang-tidy
              cppcheck
              include-what-you-use

              # misc
              bear
              man-pages
              man-pages-posix
            ];

            inputsFrom = [ self.packages.${system}.default ];

            env = {
              CMAKE_EXPORT_COMPILE_COMMANDS = "ON";
            };

            shellHook = ''
              cat > .clangd << EOF
              CompileFlags:
                Add:
                  - -isystem
                  - ${pkgs.glibc.dev}/include
                  - -isystem
                  - ${pkgs.linuxHeaders}/include
              EOF
            '';
          };
        });
    };
}
