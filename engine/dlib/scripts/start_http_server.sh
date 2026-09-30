#!/usr/bin/env bash

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
SERVER_SOURCE="${SCRIPTDIR}/../src/test/http_server/TestHttpServer.java"
SERVER_CLASS_DIR="${SCRIPTDIR}/../build/src/test/http_server"
SERVER_CLASS="${SERVER_CLASS_DIR}/TestHttpServer.class"

LOGFILE="${TEST_HTTP_SERVER_LOG:-test_http_server.log}"

echo $CLASSPATH
echo "Logging TestHttpServer output to ${LOGFILE}"

#DEBUG="-DDEBUG=true -Dorg.eclipse.jetty.LEVEL=DEBUG -Djavax.net.debug=ssl,handshake,data"

mkdir -p "${SERVER_CLASS_DIR}"
if [ ! -f "${SERVER_CLASS}" ] || [ "${SERVER_SOURCE}" -nt "${SERVER_CLASS}" ]; then
    echo "Compiling TestHttpServer.java"
    javac -g --release 8 -Xlint:-options -cp "$CLASSPATH" -d "${SERVER_CLASS_DIR}" "${SERVER_SOURCE}"
fi

java -cp $CLASSPATH ${DEBUG} TestHttpServer >> "${LOGFILE}" 2>&1 &
echo $! > test_http_server.pid
