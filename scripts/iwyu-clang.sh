#!/usr/bin/env bash

IWYU=$(which include-what-you-use)

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

IWYU_CXX=$1
shift

set +e

$IWYU  -Xiwyu ${DIR}/iwyu.imp $@

set -e

$IWYU_CXX $@
