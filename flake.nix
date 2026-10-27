{
  description = "NSL Snap: a screenshot tool for Windows, macOS and Linux (GNOME on Wayland)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: nixpkgs.legacyPackages.${system};

      # D-013: C++20, Qt 6.8 Widgets, CMake. CMakeLists.txt only asks for
      # Qt6 >= 6.8, so nixpkgs' own Qt 6 (newer) is used as-is instead of
      # pinning to the 6.8.3 aqtinstall build from docs/07.
      nslSnapFor =
        pkgs:
        pkgs.qt6.callPackage (
          {
            stdenv,
            cmake,
            ninja,
            pkg-config,
            wrapQtAppsHook,
            qtbase,
            qtsvg,
            qtimageformats,
          }:
          stdenv.mkDerivation {
            pname = "nsl-snap";
            version = "0.1.0";

            src = self;

            nativeBuildInputs = [
              cmake
              ninja
              pkg-config
              wrapQtAppsHook
            ];
            buildInputs = [
              qtbase
              qtsvg
              qtimageformats
            ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
            ];

            # CMakeLists.txt has no install() rules (it only builds the
            # binary and tests, see docs/07); install it manually.
            installPhase = ''
              runHook preInstall
              mkdir -p "$out/bin"
              cp nsl-snap "$out/bin/"
              runHook postInstall
            '';

            # The GNOME capture backend talks to the extension over D-Bus
            # (docs/01-architecture.md); the extension itself ships as the
            # separate nsl-snap-gnome-extension package below.
            doCheck = false;

            meta = {
              description = "Screenshot tool with clean single-window capture, inspired by WinSnap";
              homepage = "https://github.com/michaelpfister/ch-nsl-snap";
              license = nixpkgs.lib.licenses.mit;
              mainProgram = "nsl-snap";
              platforms = nixpkgs.lib.platforms.linux;
            };
          }
        ) { };

      # Packages the GNOME Shell extension (gnome-extension/nsl-snap@netslash.ch)
      # as its own derivation, following the shape of pkgs.gnomeExtensions.*:
      # the UUID directory under share/gnome-shell/extensions, schema
      # precompiled. GNOME Shell only discovers new extensions after a
      # logout/login on Wayland (docs/07); this package does not change
      # that, it only builds the files.
      nslSnapExtensionFor =
        pkgs:
        let
          uuid = "nsl-snap@netslash.ch";
        in
        pkgs.stdenvNoCC.mkDerivation {
          pname = "nsl-snap-gnome-extension";
          version = "0.1.0";

          src = self + "/gnome-extension/${uuid}";

          nativeBuildInputs = [ pkgs.glib ];

          dontBuild = true;

          installPhase = ''
            runHook preInstall

            dest="$out/share/gnome-shell/extensions/${uuid}"
            mkdir -p "$dest"
            cp -r . "$dest"
            glib-compile-schemas --strict "$dest/schemas"

            runHook postInstall
          '';

          passthru.extensionUuid = uuid;

          meta = {
            description = "NSL Snap GNOME Shell extension: pick, capture, return image bytes";
            homepage = "https://github.com/michaelpfister/ch-nsl-snap";
            license = nixpkgs.lib.licenses.mit;
            platforms = nixpkgs.lib.platforms.linux;
          };
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          nsl-snap = nslSnapFor pkgs;
          nsl-snap-gnome-extension = nslSnapExtensionFor pkgs;
          default = nslSnapFor pkgs;
        }
      );

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.nsl-snap}/bin/nsl-snap";
        };
      });

      devShells = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          # Replaces docs/07-development-setup.md's apt-install + aqtinstall
          # steps for Linux: `nix develop` gives a shell with Qt 6, CMake,
          # Ninja and the OpenGL/Vulkan/xkbcommon headers Qt's build needs.
          default = pkgs.mkShell {
            packages = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
              pkgs.qt6.qtbase
              pkgs.qt6.qtsvg
              pkgs.qt6.qtimageformats
              pkgs.qt6.qttools
              pkgs.qt6.wrapQtAppsHook
              pkgs.glib # glib-compile-schemas, for the GNOME extension
              pkgs.libGL
              pkgs.libglvnd
              pkgs.vulkan-loader
              pkgs.libxkbcommon
            ];
          };
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          nsl-snap = (nslSnapFor pkgs).overrideAttrs (old: {
            cmakeFlags = old.cmakeFlags ++ [ "-DCMAKE_BUILD_TYPE=Debug" ];
            doCheck = true;
            checkPhase = ''
              runHook preCheck
              QT_QPA_PLATFORM=offscreen ctest --test-dir . --output-on-failure
              runHook postCheck
            '';
          });
        }
      );
    };
}
