{ lib
, stdenv
, cmake
, ninja
, pkg-config
, qt6
, kdePackages
, wayland
, wayland-scanner
, wlr-protocols
, libevdev
}:

stdenv.mkDerivation {
  pname = "home-theatre-launcher";
  version = "0.1.0";

  src = lib.cleanSourceWith {
    src = lib.cleanSource ../launcher;
    filter = path: _: baseNameOf path != "build";
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    wayland-scanner
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtsvg
    qt6.qtwayland
    kdePackages.layer-shell-qt
    wayland
    wlr-protocols
    libevdev
  ];

  cmakeFlags = [
    "-DWLR_PROTOCOLS_DIR=${wlr-protocols}/share/wlr-protocols"
  ];

  doCheck = true;
  preCheck = ''
    export QT_QPA_PLATFORM=offscreen
  '';

  meta = {
    description = "Minimal TV launcher for any configured apps (Moonlight, Waydroid, …)";
    mainProgram = "home-theatre-launcher";
    platforms = lib.platforms.linux;
  };
}
