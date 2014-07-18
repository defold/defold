# NOTE: This script is only used for CI
# The corresponding file for development is build.xml

set -e
mkdir -p lib

SHA1=`git log --pretty=%H -n1`

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/share/builtins.zip lib/builtins.zip
