#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
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



readonly PRODUCT=jctest
readonly VERSION=0.12
readonly BASE_URL=https://github.com/JCash/jctest/archive/refs/tags
readonly FILE_URL=v${VERSION}.tar.gz

export CONF_TARGET=$1

. ../common.sh

function cmi_configure() {
    echo "skipping cmi_configure"
}

function cmi_make() {
    echo "cmi_make"
    local include=${PREFIX}/include/

    mkdir ${PREFIX}/include/
    mkdir ${PREFIX}/include/jc_test
    mkdir ${PREFIX}/share
    mkdir ${PREFIX}/bin
    mkdir ${PREFIX}/lib

    pwd
    cp -v src/jc_test.h ${PREFIX}/include/jc_test
}

function cmi_package_platform() {
    echo "skipping cmi_package_platform"
}

download
cmi $1
