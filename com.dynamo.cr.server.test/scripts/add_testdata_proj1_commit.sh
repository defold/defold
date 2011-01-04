#!/bin/bash

set -e
pushd tmp/test_data/proj1/content > /dev/null
echo 'testing' >> file1.txt
git commit -a -m "Added testing to file1.txt"
popd > /dev/null
