#!/bin/sh


if [[ $2 -eq "js-web" ]]; then
	export CXXFLAGS='-DGTEST_USE_OWN_TR1_TUPLE=1'
fi

readonly BASE_URL=https://github.com/google/googletest/archive
readonly FILE_URL=release-1.5.0.tar.gz
readonly PRODUCT=gtest
readonly VERSION=1.5.0

if [[ $1 == *darwin* ]] ; then
    # tr1/tuple isn't available on clang/darwin and gtest 1.5.0 assumes that
    # see corresponding flag in waf_dynamo.py
    export CXXFLAGS='-DGTEST_USE_OWN_TR1_TUPLE=1'
fi

. ../common.sh

download
cmi $1
