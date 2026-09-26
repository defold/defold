# GLFW 2.7.1

Build a package from the repository root, with the platform SDK set up:

```sh
./scripts/build.py shell
./scripts/build.py build_external --package=glfw --platform=arm64-android
```

CMake writes `packages/glfw-2.7.1-<platform>.tar.gz`. Android packages contain
`libdmglfw`, `libdmglfw_vulkan`, and `share/java/glfw_android.jar`. Web packages
contain `libdmglfw` and `lib/<platform>/js/library_glfw.js`. Each platform archive
also includes the GLFW headers.

The iOS source build runs automatically during `install_ext`. Desktop engine
packages use GLFW 3.4, built separately under `share/ext/glfw`.
