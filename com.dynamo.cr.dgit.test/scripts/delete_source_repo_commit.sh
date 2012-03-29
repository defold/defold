#!/bin/bash

set -e
pushd tmp/tmp_source_repo > /dev/null
git rm main.cpp
git commit -a -m "Deleted main.cpp"
git push
popd > /dev/null
