#!/usr/bin/env bash
set -eu

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"

package () {
	_PLATFORM="${1}"  # Platform
	_WND="${2}"       # Window Manager
	_ARCH="${3}"      # Architecture
	_JRE="${4}"       # Java Runtime Environment

	(
		_PRODUCT_DIR="${SCRIPT_PATH}/../target/products"
		_PACKAGE="Defold-${1}.${2}.${3}.zip"

		cd "${_PRODUCT_DIR}/com.dynamo.cr.editor.product/${_PLATFORM}/${_WND}/${_ARCH}"

		# Clean up previous artefacts
		[ -f "eclipsec.exe" ] && rm -f "eclipsec.exe"
		[ -f "Defold" ] && [ "${_PLATFORM}" == "macosx" ] && rm -f "Defold"
		[ -d "jre" ] && rm -rf "jre"
		[ -f "${_PRODUCT_DIR}/${_PACKAGE}" ] && rm -f "${_PRODUCT_DIR}/${_PACKAGE}"

		clear; ls; sleep 10

		# Construct package
		unzip -q "${SCRIPT_PATH}/../target/${_JRE}"
		clear; ls; sleep 10
		if [ "${_PLATFORM}" == "linux" ]; then
			if [ "${_ARCH}" == "x86_64" ]; then
				__XULRUNNER="xulrunner-1.9.2.13.en-US.linux-x86_64.tar.bz2"
				tar -xj "${SCRIPT_PATH}/../xulrunner/${__XULRUNNER}"
				clear; ls; sleep 10
				# Patch Defold.ini
			fi
		fi

		zip -r -y -q "${_PRODUCT_DIR}/${_PACKAGE}"
	)
}

#package macosx cocoa x86_64 jre-8u5-macosx-x64.zip &
#package linux gtk x86 jre-8u5-linux-i586.zip &
package linux gtk x86_64 jre-8u5-linux-x64.zip &
#package win32 win32 x86 jre-8u5-windows-i586.zip &

# wait for background jobs
wait