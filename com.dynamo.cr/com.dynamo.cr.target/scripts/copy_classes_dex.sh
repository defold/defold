set -e
SHA1=`git log --oneline | head -1 | awk '{ print $1 }'`

scp builder@ci-master.defold.com:builds/armv7-android/classes.dex.${SHA1} lib/classes.dex
