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

readonly NO_MAKE_INSTALL=1
readonly INSTALL_LIB_FILES="libgtest.a libgtest_main.a"
readonly INSTALL_INCLUDE_FILES="gtest/gtest-death-test.h gtest/gtest-param-test.h gtest/gtest-spi.h gtest/gtest-typed-test.h gtest/gtest_pred_impl.h gtest/gtest-message.h gtest/gtest-test-part.h gtest/gtest.h gtest/gtest_prod.h gtest/internal/gtest-death-test-internal.h gtest/internal/gtest-linked_ptr.h gtest/internal/gtest-param-util.h gtest/internal/gtest-tuple.h gtest/internal/gtest-filepath.h gtest/internal/gtest-param-util-generated.h gtest/internal/gtest-port.h gtest/internal/gtest-internal.h gtest/internal/gtest-string.h gtest/internal/gtest-type-util.h"

. ../common.sh

download
cmi $1
