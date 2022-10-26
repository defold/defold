#!/usr/bin/env bash

# The platform is just to be able to load it
. ./version.sh

# VERSION from build_luajit.h
readonly PATCH_FILE=patch_${VERSION}

ORIGINAL_REPO=git@github.com:LuaJIT/LuaJIT.git
CHANGED_REPO=git@github.com:JCash/LuaJIT.git

BRANCH_A=${SHA1}
BRANCH_B=v2.1-defold

DIR_A=repo_a
DIR_B=repo_b

if [ ! -d "${DIR_A}" ]; then
    echo "Cloning ${ORIGINAL_REPO} into ${DIR_A}"
    git clone ${ORIGINAL_REPO} ${DIR_A}
    (cd ${DIR_A} && git checkout ${BRANCH_A})
fi

if [ ! -d "${DIR_B}" ]; then
    echo "Cloning ${CHANGED_REPO} into ${DIR_B}"
    git clone ${CHANGED_REPO} ${DIR_B}
    (cd ${DIR_B} && git checkout ${BRANCH_B})
fi

echo "Creating patch..."

diff -ruw --exclude=".git" ${DIR_A} ${DIR_B} > ${PATCH_FILE}

echo "Wrote ${PATCH_FILE}"
