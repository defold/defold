#!/bin/bash
set -e

package () {
	pushd target/products/com.dynamo.cr.editor.product/${1}/${2}/${3} > /dev/null
	rm -f eclipsec.exe
	[ "$1" == "macosx" ] && rm -f Defold
	rm -rf jre
	echo "Unzipping ${4}"
	unzip -q ../../../../../${4}
	rm -f ../../../../Defold-${1}.${2}.${3}.zip
	echo "Zipping Defold-${1}.${2}.${3}.zip"
	zip -r -y -q ../../../../Defold-${1}.${2}.${3}.zip .
	popd > /dev/null
}

package macosx cocoa x86_64 jre-8u5-macosx-x64.zip &
package linux gtk x86 jre-8u5-linux-i586.zip &
package linux gtk x86_64 jre-8u5-linux-x64.zip &
package win32 win32 x86 jre-8u5-windows-i586.zip &

# wait for background jobs
wait
