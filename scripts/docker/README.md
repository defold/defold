# Docker

This Docker container let's you build Defold in Linux environment targeting different platforms through Clang.

Currently supported platforms:

* Linux x86_64
* Linux arm64

# Installation

* Download and install Docker from [www.docker.com](www.docker.com)
* Build the container

```sh
  defold$ ./scripts/docker/build.sh
```

It builds multi-platform container (for x86_64 and arm64). If you need only x86_64 version you can run script with `NO_ARM64`:

```sh
  defold$ NO_ARM64=1 ./scripts/docker/build.sh
```

# Run

* Run the container (with or without path specified)

```sh
  defold$ ./scripts/docker/run.sh [./path/to/folder]
```

and it will mount that folder into the container, and you can start typing right away. If you want run Docker container for platform other than host platform - pass variable `DM_DOCKER_BUILD_PLATFORM`

```sh
  defold$ DM_DOCKER_BUILD_PLATFORM=linux/amd64 ./scripts/docker/run.sh [./path/to/folder]
```

Possible values: `linux/amd64`, `linux/arm64`.

## Linux window tests

Build the image and open a shell for either `linux/amd64` or `linux/arm64` using the commands above.
From the mounted repository, install the engine dependencies and enter the Defold environment.

```sh
./scripts/build.py install_ext
./scripts/build.py shell
```

Configure the engine and run the same window tests as native Linux CI.

```sh
case "$(uname -m)" in
  x86_64) platform=x86_64-linux ;;
  aarch64) platform=arm64-linux ;;
esac
cmake -S . -B "engine/build/$platform" -DTARGET_PLATFORM="$platform" -DBUILD_TESTS=ON
cmake --build "engine/build/$platform" --target run_linux_window_tests
```

The runner starts Xvfb with Openbox and a private Weston compositor. Mesa software rendering exercises GLX or EGL and Vulkan presentation. A second Weston compositor receives keyboard and pointer input from XTest. The gamepad test replays evdev events through a temporary device fixture. The runner needs neither a host display socket nor privileged device access.

The engine tries X11 first. On a Wayland desktop with XWayland available, launch a game with `env -u DISPLAY ./dmengine` to exercise native Wayland.

Before release, exercise the same build on a physical Linux desktop for GPU drivers, mixed-DPI displays, fullscreen transitions, clipboard integration and USB/Bluetooth controllers.

# FAQ

## How do I get another prompt into the running container?

* Run the debug script

```sh
  defold$ ./scripts/docker/run.sh
```
