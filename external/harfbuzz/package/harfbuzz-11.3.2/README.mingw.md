Most HarfBuzz developers do so on Linux or macOS. However, HarfBuzz is a
cross-platform library and it is important to ensure that it works on Windows
as well. In particular, we use this workflow to develop and test the HarfBuzz
Uniscribe shaper and DirectWrite shaper and font backend, all from Linux or
macOS.

This document provides instructions for cross-compiling HarfBuzz on Linux or
macOS, for Windows, using the MinGW toolchain, and running tests and utilties
under Wine.

We then discuss using native Windows Uniscribe or DirectWrite DLLs, which
allows you to test HarfBuzz's shaping against the Microsoft shaping engines
instead of those provided by Wine.

This document assumes that you are familiar with building HarfBuzz on Linux or
macOS.

You can build for 32bit or 64bit Windows. If your intention is to use a native
Uniscribe usp10.dll from Windows 7 or before, you would need to build for 32bit.
If you want to use a native DirectWrite DLL from Windows 10 or later, you would
need to build for 64bit.

We suggest you read to the end of this document before starting, as it provides
a few different ways to build and test HarfBuzz for Windows.

1. Install Wine.

  - Fedora: `dnf install wine`.
  - Ubuntu, 32bit: `apt install wine wine32`.
  - Ubuntu, 64bit: `apt install wine wine64`.
  - Mac: `brew install wine-stable`.

Note that to run Wine on Apple silicon systems, you need the Apple Rosetta translator.
Follow the instructions you got from brew. This should do it:

  - `softwareupdate --install-rosetta --agree-to-license`

2. Install the `mingw-w64` cross-compiler.

  - Fedora, 32bit: `dnf install mingw32-gcc-c++`
  - Fedora, 64bit: `dnf install mingw64-gcc-c++`
  - Ubuntu, 32bit: `apt install g++-mingw-w64-i686`
  - Ubuntu, 64bit: `apt install g++-mingw-w64-x86-64`
  - Mac: `brew install mingw-w64`

3. Install dependencies.

First, make sure you do not have the mingw32 harfbuzz package, as that will
override our own build with older `meson`:

  - Fedora, 32bit: `dnf remove mingw32-harfbuzz`
  - Fedora, 64bit: `dnf remove mingw64-harfbuzz`

Then install the actual dependencies:

  - Fedora, 32bit: `dnf install mingw32-glib2 mingw32-cairo mingw32-freetype`
  - Fedora, 64bit: `dnf install mingw64-glib2 mingw64-cairo mingw64-freetype`

If you cannot find these packages for your distribution, or you are on macOS,
you can skip to the next step, as meson will automatically download and build
the dependencies for you.

4. If you are familiar with `meson`, you can use the cross-compile files we
provide to find your way around. But we do not recommend this way. Read until
the end of this section before deciding which one to use.

  - 32bit: `meson --cross-file=.ci/win32-cross-file.txt build-win -Dglib-enabled -Dcairo=enabled -Dgdi=enabled -Ddirectwrite=enabled`
  - 64bit: `meson --cross-file=.ci/win64-cross-file.txt build-win -Dglib-enabled -Dcairo=enabled -Dgdi=enabled -Ddirectwrite=enabled`

In which case, you will proceed to run `ninja` as usual to build:

  - `ninja -C build-win`

Or you can simply invoke the scripts we provide for our Continuous Integration
system, to configure and build HarfBuzz for you. This is the easiest way to
build HarfBuzz for Windows and how we build our Windows binaries:

  - 32bit: `./.ci/build-win.sh 32 && ln -s build-win32 build-win`
  - 64bit: `./.ci/build-win.sh 64 && ln -s build-win64 build-win`

This might take a while, since, if you do not have the dependencies installed,
meson will download and build them for you.

5. If everything succeeds, you should have the `hb-shape.exe`, `hb-view.exe`,
`hb-subset.exe`, and `hb-info.exe` executables in `build-win/util`.

6. Configure your wine to find system mingw libraries. While there, set it also
to find the built HarfBuzz DLLs:

  - Fedora, 32bit: `export WINEPATH="$HOME/harfbuzz/build-win/src;/usr/i686-w64-mingw32/sys-root/mingw/bin"`
  - Fedora, 64bit: `export WINEPATH="$HOME/harfbuzz/build-win/src;/usr/x86_64-w64-mingw32/sys-root/mingw/bin"`
  - Other systems: `export WINEPATH="$HOME/harfbuzz/build-win/src"`

Adjust for the path where you have built HarfBuzz.  You might want to add this
to your `.bashrc` or `.zshrc` file.

Alternatively, can skip this step if commands are run through the `meson devenv`
command, which we will introduce in the next step. I personally find it more
convenient to set the `WINEPATH` variable, as it allows me to run the executables
directly from the shell.

7. Run the `hb-shape` executable under Wine:

  - `wine build-win/util/hb-shape.exe perf/fonts/Roboto-Regular.ttf Test`

