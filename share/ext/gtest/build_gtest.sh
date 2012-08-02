#!/bin/sh

readonly BASE_URL=http://googletest.googlecode.com/files
readonly FILE_URL=gtest-1.5.0.tar.gz
readonly PRODUCT=gtest
readonly VERSION=1.5.0

. ../common.sh

download
cmi $1
