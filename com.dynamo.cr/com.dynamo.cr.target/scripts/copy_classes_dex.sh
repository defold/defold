set -e
SHA1=`git log --pretty=%H -n1`

rm -rf res
mkdir -p res
cp -r $DYNAMO_HOME/ext/share/java/res/* res/

cp -v $DYNAMO_HOME/archive/${SHA1}/engine/armv7-android/classes.dex lib/classes.dex
