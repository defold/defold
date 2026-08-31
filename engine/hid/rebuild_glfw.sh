#!/usr/bin/env bash

(cd $DEFOLD_HOME/external/glfw && PREFIX=$DYNAMO_HOME waf configure --platform=arm64-android && waf install)

(cd ${DYNAMO_HOME}/ext && tar xvf ${DEFOLD_HOME}/packages/glfw-2.7.1-arm64-android.tar.gz)

rm ./build/src/test/test_app_hid
