from threading import Thread
from SocketServer import ThreadingMixIn
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
import time

class Handler(BaseHTTPRequestHandler):

    def version_string(self):
        return "Dynamo 1.0"

    def do_GET(self):
        to_send = ""
        if self.path == "/":
            a,b = self.headers.get('X-A', None), self.headers.get('X-B', None)
            if a and b:
                to_send = 'Hello %s%s' % (a, b)
            else:
                to_send = 'Hello'
        elif self.path == '/sleep':
            time.sleep(2)

        if to_send:
            self.send_response(200)
        else:
            self.send_response(404)

        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write(to_send)

    def do_POST(self):
        len = self.headers.get('Content-Length')
        s = self.rfile.read(int(len))

        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write('PONG')
        self.wfile.write(s)

class Server(Thread):
    def __init__(self, disable_error_logging = True):
        self.server = HTTPServer(("localhost", 9001), Handler)
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
    s = Server(disable_error_logging = False)
    s.start()

    while True:
        try:
            import time
            time.sleep(0.1)
        except:
            s.stop()
            break
