#!/usr/bin/env python

import BaseHTTPServer, SimpleHTTPServer, ssl, argparse

class SimpleHTTPFileHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

    def do_GET(self):
        print("========== GET STARTED ==========")
        print(self.headers)
        SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        print("========== POST STARTED ==========")
        print(self.headers)
        SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--certificate', required=True)
    parser.add_argument('--keyfile', required=True)
    arguments = parser.parse_args()

    httpd = BaseHTTPServer.HTTPServer(('', 443), SimpleHTTPFileHandler)
    httpd.socket = ssl.wrap_socket(httpd.socket, certfile=arguments.certificate, keyfile=arguments.keyfile, server_side=True)
    httpd.serve_forever()