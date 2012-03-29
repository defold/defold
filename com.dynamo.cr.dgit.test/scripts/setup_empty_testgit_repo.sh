#!/bin/bash

set -e
rm -rf tmp/*repo*

function repo1() {
    mkdir -p tmp/source_repo
    pushd tmp/source_repo > /dev/null
    git init --bare
    popd > /dev/null

    git clone tmp/source_repo tmp/tmp_source_repo
    pushd tmp/tmp_source_repo > /dev/null

    (cat << EOF
test
EOF
) > "test"

    git add "test"
    git commit -m "Initial repo"
    git push origin master

    popd > /dev/null
}

repo1
