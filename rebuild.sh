#!/usr/bin/env bash

set -e

(cd engine/engine && rm -v -rf build/src/dmengine build/src/engine* build/src/libengine*.a)
(cd engine/dlib && waf --opt-level=0 --with-asan --target=profile_defold --skip-tests install)
(cd engine/engine && waf --with-asan --opt-level=0 --target=dmengine --skip-tests -v)
