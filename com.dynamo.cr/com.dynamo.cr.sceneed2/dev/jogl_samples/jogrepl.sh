#! /bin/sh

BASE=../../..
JOGL=./jogl-all.jar:./gluegen-rt.jar
CLOJURE=./clojure.jar

if [ ! -f "jogl-all.jar" ]
then
    echo "Download the latest jogl-all.jar from www.jogamp.com"
    exit=1
fi

if [ ! -f "gluegen-rt.jar" ]
then
    echo "Download the latest gluegen-rt.jar from www.jogamp.com"
    exit=1
fi

if [[ $exit ]]
then
    exit $exit
fi

case `uname` in
    Darwin) native="osx";;
    Linux)  native="linux";;
esac

libs=$BASE/javax.media.opengl/lib/$native

if [ -z "$1" ]
then
    SCRIPT="-r"
else
    SCRIPT="$@"
fi

if [ -x `which rlwrap` ]
then
    RL=`which rlwrap`
else
    RL=""
fi

$RL java -classpath .:$JOGL:$CLOJURE clojure.main $SCRIPT
