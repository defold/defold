#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

MULTI_ARCH="linux/amd64"
[[ -z "$NO_ARM64" ]] && MULTI_ARCH+=",linux/arm64"

docker buildx build --platform $MULTI_ARCH -t builder/ubuntu:22.04 -t builder/ubuntu:latest ${DIR}/
