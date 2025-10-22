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

# FAQ

## How do I get another prompt into the running container?

* Run the debug script

```sh
  defold$ ./scripts/docker/run.sh
```
