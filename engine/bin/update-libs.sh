#! /bin/bash
LIBS="dlib ddf particle glfw graphics hid input physics resource lua script render gameobject gui sound gamesys tools engine"

set -e
[ -z $DYNAMO_HOME ] && echo "DYNAMO_HOME not set" && exit 1

for lib in $LIBS; do
    echo "Building $lib"
    cd ../$lib

    waf configure --prefix=$DYNAMO_HOME $@
    waf clean

    tmp=`uname -s`
    if [ "MINGW" == ${tmp:0:5} ]; then
        # Avoid some windows related problem in waf
        # Still present?
        set +e
        waf
        set -e
    fi
    waf $@
    waf install $@
done
