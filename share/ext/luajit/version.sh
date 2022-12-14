#!/usr/bin/env bash

export SHA1=6c4826f12c4d33b8b978004bc681eb1eef2be977
export SHA1_SHORT=${SHA1:0:7}
export VERSION=2.1.0-${SHA1_SHORT}
export PRODUCT=luajit
export TARGET_FILE=${PRODUCT}-${VERSION}
