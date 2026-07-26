#!/usr/bin/env bash

#rm -rf $DEFOLD_HOME/share/ext/glfw/source
(cd $DEFOLD_HOME/share/ext/glfw && ./build_glfw.sh arm64-macos)

# cp -v ${DEFOLD_HOME}/share/ext/glfw/glfw-3.4-arm64-macos.tar.gz ${DEFOLD_HOME}/packages

# (cd ${DYNAMO_HOME}/ext && tar xvf ${DEFOLD_HOME}/packages/glfw-3.4-arm64-macos.tar.gz)

cp -v ../../share/ext/glfw/source/src/libglfw3.a ../../tmp/dynamo_home/ext/lib/arm64-macos/libglfw3.a

rm ./build/src/test/test_app_hid

waf --opt-level=0 --skip-tests
