#!/usr/bin/env bash
export SHA1=63c8f1a9b479ba5518cf6a170ff7729178bc3cfd
export SHA1_SHORT=${SHA1:0:7}
export VERSION=2.1.0-${SHA1_SHORT}
export PRODUCT=luajit
export TARGET_FILE=${PRODUCT}-${VERSION}
