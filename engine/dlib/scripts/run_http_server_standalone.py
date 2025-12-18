#! /usr/bin/env python
# Copyright 2020-2025 The Defold Foundation
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

import urllib, urllib.request, time, atexit, os, sys, configparser, tempfile

server_sockets = None

server_config_path = "test_http_server.cfg"
if os.path.exists(server_config_path):
    os.unlink(server_config_path)

start = time.time()
timeout = 30 if sys.platform == 'win32' else 15
os.system('scripts/start_http_server.sh')
while True:
    if time.time() - start > timeout:
        error('HTTP server failed to start within ' + timeout + ' seconds')
        sys.exit(1)
    try:
        if not os.path.exists(server_config_path):
            raise IOError("Waiting for server to write config file")

        server_sockets = configparser.RawConfigParser()
        server_sockets.read(server_config_path)
        server_socket = server_sockets.getint("server", "socket")

        urllib.request.urlopen('http://localhost:%d' % server_socket)
        print("Started server at http://localhost:%d" % server_socket)
        break
    except IOError:
        print('Waiting for HTTP testserver to start')
    except urllib.URLError:
        print('Waiting for HTTP testserver to start on port %d...' % server_socket)

    sys.stdout.flush()
    time.sleep(0.5)


# write a config file for the tests to use
config = configparser.RawConfigParser()

config.add_section("server")
config.set("server", "socket", server_sockets.getint("server", "socket"))
config.set("server", "socket_ssl", server_sockets.getint("server", "socket_ssl"))
config.set("server", "socket_ssl_test", server_sockets.getint("server", "socket_ssl_test"))

configfilepath = os.path.basename("unittest_data.cfg")
with open(configfilepath, 'w') as f:
    config.write(f)
    print("Wrote test config file: %s" % configfilepath)
