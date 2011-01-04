#!/bin/bash

set -e
pushd tmp/tmp_source_repo > /dev/null
echo '#include <stdint.h>' >> main.cpp
git commit -a -m "Added stdint.h"
git push
popd > /dev/null
