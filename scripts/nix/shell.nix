{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    clang
    cmake
    freeglut
    git
    glib
    leiningen
    libGL
    libGLU
    libxext
    ninja
    openal
    openjdk21
    pkg-config
    protobuf
    xorg.libX11
    xorg.libXcursor
    xorg.libXi
    xorg.libXinerama
    xorg.libXrandr
    xorg.libXtst
    xorg.libXxf86vm

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
    export LD_LIBRARY_PATH=${
      pkgs.lib.makeLibraryPath [
        pkgs.freeglut
        pkgs.glib
        pkgs.libGL
        pkgs.libGLU
        pkgs.libxext
        pkgs.openal
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
