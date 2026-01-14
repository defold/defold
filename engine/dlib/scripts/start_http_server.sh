#!/usr/bin/env bash

# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

set -e

SCRIPTDIR=$(dirname "$0")

PATHSEP=":"
if [ "${OS}" == "Windows_NT" ]; then
	if [ "${TERM}" == "cygwin" ]; then
		PATHSEP=":"
	else
		PATHSEP=";"
	fi
fi

CLASSPATH="."
CLASSPATH=${CLASSPATH}${PATHSEP}${SCRIPTDIR}/../ext/*
CLASSPATH=${CLASSPATH}${PATHSEP}${SCRIPTDIR}/../ext/jetty/*
CLASSPATH=${CLASSPATH}${PATHSEP}${SCRIPTDIR}/../ext/jetty/logging/*
CLASSPATH=${CLASSPATH}${PATHSEP}${SCRIPTDIR}/../build/src/test/http_server

LOGFILE="${TEST_HTTP_SERVER_LOG:-test_http_server.log}"

echo $CLASSPATH
echo "Logging TestHttpServer output to ${LOGFILE}"

#DEBUG="-DDEBUG=true -Dorg.eclipse.jetty.LEVEL=DEBUG -Djavax.net.debug=ssl,handshake,data"

java -cp $CLASSPATH ${DEBUG} TestHttpServer >> "${LOGFILE}" 2>&1 &
echo $! > test_http_server.pid
