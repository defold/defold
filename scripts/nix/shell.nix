{
  pkgs ? import <nixpkgs> { },
}:

let
  jdk = pkgs.openjdk25;
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    wayland-protocols
    wayland-scanner
  ];

  buildInputs = with pkgs; [
    clang
    cmake
    git
    glib
    libdecor
    libGL
    libxext
    libxkbcommon
    ninja
    openal
    jdk
    pkg-config
    wayland
    xorg.libX11
    xorg.libXcursor
    xorg.libXi
    xorg.libXinerama
    xorg.libXrandr
    xorg.libXtst
    xorg.libXxf86vm

    (leiningen.override { jdk = jdk; })
    (python313.withPackages (
      ps: with ps; [
        pip
        virtualenv
      ]
    ))
  ];

  shellHook = ''
    export CC=clang
    export CXX=clang++
    export JAVA_HOME="${jdk.home}"
    export LD_LIBRARY_PATH=${
      pkgs.lib.makeLibraryPath [
        pkgs.glib
        pkgs.libdecor
        pkgs.libGL
        pkgs.libxext
        pkgs.libxkbcommon
        pkgs.openal
        pkgs.wayland
        pkgs.xorg.libX11
        pkgs.xorg.libXcursor
        pkgs.xorg.libXi
        pkgs.xorg.libXinerama
        pkgs.xorg.libXrandr
        pkgs.xorg.libXtst
        pkgs.xorg.libXxf86vm
      ]
    }:$LD_LIBRARY_PATH
  '';
}
