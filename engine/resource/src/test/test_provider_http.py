#!/usr/bin/env python3
"""Run HTTP provider regressions against deterministic local response fixtures."""

import http.server
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from urllib.parse import quote_plus, unquote, urlsplit


# These limits must stay synchronized with MAX_BATCH_FILE_SIZE and MAX_BATCH_SIZE
# in engine/resource/src/providers/provider_http.cpp and max-batch-file-size and
# max-batch-size in editor/src/clj/editor/hot_reload.clj.
MAX_BATCH_FILE_SIZE = 1024 * 1024
MAX_BATCH_SIZE = 4 * 1024 * 1024


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    large = bytes(i % 251 for i in range(128 * 1024))
    data_dir = None
    retries = 0
    batch_mode = "normal"
    revision = 1
    deleted = False
    resource_requests = 0
    verification_requests = 0
    verification_entries = 0
    index_mode = "unavailable"
    index_prefix = ""
    index_requests = 0
    download_mode = "unavailable"
    download_requests = 0
    download_bytes = 0
    download_max_bytes = 0
    download_first_size = 0
    download_sorted = 1

    def log_message(self, *_):
        pass

    def do_HEAD(self):
        self.respond(head=True)

    def do_GET(self):
        self.respond(head=False)

    def do_POST(self):
        if self.path == "/__resource_batch__":
            self.respond_batch()
            return
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
            if path.startswith("/build/") and not self.is_deleted(path) and tag == self.batch_tag(path):
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
    def is_deleted(path):
        return Handler.deleted and path.endswith("/changed.bin")

    @staticmethod
    def batch_tag(path):
        revision = Handler.revision if path.endswith(("/changed.bin", "/space file.bin", "/shrink.bin")) else 1
        return f'"version-{revision}"'

    @staticmethod
    def batch_size(path):
        if path.endswith("/empty.bin"):
            return 0
        if path.endswith("/huge.bin"):
            return 1024 * 1024 * 1024
        if path.endswith("/over-limit.bin"):
            return MAX_BATCH_FILE_SIZE + 1
        if path.rsplit("/", 1)[-1].startswith("limit-"):
            return MAX_BATCH_FILE_SIZE
        size = 12 if Handler.batch_tag(path) == '"version-2"' else 8
        return 20 - size if path.endswith("/shrink.bin") else size

    @staticmethod
    def batch_data(size):
        return (bytes(range(251)) * ((size + 250) // 251))[:size]

    def respond_batch(self):
        Handler.download_requests += 1
        if Handler.download_mode == "unavailable":
            self.send_error(404)
            return
        records = self.rfile.read(int(self.headers["Content-Length"])).decode().splitlines()
        entries = [record.split("\t") for record in records]
        if any(self.is_deleted(unquote(key)) for key, _ in entries):
            self.send_error(404)
            return
        sizes = [self.batch_size(unquote(key)) for key, _ in entries]
        Handler.download_first_size = sizes[0]
        Handler.download_sorted = int(sizes[1:] == sorted(sizes[1:]))
        Handler.download_bytes += sum(sizes)
        Handler.download_max_bytes = max(Handler.download_max_bytes, sum(sizes))
        if max(sizes) > MAX_BATCH_FILE_SIZE or sum(sizes) > MAX_BATCH_SIZE:
            self.send_error(413)
            return
        if Handler.download_mode == "stale" or any(tag != self.batch_tag(unquote(key)) for key, tag in entries):
            self.send_error(409)
            return
        data = bytearray(b"DM_RESOURCE_BATCH 1\n")
        first_body = 0
        for i, ((key, tag), size) in enumerate(zip(entries, sizes)):
            if i == 0:
                if Handler.download_mode == "invalid-tag":
                    tag = "wrong"
                elif Handler.download_mode == "invalid-size":
                    size += 1
                elif Handler.download_mode == "invalid-path":
                    key += ".wrong"
            data.extend(f"{key}\t{size}\t{tag}\n".encode())
            if i == 0:
                first_body = len(data)
            data.extend(self.batch_data(size))
        content_length = len(data)
        if Handler.download_mode == "truncated-first":
            data = data[:first_body + sizes[0] // 2]
        elif Handler.download_mode == "truncated-last":
            data = data[:-3]
        self.send_response(200)
        self.send_header("Content-Length", str(content_length))
        self.send_header("Connection", "close")
        self.close_connection = True
        self.end_headers()
        # Cross both text-header and binary-body boundaries on short fixtures.
        stride = 7 if len(data) < 4096 else 4093
        for offset in range(0, len(data), stride):
            self.wfile.write(data[offset:offset + stride])

    def respond(self, head):
        path = self.path
        if path == "/__resource_index__":
            Handler.index_requests += 1
            if Handler.index_mode == "unavailable":
                self.send_error(404)
                return
            files = ["unchanged.bin", "changed.bin", "empty.bin", "huge.bin", "shrink.bin",
                     "space file.bin", "space%20file.bin", "a+b.bin"]
            if Handler.download_mode == "large":
                files += [f"limit-{i}.bin" for i in range(8)] + ["over-limit.bin"]
            records = ["DM_RESOURCE_INDEX 1\n"]
            for name in files:
                file_path = Handler.index_prefix + "/" + name
                if self.is_deleted(file_path):
                    continue
                size = self.batch_size(file_path)
                records.append(f"{quote_plus(file_path)}\t{size}\t{self.batch_tag(file_path)}\n")
            records.append(f"{quote_plus(Handler.index_prefix + '-other/outside.bin')}\t999\tother\n")
            data = "".join(records).encode()
            if Handler.index_mode == "oversized":
                data += b"x" * 4096 + b"\n"
            elif Handler.index_mode == "invalid-size":
                data += b"%2Fbuild%2Fbad.bin\t4294967296\ttag\n"
            elif Handler.index_mode == "invalid-path":
                data += b"%2Fbuild%2Fbad%00.bin\t8\ttag\n"
            elif Handler.index_mode == "duplicate":
                data += records[1].encode()
            elif Handler.index_mode == "incomplete":
                data = data[:-1]
            elif Handler.index_mode == "version":
                data = data.replace(b"DM_RESOURCE_INDEX 1", b"DM_RESOURCE_INDEX 2")
            self.send_response(200)
            self.send_header("Content-Length", str(len(data) + (3 if Handler.index_mode == "truncated" else 0)))
            self.send_header("Connection", "close")
            self.close_connection = True
            self.end_headers()
            for offset in range(0, len(data), 17):
                self.wfile.write(data[offset:offset + 17])
            return
        if path.startswith("/__test/"):
            command = path[len("/__test/"):]
            value = 0
            if command == "reset":
                Handler.batch_mode = "normal"
                Handler.revision = 1
                Handler.deleted = False
                Handler.index_mode = "unavailable"
                Handler.download_mode = "unavailable"
            elif command == "counts-reset":
                Handler.resource_requests = 0
                Handler.verification_requests = 0
                Handler.verification_entries = 0
                Handler.index_requests = 0
                Handler.download_requests = 0
                Handler.download_bytes = 0
                Handler.download_max_bytes = 0
                Handler.download_first_size = 0
            elif command == "change":
                Handler.revision = 2
            elif command == "delete":
                Handler.deleted = True
            elif command == "requests":
                value = Handler.resource_requests
            elif command == "verifications":
                value = Handler.verification_requests
            elif command == "entries":
                value = Handler.verification_entries
            elif command == "indices":
                value = Handler.index_requests
            elif command == "downloads":
                value = Handler.download_requests
            elif command == "download-called":
                value = int(Handler.download_requests > 0)
            elif command == "download-bytes":
                value = Handler.download_bytes
            elif command == "download-max-bytes":
                value = Handler.download_max_bytes
            elif command == "download-first-size":
                value = Handler.download_first_size
            elif command == "download-sorted":
                value = Handler.download_sorted
            elif command.startswith("download-"):
                Handler.download_mode = command[len("download-"):]
            elif command.startswith("index/"):
                Handler.index_prefix = command[len("index"):]
                Handler.index_mode = "normal"
            elif command.startswith("index-"):
                Handler.index_mode = command[len("index-"):]
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
            if self.is_deleted(path):
                self.send_error(404)
                return
            tag = self.batch_tag(path)
            if self.headers.get("If-None-Match") == tag:
                self.send_response(304)
                self.send_header("ETag", tag)
                self.end_headers()
                return
            data = self.batch_data(self.batch_size(path))
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
