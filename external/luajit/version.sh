#!/usr/bin/env bash
export SHA1=3e223cb8a41cff3b92931acb846af7b67a5d2537
export SHA1_SHORT=${SHA1:0:7}
export VERSION=2.1.0-${SHA1_SHORT}
export PRODUCT=luajit
export TARGET_FILE=${PRODUCT}-${VERSION}