Or using `meson devenv to do the same:

  - `meson devenv -C build-win util/hb-shape.exe $PWD/perf/fonts/Roboto-Regular.ttf Test`

You probably will get lots of Wine warnings, but if all works fine, you
should see:
```
[gid57=0+1123|gid74=1+1086|gid88=2+1057|gid89=3+670]
```

You can make Wine less verbose, without hiding all errors, by setting:

  - `export WINEDEBUG=fixme-all,warn-all,err-plugplay,err-seh,err-rpc,err-ntoskrnl,err-winediag,err-systray,err-hid`

Add this to your `.bashrc` or `.zshrc` file as well.

Next, let's try some non-Latin text. Unfortunately, the command-line parsing of
our cross-compiled glib is not quite Unicode-aware, at least when run under
Wine. So you will need to find some other way to feed Unicode text to the
shaper. There are three different ways you can try:

  - `echo حرف | wine build-win/util/hb-shape.exe perf/fonts/Amiri-Regular.ttf`
  - `wine build-win/util/hb-shape.exe perf/fonts/Amiri-Regular.ttf -u 062D,0631,0641`
  - `wine build-win/util/hb-shape.exe perf/fonts/Amiri-Regular.ttf --text-file harf.txt`

To get the Unicode codepoints for a string, you can use the `hb-unicode-decode`
utility:
```
$ test/shape/hb-unicode-decode حرف
U+062D,U+0631,U+0641
```

8. Next, let's try the `hb-view` utility. By default, `hb-view` outputs ANSI text,
which Wine will not display correctly. You can use the `-o` option to redirect the
output to a file, or just redirect the output using the shell, which will produce
a PNG file.

  - `wine build-win/util/hb-view.exe perf/fonts/Roboto-Regular.ttf Test > test.png`

7. As noted, if your Linux has `binfmt_misc` enabled, you can run the executables
directly. If not, you can modify the cross-file to use the `exe_wrapper` option as
specified before.

  - `build-win/util/hb-shape.exe perf/fonts/Roboto-Regular.ttf Test`

If that does not work, you can use the `wine` command as shown above.

10. You can try running the test suite. If on Linux with `binfmt_misc` enabled, you
can run the tests directly:

  - `ninja -C build-win test`

For other situations, use `meson devenv`:

  - `meson devenv -C build-win ninja test`

This might take a couple of minutes to run. Running under Wine is expensive, so
be patient.

If all goes well, tests should run. If all is well, you should probably see about
400 tests pass, some skipped, but none failing.

11. In the above testing situation, the `directwrite` test will be disabled
automatically upon detection of running under Wine. The reason the `directwrite`
test would otherwise fails is that we are running against the Wine-provided
DirectWrite DLL, which is an incomplete reimplementation of the DirectWrite API
by Wine, and not the real thing.

If you want to test the Uniscribe or DirectWrite shapers against the real
Uniscribe / DirectWrite, you can follow the instructions below.

11. Old Uniscribe: Assuming a 32bit build for now.

Bring a 32bit version of `usp10.dll` for yourself from
`C:\Windows\SysWOW64\usp10.dll` of your 64bit Windows installation,
or `C:\Windows\System32\usp10.dll` for 32bit Windows installation.

You want one from Windows 7 or earlier.  One that is not just a proxy for
`TextShaping.dll`.  Rule of thumb, your `usp10.dll` should have a size more
than 500kb.

Put the file in `~/.wine/drive_c/windows/syswow64/` so wine can find it.

You can now tell wine to use the native `usp10.dll`:

  - `export WINEDLLOVERRIDES="usp10=n"`
  - `wine build-win/util/hb-shape.exe perf/fonts/Roboto-Regular.ttf Test --shaper=uniscribe`

12. DirectWrite and new Uniscribe: You can use the same method to test the
DirectWrite shaper against the native DirectWrite DLL. Try with a 64bit build
this time.

Bring `TextShaping.dll`, `DWrite.dll`, and `usp10.dll` from your 64bit Windows
installation (`C:\Windows\System32`) to `~/.wine/drive_c/windows/system32/`.

You want the ones from Windows 10 or later. You might have some luck downloading
them from the internet, but be careful with the source. I had success with the
DLLs from [https://dllme.com](dllme.com), but I cannot vouch for the site.

You can now tell wine to use the native DirectWrite:

  - `export WINEDLLOVERRIDES="textshaping,dwrite,usp10=n"`
  - `wine build-win/util/hb-shape.exe perf/fonts/Roboto-Regular.ttf Test --shaper=directwrite`

If all works well, you should be able to rerun the tests and see all pass this time.

13. For some old instructions on how to test HarfBuzz's native Indic shaper against
Uniscribe, see: https://github.com/harfbuzz/harfbuzz/issues/3671

14. That's it! If you made it this far, you are now able to develop and test
HarfBuzz on Windows, from Linux or macOS. Enjoy!
