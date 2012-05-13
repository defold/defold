#!/bin/bash
set -e
rm -rf tmp/*repo*

DIR=$(cd $(dirname "$0"); pwd)

setup_proj() {
    rm -rf tmp/test_data
    mkdir -p tmp/test_data/$2
    cp -r $DIR/../test_data/$1/* tmp/test_data/$2

    pushd tmp/test_data/$2 > /dev/null
    git init
    git add .
    git commit -m initial
    git config receive.denycurrentbranch ignore
    git config http.receivepack true
    popd > /dev/null
}

setup_proj proj1 $1

