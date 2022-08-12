#!/usr/bin/env bash

export SHA1=633f265f67f322cbe2c5fd11d3e46d968ac220f7
export SHA1_SHORT=${SHA1:0:7}
export VERSION=2.1.0-${SHA1_SHORT}
export PRODUCT=luajit
export TARGET_FILE=${PRODUCT}-${VERSION}
