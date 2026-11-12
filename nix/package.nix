# Snapslash as a Nix package (docs/07-development-setup.md, "NixOS").
# Built with nixpkgs' current Qt 6; the app needs 6.8 or later.
{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  wayland-scanner,
  wayland,
  qt6,
  kdePackages,
  pipewire,
  wrapGAppsHook3,
  gtk3,
  gsettings-desktop-schemas,
}:

let
  # Same as project(... VERSION) in CMakeLists.txt; the release workflow
  # checks both against the tag (D-037).
  version = "1.0.0";
  # "beta.1" of "0.3.0-beta.1": the pre-release part the app reports.
  suffix = lib.concatStringsSep "-" (lib.drop 1 (lib.splitString "-" version));
in
stdenv.mkDerivation {
  pname = "snapslash";
  inherit version;

  src = lib.cleanSource ../.;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    wayland-scanner
    qt6.wrapQtAppsHook
    # lrelease for the translations (D-038).
    qt6.qttools
    # Provides the GSettings schemas GTK needs when Qt uses its GTK theme
    # (file dialogs); without them GLib aborts the app (D-033).
    wrapGAppsHook3
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
    qt6.qtimageformats
    # Encoding videos (F-50, D-039), with nixpkgs' FFmpeg.
    qt6.qtmultimedia
    wayland
    # Videos on GNOME: Mutter's screen cast through PipeWire (D-039).
    pipewire
    # The capture overlay as a layer surface above all windows (D-031).
    kdePackages.layer-shell-qt
    gtk3
    gsettings-desktop-schemas
  ];

  cmakeFlags = lib.optional (suffix != "") "-DNSL_VERSION_SUFFIX=${suffix}";

  # One wrapper instead of two: the GTK variables go into Qt's wrapper.
  dontWrapGApps = true;
  preFixup = ''
    qtWrapperArgs+=("''${gappsWrapperArgs[@]}")
  '';

  meta = {
    description = "Screenshot tool for clean window captures, with an annotation editor";
    homepage = "https://github.com/michaelpfister/ch-nsl-snap";
    platforms = lib.platforms.linux;
    mainProgram = "snapslash";
  };
}
