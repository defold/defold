#!/bin/sh


if [[ $2 -eq "js-web" ]]; then
	export CXXFLAGS='-DGTEST_USE_OWN_TR1_TUPLE=1'
fi

readonly BASE_URL=http://googletest.googlecode.com/files
readonly FILE_URL=gtest-1.5.0.tar.gz
readonly PRODUCT=gtest
readonly VERSION=1.5.0

. ../common.sh

download
cmi $1
