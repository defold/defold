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
    export DEFOLD_HOME=${builtins.toString ../../.}
    export DYNAMO_HOME=$DEFOLD_HOME/tmp/dynamo_home
    export LD_LIBRARY_PATH=${
      pkgs.lib.makeLibraryPath [
        pkgs.freeglut
        pkgs.glib
        pkgs.libGL
        pkgs.libGLU
        pkgs.openal
        pkgs.xorg.libX11
        pkgs.xorg.libXcursor
        pkgs.xorg.libXi
        pkgs.xorg.libXinerama
        pkgs.xorg.libXrandr
        pkgs.xorg.libXtst
        pkgs.xorg.libXxf86vm
      ]
    }:$DYNAMO_HOME/lib/x86_64-linux:$DYNAMO_HOME/ext/lib/x86_64-linux:$LD_LIBRARY_PATH
    export PYTHONPATH=${pkgs.python313}/lib/python3.13/site-packages:$DYNAMO_HOME/lib/python:$DEFOLD_HOME/build_tools:$DYNAMO_HOME/ext/lib/python
  '';
}
