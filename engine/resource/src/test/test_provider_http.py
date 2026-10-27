#!/usr/bin/env python3
"""Run HTTP provider regressions against deterministic local response fixtures."""

import http.server
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from urllib.parse import unquote, urlsplit


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    large = bytes(i % 251 for i in range(128 * 1024))
    data_dir = None
    retries = 0
    batch_mode = "normal"
    revision = 1
    resource_requests = 0
    verification_requests = 0
    verification_entries = 0

    def log_message(self, *_):
        pass

    def do_HEAD(self):
        self.respond(head=True)

    def do_GET(self):
        self.respond(head=False)

    def do_POST(self):
        if self.path != "/__verify_etags__":
            self.send_error(404)
            return
        Handler.verification_requests += 1
        entries = self.rfile.read(int(self.headers["Content-Length"])).decode().splitlines()
        Handler.verification_entries = len(entries)
        if Handler.batch_mode == "unavailable":
            self.send_error(404)
            return
        valid = []
        for entry in entries:
            key, tag = entry.split(" ", 1)
            path = unquote(urlsplit(key).path)
            if path.startswith("/build/") and tag == self.batch_tag(path):
                valid.append(key)
        # Deliberately omit the final newline and split records across writes.
        data = "\n".join(valid).encode()
        if Handler.batch_mode == "invalid":
            data += b"\n" + b"x" * 4096
        self.send_response(200)
        self.send_header("Content-Length", str(len(data) + (3 if Handler.batch_mode == "truncated" else 0)))
        self.send_header("Connection", "close")
        self.close_connection = True
        self.end_headers()
        for offset in range(0, len(data), 17):
            self.wfile.write(data[offset:offset + 17])

    @staticmethod
    def batch_tag(path):
        revision = Handler.revision if path.endswith(("/changed.bin", "/space file.bin")) else 1
        return f'"version-{revision}"'

    def respond(self, head):
        path = self.path
        if path.startswith("/__test/"):
            command = path[len("/__test/"):]
            value = 0
            if command == "reset":
                Handler.batch_mode = "normal"
                Handler.revision = 1
            elif command == "counts-reset":
                Handler.resource_requests = 0
                Handler.verification_requests = 0
                Handler.verification_entries = 0
            elif command == "change":
                Handler.revision = 2
            elif command == "requests":
                value = Handler.resource_requests
            elif command == "verifications":
                value = Handler.verification_requests
            elif command == "entries":
                value = Handler.verification_entries
            else:
                Handler.batch_mode = command
            data = str(value).encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if path.startswith("/build/"):
            Handler.resource_requests += 1
            path = unquote(path)
            tag = self.batch_tag(path)
            if self.headers.get("If-None-Match") == tag:
                self.send_response(304)
                self.send_header("ETag", tag)
                self.end_headers()
                return
            data = bytes(range(12 if tag == '"version-2"' else 8))
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("ETag", tag)
            self.end_headers()
            if not head:
                self.wfile.write(data)
            return
        bad_cached_range = path in ("/cached-bad-range", "/cached-bad-range-chunked", "/cached-bad-range-unknown")
        cacheable = bad_cached_range or path in ("/somedata", "/unknown-range", "/unknown-range-chunked")
        if path == "/huge-head":
            self.send_response(200)
            self.send_header("Content-Length", str(1024 * 1024 * 1024))
            self.end_headers()
            return
        if path in ("/test.cont", "/test_ref.cont"):
            data = (self.data_dir / path[1:]).read_bytes()
        elif path in ("/large", "/chunked", "/unknown"):
            data = self.large
        elif cacheable or path in ("/retry", "/bad-range", "/truncated"):
            data = bytes(range(8))
        else:
            self.send_error(404)
            return

        status = 200
        content_range = None
        if cacheable and self.headers.get("Range"):
            start, end = map(int, self.headers["Range"][6:].split("-"))
            end = min(end, len(data) - 1)
            total = len(data) if path == "/somedata" else "*"
            content_range = f"bytes {start}-{end}/{total}"
            data = data[start:end + 1]
            status = 206
        if bad_cached_range:
            # The body contains five bytes, but the declared range contains two.
            total = "*" if path == "/cached-bad-range-unknown" else "8"
            content_range = f"bytes 3-4/{total}"
        if path == "/bad-range":
            content_range = "bytes 7-2/8"
            status = 206
        tag = '"' + (content_range or "full") + '"'
        if cacheable and self.headers.get("If-None-Match") == tag:
            self.send_response(304)
            self.send_header("ETag", tag)
            self.end_headers()
            return

        retry = path == "/retry" and Handler.retries == 0
        if path == "/retry":
            Handler.retries += 1
        self.send_response(status)
        self.send_header("Connection", "close")
        self.close_connection = True
        if cacheable:
            self.send_header("ETag", tag)
        if content_range:
            self.send_header("Content-Range", content_range)
        chunked = path in ("/chunked", "/unknown-range-chunked", "/cached-bad-range-chunked") or retry
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        elif path != "/unknown":
            self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if head:
            return
        if retry:
            # Exceed the destination, then end mid-framing. The successful
            # retry must reset both the write cursor and the overflow error.
            self.wfile.write(b"10\r\nincorrect prefix\r\n")
            return
        if path == "/truncated":
            self.wfile.write(data[:3])
            return
        for offset in range(0, len(data), 333):
            chunk = data[offset:offset + 333]
            if chunked:
                self.wfile.write(f"{len(chunk):x}\r\n".encode())
            self.wfile.write(chunk)
            if chunked:
                self.wfile.write(b"\r\n")
        if chunked:
            self.wfile.write(b"0\r\n\r\n")


def main():
    executable = str(Path(sys.argv[1]).resolve())
    Handler.data_dir = Path(sys.argv[2]).resolve()
    # Binding/listening happens before the test starts; no readiness sleeps.
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            with tempfile.TemporaryDirectory(prefix="defold-provider-http-") as directory:
                config = Path(directory) / "server.cfg"
                config.write_text(f"[server]\nsocket={server.server_port}\n")
                return subprocess.call([executable, str(config)], cwd=directory)
        finally:
            server.shutdown()
            thread.join()


if __name__ == "__main__":
    sys.exit(main())
