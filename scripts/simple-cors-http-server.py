#!/usr/bin/env python3
import http.server
import os
import sys

class CORSRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=os.getcwd(), **kwargs)

    extensions_map = {
        ".html": "text/html",
        ".css": "text/css",
        ".js": "application/javascript",
        ".json": "application/json",
        ".wasm": "application/wasm",
        ".png": "image/png",
        ".jpg": "image/jpg",
        ".svg": "image/svg+xml",
        ".xml": "application/xml",
        "": "application/octet-stream"
    }

    def send_response(self, *args, **kwargs):
        http.server.SimpleHTTPRequestHandler.send_response(
            self,
            *args,
            **kwargs
        )
        # self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        if self.path.endswith(".wasmz"):
            self.send_header("Content-Encoding", "gzip")


# class CORSRequestHandler (http.server.SimpleHTTPRequestHandler):
#     def end_headers (self):
#         self.send_header('Access-Control-Allow-Origin', '*')
#         self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
#         self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
#         http.server.SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
    http.server.test(CORSRequestHandler, http.server.HTTPServer, port=int(sys.argv[1]) if len(sys.argv) > 1 else 8000)
