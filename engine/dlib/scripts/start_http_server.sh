#!/usr/bin/env bash

# Copyright 2020 The Defold Foundation
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

CLASSPATH=$SCRIPTDIR/../ext/jetty-all-7.0.2.v20100331.jar:$SCRIPTDIR/../ext/servlet-api-2.5.jar:$SCRIPTDIR/../build/default/src/test/http_server:.:$CLASSPATH

echo $CLASSPATH

java -cp $CLASSPATH TestHttpServer &
echo $! > test_http_server.pid

