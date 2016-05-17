#!/usr/bin/python

import string, cgi, time, os, ssl, sys
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

PORT_NUMBER = int(sys.argv[1])

class MyRequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        try:
            if "hub.verify_token" in self.path:
                # fb test hooks
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                querys = dict(cgi.parse_qsl(self.path))
                verify_token = "tokentest"
                if querys["hub.verify_token"] == verify_token:
                    self.wfile.write(str(querys["hub.challenge"]))
                return

            if self.path.endswith("/postwebhook"):
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write("ok")
                return

            # your logic for this URL here, such as executing a bash script
            file_path = os.path.abspath(self.path.lstrip("/"))
            if os.path.isfile(file_path):
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                f=open(file_path)
                self.wfile.write(f.read())
                f.close()
                return

            else:
                self.send_error(404, "File Not Found %s" % self.path)

        except:
            self.send_error(404, "File Not Found %s" % self.path)


    def do_POST(self):
        try:
            if self.headers["Content-type"] == "application/json":
                self.send_response(200)
                length = int(self.headers.getheader('content-length'))
                print 'content-data:', str(self.rfile.read(length))
                return
        except:
            pass
        self.do_GET()


def main():
    try:
        # you can specify any port you want by changing "81"
        server = HTTPServer(("", PORT_NUMBER), MyRequestHandler) # port 81?
        server.socket = ssl.wrap_socket(server.socket, certfile="cert.pem", keyfile="key.pem", server_side = True)
        print "starting httpserver on port " , PORT_NUMBER , "\nPress ^C to quit."
        server.serve_forever()
    except KeyboardInterrupt:
        print " received, shutting down server"

    server.socket.close()

if __name__ == "__main__":
    main()
