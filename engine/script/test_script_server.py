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

from threading import Thread
from socketserver import ThreadingMixIn
from http.server import HTTPServer, BaseHTTPRequestHandler
import threading
import time
import sys
import socket

class Handler(BaseHTTPRequestHandler):

    def version_string(self):
        return "Dynamo 1.0"

    def do_GET(self):
        to_send = ""
        extra_headers = None

        if self.path == "/":
            a,b = self.headers.get('X-A', None), self.headers.get('X-B', None)
            if a and b:
                to_send = 'Hello %s%s' % (a, b)
            else:
                to_send = 'Hello'

        elif self.path.startswith('/sleep'):

            tokens = self.path.split('/')
            sleeptime = 2.0
            if len(tokens) > 2:
                try:
                    sleeptime = float(tokens[2])
                except:
                    self.send_response(500, "Could not parse time argument as float: %s" % self.path)
                    self.send_header("Content-type", "text/plain")
                    self.end_headers()
                    self.wfile.write(to_send.encode('ascii'))
                    return

            sys.stdout.flush()
            time.sleep( sleeptime )
            to_send = "slept for %f" % sleeptime
        else:
            try:
                import test_script_server_plugin
                result = test_script_server_plugin.do_GET(self.path)

                to_send = result['to_send']
                extra_headers = result['headers']

            except Exception as e:
                pass

        if to_send:
            self.send_response(200)
        else:
            self.send_response(404)

        self.send_header("Content-type", "text/plain")

        if extra_headers:
            for k,v in extra_headers.items():
                self.send_header(k, v)

        self.end_headers()
        self.wfile.write(to_send.encode('ascii'))

    def do_POST(self):
        len = self.headers.get('Content-Length')
        s = self.rfile.read(int(len))
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write('PONG'.encode('ascii'))
        self.wfile.write(s)

    def do_PUT(self):
        len = self.headers.get('Content-Length')
        s = self.rfile.read(int(len))

        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write('PONG_PUT'.encode('ascii'))
        self.wfile.write(s)

    def do_HEAD(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.send_header("Content-Length", 1234)
        self.end_headers()


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """ """


class Server(Thread):
    def __init__(self, disable_error_logging = True, port=9001, ip="localhost"):
        self.server = ThreadedHTTPServer((ip, port), Handler)
        if disable_error_logging:
            # Disable broken pipe messages etc from python server
            self.server.handle_error = self.handle_error
        Thread.__init__(self)

    def handle_error(self, request, client_address):
        pass

    def run(self):
        self.server.serve_forever()

    def stop(self):
        self.server.shutdown()


if __name__ == '__main__':
    server = ThreadedHTTPServer(('localhost', 9001), Handler)
    server.serve_forever()
