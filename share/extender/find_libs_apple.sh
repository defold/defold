
# use this on while debugging a docker container, to see what libs/frameworks are installed

SDK=${DYNAMO_HOME}/ext/SDKs/iPhoneOS11.2.sdk
#SDK=${DYNAMO_HOME}/ext/SDKs/MacOSX10.12.sdk


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
