#!/usr/bin/env bash
set -eu

# ----------------------------------------------------------------------------
# Environment
# ----------------------------------------------------------------------------
SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")"; pwd)"
CONFIG_PATH="${SCRIPT_PATH}/Defold.ini"

# ----------------------------------------------------------------------------
# Setup
# ----------------------------------------------------------------------------
trap 'terminate 1 "An unexpected error occurred."' SIGINT SIGTERM EXIT
function terminate() {
	_MESSAGE="${2:-Terminated.}"
	_RETCODE="${1:-1}"

	trap - SIGINT SIGTERM EXIT
	echo "${_MESSAGE}"
	exit ${_RETCODE}
}

# ----------------------------------------------------------------------------
# Configuration
# ----------------------------------------------------------------------------
if [ -f "${CONFIG_PATH}" ]; then

  # --------------------------------------------------------------------------
  # Configure absolute path for XULRunner
  # --------------------------------------------------------------------------
  if grep "XULRunnerPath" "${CONFIG_PATH}" > /dev/null 2>&1; then
    _SEARCH='^(-Dorg.eclipse.swt.browser.XULRunnerPath=).*?$'
    _REPLACE="\\1=${SCRIPT_PATH}/xulrunner"
    sed -ir "s#${_SEARCH}#${_REPLACE}#g" "${CONFIG_PATH}"
    if ! grep "XULRunnerPath" "${CONFIG_PATH}" > /dev/null 2>&1; then
      terminate 1 "[ERROR] Failed to configure XULRunnerPath!"
    fi
  fi
else
  terminate 1 "[ERROR] Configuration could not identify the environment!"
fi

# ----------------------------------------------------------------------------
# Teardown
# ----------------------------------------------------------------------------
terminate 0 "[SUCCESS] Configuration complete!"