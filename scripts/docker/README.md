# Docker

This Docker container let's you build Defold in Linux environment targeting different platforms through Clang

Currently supported platforms:

* Linux 64 bit

# Installation

* Download and install Docker from [www.docker.com](www.docker.com)
* Build the container

  defold$ ./scripts/docker/build.sh

# Run

* Run the container (wit or withhout path specified)

  defold$ ./scripts/docker/run.sh [./path/to/folder]

and it will mount that folder into the container, and you can start typing right away.


# FAQ

## How do I get another prompt into the running container?

* Run the debug script

  defold$ ./scripts/docker/run.sh

