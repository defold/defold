#!/usr/bin/env bash

set -e

(cd ../dlib && waf install --opt-level=0)
waf clean
waf --opt-level=0 --skip-tests
./build/src/test/test_liveupdate_job_single

