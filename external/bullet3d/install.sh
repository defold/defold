#!/usr/bin/env bash

readonly BASE_URL=https://github.com/bulletphysics/bullet3/archive/refs/tags
readonly FILE_URL=3.25.tar.gz
readonly PRODUCT=bullet
readonly VERSION=3.25

. ../common.sh

function cmi_unpack() {
	echo cmi_unpack
    local archive_root=bullet3-${VERSION}
    local package_root=${PRODUCT}-${VERSION}
    mkdir -p ${package_root}
    tar xfz ../../download/$FILE_URL --strip-components=1 -C ${package_root} \
        ${archive_root}/VERSION \
        ${archive_root}/LICENSE.txt \
        ${archive_root}/AUTHORS.txt \
        ${archive_root}/src/BulletCollision \
        ${archive_root}/src/BulletDynamics \
        ${archive_root}/src/LinearMath \
        ${archive_root}/src/btBulletCollisionCommon.h \
        ${archive_root}/src/btBulletDynamicsCommon.h
}

cmi_install
