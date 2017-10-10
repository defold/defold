#!/usr/bin/env bash


if [[ $2 -eq "js-web" ]]; then
	export CXXFLAGS='-DGTEST_USE_OWN_TR1_TUPLE=1'
fi

readonly BASE_URL=https://github.com/google/googletest/archive
readonly FILE_URL=release-1.8.0.tar.gz
readonly PRODUCT=gtest
readonly VERSION=1.8.0

if [[ $1 == *darwin* ]] ; then
    # tr1/tuple isn't available on clang/darwin and gtest 1.5.0 assumes that
    # see corresponding flag in waf_dynamo.py
    export CXXFLAGS='-DGTEST_USE_OWN_TR1_TUPLE=1'
fi

export CONF_TARGET=$1

. ../common.sh

function cmi_configure() {
	echo "No configure exists"
}


case $CONF_TARGET in
	*win32)
		# visual studio projects can be built with cmake: https://stackoverflow.com/a/28370892
		function cmi_make() {
			set +e
			pushd googletest
			if [ "$CONF_TARGET" == "win32" ]; then
                ARCH=""
            else
                ARCH=" Win64"
			fi
            echo "TARGET" $CONF_TARGET "->" ${ARCH}
			cmake -G "Visual Studio 14 2015${ARCH}" .
			cmake  --build . --config Release

			# "install"
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/share/$CONF_TARGET
			mkdir -p $PREFIX/include/

			cp -v ./Release/*.lib $PREFIX/lib/$CONF_TARGET/
			cp -v ./gtest.dir/Release/gtest.pdb $PREFIX/lib/$CONF_TARGET/
			cp -v ./gtest_main.dir/Release/gtest_main.pdb $PREFIX/lib/$CONF_TARGET/
			popd
			set +e
		}
		;;
	*)
		function cmi_make() {
			set -e
		    pushd googletest/make
		    make -j8 gtest.a
		    make -j8 gtest_main.a
			set +e

			# "install"
			mkdir -p $PREFIX/lib/$CONF_TARGET
			mkdir -p $PREFIX/bin/$CONF_TARGET
			mkdir -p $PREFIX/share/$CONF_TARGET
			mkdir -p $PREFIX/include/
			cp -v gtest_main.lib $PREFIX/lib/$CONF_TARGET/libgtest_main.lib
			cp -v gtest.a $PREFIX/lib/$CONF_TARGET/libgtest.a
			cp -v gtest_main.a $PREFIX/lib/$CONF_TARGET/libgtest_main.a
			cp -v -r ../include/ $PREFIX/include/

		    popd
			set +e
		}
		;;
esac


download
cmi $1
