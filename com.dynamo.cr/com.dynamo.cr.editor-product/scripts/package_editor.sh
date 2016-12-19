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
    [ -d "xulrunner" ] && rm -rf "xulrunner"
    [ -f "Defold-linux.sh" ] && rm -f "Defold-linux.sh"
    [ -f "${_PRODUCT_DIR}/${_PACKAGE}" ] && rm -f "${_PRODUCT_DIR}/${_PACKAGE}"

    # Construct package
    unzip -q "${SCRIPT_PATH}/../target/${_JRE}"
    if [ "${_PLATFORM}" == "linux" ]; then
      # Include the Defold-linux.sh launch script in the package for Linux
      cp "${SCRIPT_PATH}/Defold-linux.sh" .

      if [ "${_ARCH}" == "x86_64" ]; then
      	# Include XULRunner for 64 bit Linux
        __XULRUNNER="xulrunner-1.9.2.13.en-US.linux-x86_64.tar.bz2"
        tar -xjf "${SCRIPT_PATH}/../xulrunner/${__XULRUNNER}"

        if ! grep "XULRunnerPath" "Defold.ini" > /dev/null 2>&1; then
          _DEFAULT_BROWSER="-Dorg.eclipse.swt.browser.DefaultType=mozilla"
          _ARG_KEY="-Dorg.eclipse.swt.browser.XULRunnerPath"
          _ARG_VAL="xulrunner"
          echo "${_DEFAULT_BROWSER}" >> "Defold.ini"
          echo "${_ARG_KEY}=${_ARG_VAL}" >> "Defold.ini"
        fi
      fi
    fi

    zip -r -y -q "${_PRODUCT_DIR}/${_PACKAGE}" .

    # Clean up current artefacts
    if [ -f "Defold.ini" ]; then
      sed -i '/XULRunnerPath/d' "Defold.ini"
    fi
  )
}

package macosx cocoa x86_64 jre-8u5-macosx-x64.zip &
package linux gtk x86 jre-8u5-linux-i586.zip &
package linux gtk x86_64 jre-8u5-linux-x64.zip &
package win32 win32 x86 jre-8u5-windows-i586.zip &
package win32 win32 x86_64 jre-8u5-windows-x64.zip &

# wait for background jobs
wait
