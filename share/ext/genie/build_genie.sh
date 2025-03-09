#!/usr/bin/env bash
# Copyright 2020-2025 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

set -e

readonly REPO_URL=https://github.com/bkaradzic/genie

readonly PRODUCT=genie

PLATFORM=$1

. ../common.sh

function cmi_download() {
    echo "cmi_download -- no package to download"
}

function cmi_unpack() {
    echo "cmi_unpack -- Cloning repo"
    git clone --depth 1 ${REPO_URL} .
}

function cmi_configure() {
    echo "cmi_configure -- no project to configure"
}

function run() {
    echo "CC: $*"
    $*
}

function cmi_make() {
    # The version is from our own fork but that's ok I think
    export VERSION=$(git rev-parse --short HEAD)
    make -j8
}

function cmi_package_platform() {
    local platform=$1
    local TGZ="${PRODUCT}-${VERSION}-${platform}.tar.gz"
    mkdir -p ${PREFIX}/bin/${platform}

    if [ "$(uname)" == "Darwin" ]; then
        cp -v ./tmp/bin/darwin/genie ${PREFIX}/bin/${platform}
    fi

    pushd ${PREFIX}  >/dev/null
        tar cfvz $TGZ bin
    popd >/dev/null

    [ ! -d ../build ] && mkdir ../build
    mv -v $PREFIX/$TGZ ../build
    echo "../build/$TGZ created"
}

cmi $1
