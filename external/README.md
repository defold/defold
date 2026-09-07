# External

`./scripts/build.py build_ext` builds source dependencies (currently Bullet)
with the regular Defold CMake toolchain and installs them into
`tmp/dynamo_home/ext`. Run it after `install_ext`, before the first engine
build, and whenever these sources or the toolchain change. Use `--platform`
for cross-compilation. Repeated calls reuse the CMake build directory.
`distclean` removes these build caches as well as the installed SDK.

The other external libraries are distributed as packages. Rebuild those with
`build_external`, which writes archives under `defold/packages`.

# Modifications

Always keep the original code separate from the modified code, so that it's easy to reason about and update.

Keep any engine changes in a `.patch` file.
