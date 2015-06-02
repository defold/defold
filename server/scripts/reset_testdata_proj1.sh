#!/bin/bash

set -e
pushd tmp/test_data/$1/content > /dev/null
git reset --hard
popd > /dev/null
