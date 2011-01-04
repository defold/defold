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

#include <stdio.h>

int main()
{
    printf("Hello world");
}

EOF
) > main.cpp

    (cat << EOF

// temp
EOF
) > rename.cpp

    git add main.cpp
    git add rename.cpp
    git commit -m "Initial repo"
    git push origin master

    popd > /dev/null
}

repo1
