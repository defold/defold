
# use this on while debugging a docker container, to see what libs/frameworks are installed

#SDK=/opt/MacOSX.sdk
#SDK=/opt/iPhoneOS.sdk
SDK=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk
SDK=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk


echo Searching $SDK

LIBS=$(find $SDK/usr/lib/ -iname *.tbd)

echo -n "allowedLibs: ["
for f in $LIBS
do
	NAME=$(basename $f)
	NAME=${NAME/lib/}
	NAME=${NAME/.tbd/}
	NAME=${NAME//./\\\\.}
	NAME=${NAME//++/\\\\+\\\\+}
	echo -n \"${NAME}\",
done

FRAMEWORKS=$(ls $SDK/System/Library/Frameworks)
for f in $FRAMEWORKS
do
	if [ ${f: -10} == ".framework" ]
	then
		NAME=$(basename $f)
		NAME=${NAME//.framework/}
		NAME=${NAME//./\\\\.}
		NAME=${NAME//++/\\\\+\\\\+}
		echo -n \"${NAME}\",
	fi
done

echo "]"
