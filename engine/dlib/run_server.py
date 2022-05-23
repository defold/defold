#! /usr/bin/env python

import urllib, urllib.request, time, atexit, os, sys, configparser, tempfile

server_sockets = None


os.system('scripts/start_http_server.sh')
#atexit.register(os.system, 'scripts/stop_http_server.sh')

server_config_path = "test_http_server.cfg"
if os.path.exists(server_config_path):
    os.unlink(server_config_path)

start = time.time()
while True:
    if time.time() - start > 5:
        error('HTTP server failed to start within 5 seconds')
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