# NSL Snap as a Nix package (docs/07-development-setup.md, "NixOS").
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
  wrapGAppsHook3,
  gtk3,
  gsettings-desktop-schemas,
}:

stdenv.mkDerivation {
  pname = "nsl-snap";
  version = "0.1.0";

  src = lib.cleanSource ../.;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    wayland-scanner
    qt6.wrapQtAppsHook
    # Provides the GSettings schemas GTK needs when Qt uses its GTK theme
    # (file dialogs); without them GLib aborts the app (D-033).
    wrapGAppsHook3
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
    qt6.qtimageformats
    wayland
    # The capture overlay as a layer surface above all windows (D-031).
    kdePackages.layer-shell-qt
    gtk3
    gsettings-desktop-schemas
  ];

  # One wrapper instead of two: the GTK variables go into Qt's wrapper.
  dontWrapGApps = true;
  preFixup = ''
    qtWrapperArgs+=("''${gappsWrapperArgs[@]}")
  '';

  meta = {
    description = "Screenshot tool for clean window captures, with an annotation editor";
    homepage = "https://github.com/michaelpfister/ch-nsl-snap";
    platforms = lib.platforms.linux;
    mainProgram = "nsl-snap";
  };
}
