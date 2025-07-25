On Linux, install the development packages for FreeType, Cairo, and GLib. For
example, on Ubuntu / Debian, you would do:

    $ sudo apt-get install meson pkg-config ragel gtk-doc-tools gcc g++ libfreetype6-dev libglib2.0-dev libcairo2-dev

whereas on Fedora, RHEL, CentOS, and other Red Hat based systems you would do:

    $ sudo dnf install meson pkgconfig gtk-doc gcc gcc-c++ freetype-devel glib2-devel cairo-devel

and on ArchLinux and Manjaro:

    $ sudo pacman -Suy meson pkg-config ragel gcc freetype2 glib2 glib2-devel cairo

On macOS:

    brew install pkg-config ragel gtk-doc freetype glib cairo meson

Then use meson to build the project and run the tests, like:

    meson build && ninja -Cbuild && meson test -Cbuild

On Windows, meson can build the project like above if a working MSVC's cl.exe
(`vcvarsall.bat`) or gcc/clang is already on your path, and if you use
something like `meson build --wrap-mode=default` it fetches and compiles most
of the dependencies also.  It is recommended to install CMake either manually
or via the Visual Studio installer when building with MSVC, using meson.

Our CI configurations are also a good source of learning how to build HarfBuzz.

There is also amalgamated source provided with HarfBuzz which reduces whole process
of building HarfBuzz to `g++ src/harfbuzz.cc -fno-exceptions` but there is
no guarantee provided with buildability and reliability of features you get.
