#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import sys, os
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import cgi, ssl

class ServerHandler(BaseHTTPRequestHandler):

    def __prepare_get_request(self):
        if self.path.find('?') != -1:
            self.path, self.query_string = self.path.split('?', 1)
        else:
            self.query_string = ''
        self.globals = dict(cgi.parse_qsl(self.query_string))

        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_GET(self):
        self.__prepare_get_request()

        # Code to handle the GET request
        self.wfile.write(str(self.globals))


def usage(exitcode):
    print("Usage: %s <port>" % (os.path.basename(__file__)))
    sys.exit(exitcode)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage(1)

    try:
        _port = int(sys.argv[1])
        server = HTTPServer(("", _port), ServerHandler)
        server.socket = ssl.wrap_socket(server.socket, server_side=True,
            certfile="cert.pem", keyfile="key.pem")
        print("Running HTTPS server on port %d" % (_port))

        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.socket.close()
    except Exception as ex:
        print("Exception occurred in HTTPS server: %s" % (str(ex)))
        if server.socket != None:
            try:
                server.socket.close()
            except Exception as c_ex:
                print("Exception occurred during exceptional shutdown: %s"
                    % (str(c_ex)))
