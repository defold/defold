set -e
SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

rm -rf res
mkdir -p res
cp -r $DYNAMO_HOME/ext/share/java/res/* res/

scp builder@ci-master.defold.com:builds/armv7-android/classes.dex.${SHA1} lib/classes.dex
