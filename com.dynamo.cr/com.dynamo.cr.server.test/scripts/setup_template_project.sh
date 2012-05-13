#!/bin/bash
set -e
rm -rf tmp/*repo*

DIR=$(cd $(dirname "$0"); pwd)

setup_proj() {
    rm -rf tmp/templates
    mkdir -p tmp/templates/$1
    cp -r $DIR/../test_data/$1/* tmp/templates/$1/

    pushd tmp/templates/$1 > /dev/null
    git init
    git add .
    git commit -m initial
    git config receive.denycurrentbranch ignore
    popd > /dev/null
}

rm -rf tmp/test_data
mkdir -p tmp/test_data
setup_proj $1

