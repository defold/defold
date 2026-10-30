"""Real HTTP acceptance for the standalone recording service with a controlled backend."""

import argparse
import contextlib
import hashlib
import http.client
import json
import os
import pathlib
import queue
import socket
import threading
import subprocess
import tempfile
import time
import unittest


class RecordingServiceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls, connection_timeout=60):
        # Exercise start, hashing, download and cleanup in a non-ASCII storage path.
        cls.directory = tempfile.TemporaryDirectory(prefix="defold-record-test-é-录像-")
        cls.log = tempfile.TemporaryFile(mode="w+")
        cls.process = subprocess.Popen([SERVER, cls.directory.name], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=cls.log, text=True, bufsize=1,
                                       env={**os.environ, "DEFOLD_RECORD_TEST_CONNECTION_TIMEOUT": str(connection_timeout)})
        ready = queue.Queue()
        cls.paused = queue.Queue()
        output = []

        def read_output():
            for line in cls.process.stdout:
                output.append(line)
                if line.startswith("RECORD_TEST_PORT="):
                    ready.put(int(line.split("=", 1)[1]))
                elif line.strip() == "RECORD_TEST_PAUSED":
                    cls.paused.put(True)
            ready.put(None)

        cls.output_thread = threading.Thread(target=read_output, daemon=True)
        cls.output_thread.start()
        try:
            cls.port = ready.get(timeout=15)
            if cls.port is not None:
                cls.instance_id = cls.request("GET", "/recording/capabilities")["instance_id"]
                return
        except queue.Empty:
            pass
        cls.process.kill()
        cls.process.wait()
        cls.log.seek(0)
        raise AssertionError("Recording test server did not become ready: " + "".join(output) + cls.log.read())

    @classmethod
    def tearDownClass(cls):
        try:
            cls.request("GET", "/test/quit", expected=200)
            cls.process.wait(timeout=15)
            if cls.process.returncode:
                cls.log.seek(0)
                raise AssertionError(cls.log.read())
        finally:
            if cls.process.poll() is None:
                cls.process.kill()
                cls.process.wait()
            cls.output_thread.join(timeout=5)
            cls.process.stdin.close()
            cls.process.stdout.close()
            cls.log.close()
            cls.directory.cleanup()

    def setUp(self):
        self.request("GET", "/test/restart")
        type(self).instance_id = self.request("GET", "/recording/capabilities")["instance_id"]

    @classmethod
    def request(cls, method, path, body=None, expected=200, headers=None, instance=True):
        headers = dict(headers or {})
        # Each request owns one connection; explicitly release the server slot.
        headers.setdefault("Connection", "close")
        if instance and path.startswith("/recording/") and path != "/recording/capabilities":
            headers.setdefault("X-Recording-Instance", cls.instance_id)
        if isinstance(body, dict):
            body = json.dumps(body)
            headers["Content-Type"] = "application/json"
        connection = http.client.HTTPConnection("127.0.0.1", cls.port, timeout=5)
        try:
            connection.request(method, path, body=body, headers=headers)
            response = connection.getresponse()
            data = response.read()
            if response.status != expected:
                raise AssertionError(f"{method} {path}: expected {expected}, got {response.status}: {data!r}")
            return json.loads(data) if data.startswith(b"{") else data
        finally:
            connection.close()

    def wait_status(self, complete_file=False):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            status = self.request("GET", "/recording/status")
            if not status["pending"]:
                if not complete_file or status["file"]["state"] != "pending":
                    return status
        self.fail("Recording completion receipt did not arrive")

    def prepare_large_recording(self, size):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        # The controlled backend has finished writing before reporting active.
        # Extend its output before Stop starts the real preparation/hash worker.
        recording, = pathlib.Path(self.directory.name).glob("*.mp4")
        with recording.open("r+b") as output:
            output.truncate(size)
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        self.assertEqual(size, file["size"])
        return file

    def wait_stream_closed(self, expected):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            stats = self.request("GET", "/test/stream-stats")
            if stats["closed"] >= expected:
                self.assertEqual(expected, stats["closed"])
                return stats
        self.fail("Stream close callback did not arrive")

    @contextlib.contextmanager
    def server_timeout(self, seconds):
        cls = type(self)
        cls.tearDownClass()
        cls.setUpClass(connection_timeout=seconds)
        try:
            yield
        finally:
            try:
                cls.tearDownClass()
            finally:
                cls.setUpClass()

    @contextlib.contextmanager
    def open_stream(self, length, read_limit=65536):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            connection.settimeout(5)
            connection.connect(("127.0.0.1", self.port))
            connection.sendall((f"GET /test/stream/{length} HTTP/1.1\r\n"
                                "Host: 127.0.0.1\r\nConnection: close\r\n"
                                f"X-Test-Read-Limit: {read_limit}\r\n\r\n").encode("ascii"))
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(200, response.status)
                self.assertEqual(str(length), response.getheader("Content-Length"))
                self.assertIsNone(response.getheader("Transfer-Encoding"))
                yield connection, response

    # Discovery and lifecycle registration must work without the automation library.
    def test_discovery_and_unregistration(self):
        paths = self.request("GET", "/openapi.json")["paths"]
        self.assertEqual({"/recording/capabilities", "/recording/status", "/recording/start",
                          "/recording/stop", "/recording/files/{id}"}, set(paths))
        self.assertNotIn("automation", json.dumps(paths))
        properties = paths["/recording/start"]["post"]["requestBody"]["content"]["application/json"]["schema"]["properties"]
        self.assertEqual({"fps", "audio"}, set(properties))
        self.assertEqual(60, paths["/recording/start"]["post"]["requestBody"]["content"]
                         ["application/json"]["schema"]["properties"]["fps"]["maximum"])
        download = paths["/recording/files/{id}"]["get"]
        self.assertFalse(next(p for p in download["parameters"] if p["name"] == "Range")["required"])
        self.assertTrue({"200", "206", "416"} <= set(download["responses"]))
        for path, method in (("/recording/start", "post"), ("/recording/stop", "post"),
                             ("/recording/files/{id}", "delete")):
            parameter = next(p for p in paths[path][method]["parameters"]
                             if p["name"] == "X-Recording-Instance")
            self.assertTrue(parameter["required"])
            self.assertEqual("header", parameter["in"])
            self.assertIn("410", paths[path][method]["responses"])
        self.request("GET", "/test/unregister")
        self.assertEqual({}, self.request("GET", "/openapi.json")["paths"])
        self.request("GET", "/recording/status", expected=404)
        self.request("GET", "/test/restart")
        self.assertEqual(5, len(self.request("GET", "/openapi.json")["paths"]))

    # Start/stop receipts, full and partial downloads, checksums and deletion form a complete service.
    def test_record_download_delete(self):
        receipt = self.request("POST", "/recording/start", {"fps": 60}, 202)
        self.assertTrue(receipt["pending"])
        status = self.wait_status()
        self.assertTrue(status["active"])
        self.assertEqual((320, 240, 60), (status["width"], status["height"], status["fps"]))
        self.assertEqual(receipt["operation_id"], status["operation_id"])
        self.request("POST", "/recording/start", {}, 409)
        self.request("POST", "/recording/stop", {"operation_id": status["operation_id"] + 1}, 410)
        self.request("GET", status["file"]["url"], expected=409)
        self.request("POST", "/recording/stop", {"operation_id": status["operation_id"]}, 202)
        status = self.wait_status(complete_file=True)
        self.assertTrue(status["finalized"])
        self.assertEqual("complete", status["file"]["state"])
        url = status["file"]["url"]
        self.assertEqual(b"standalone recording test\n", self.request("GET", url))
        self.request("GET", url, expected=416, headers={"Range": "bytes=0-9999"})
        data = self.request("GET", url, expected=206, headers={"Range": f'bytes=0-{status["file"]["size"] - 1}'})
        self.assertEqual(b"standalone recording test\n", data)
        self.assertEqual(hashlib.sha256(data).hexdigest(), status["file"]["sha256"])
        self.request("DELETE", url, expected=204)
        self.request("GET", url, expected=410)
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Hash preparation must include the last bytes on either side of its 64 KiB read boundary.
    def test_recording_hash_chunk_boundaries(self):
        for size in (65535, 65536, 65537, 2 * 65536 + 17):
            with self.subTest(size=size):
                receipt = self.request("POST", "/recording/start", {}, 202)
                self.wait_status()
                recording, = pathlib.Path(self.directory.name).glob("*.mp4")
                data = (bytes(range(256)) * ((size + 255) // 256))[:size]
                recording.write_bytes(data)
                self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
                file = self.wait_status(complete_file=True)["file"]
                self.assertEqual("complete", file["state"])
                self.assertEqual(size, file["size"])
                self.assertEqual(hashlib.sha256(data).hexdigest(), file["sha256"])
                self.request("DELETE", file["url"], expected=204)

    # Failed file preparation must release its unfinished hash context and leave a deletable receipt.
    def test_recording_hash_missing_or_empty_file(self):
        for missing in (False, True):
            with self.subTest(missing=missing):
                receipt = self.request("POST", "/recording/start", {}, 202)
                self.wait_status()
                recording, = pathlib.Path(self.directory.name).glob("*.mp4")
                if missing:
                    recording.unlink()
                else:
                    recording.write_bytes(b"")
                self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
                file = self.wait_status(complete_file=True)["file"]
                self.assertEqual("failed", file["state"])
                self.assertEqual("", file["sha256"])
                self.assertEqual(0, file["size"])
                self.request("GET", file["url"], expected=500)
                self.request("DELETE", file["url"], expected=204)

    # Whole files and ranges larger than 1 MiB must stream with correct headers on a reused connection.
    def test_full_file_and_large_ranges(self):
        size = 4 * 1024 * 1024 + 17
        file = self.prepare_large_recording(size)
        expected = bytes(range(256)) * 4096 + bytes(size - 1024 * 1024)
        with contextlib.closing(http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)) as connection:
            for bounds in [(17, size - 3), None, (0, size - 1), None]:
                with self.subTest(bounds=bounds):
                    headers = {"X-Recording-Instance": self.instance_id}
                    if bounds:
                        headers["Range"] = f"bytes={bounds[0]}-{bounds[1]}"
                    connection.request("GET", file["url"], headers=headers)
                    with connection.getresponse() as response:
                        self.assertEqual(206 if bounds else 200, response.status)
                        self.assertEqual("video/mp4", response.getheader("Content-Type"))
                        self.assertEqual("bytes", response.getheader("Accept-Ranges"))
                        self.assertEqual(file["sha256"], response.getheader("ETag"))
                        self.assertEqual(str(bounds[1] - bounds[0] + 1 if bounds else size),
                                         response.getheader("Content-Length"))
                        self.assertIsNone(response.getheader("Transfer-Encoding"))
                        self.assertEqual(f"bytes {bounds[0]}-{bounds[1]}/{size}" if bounds else None,
                                         response.getheader("Content-Range"))
                        self.assertEqual(expected[bounds[0]:bounds[1] + 1] if bounds else expected, response.read())
        self.assertEqual(file["sha256"], hashlib.sha256(expected).hexdigest())
        for invalid in ("", "bytes=2-1", f"bytes={size}-{size}", f"bytes=0-{size}",
                        "bytes=0-1,4-5", "bytes=0-", "bytes=-1", "invalid"):
            with self.subTest(invalid=invalid):
                self.request("GET", file["url"], expected=416, headers={"Range": invalid})

    # A stalled range download must yield to updates and other clients, then resume intact.
    def test_stalled_download_does_not_block_updates(self):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        expected = bytes(range(256)) * 4096
        self.assertEqual(len(expected), file["size"])
        updates = self.request("GET", "/test/updates")["updates"]

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
            # Set the receive window before connecting, then leave the body unread.
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            connection.settimeout(5)
            connection.connect(("127.0.0.1", self.port))
            connection.sendall((f'GET {file["url"]} HTTP/1.1\r\n'
                                "Host: 127.0.0.1\r\n"
                                f"X-Recording-Instance: {self.instance_id}\r\n"
                                f"Range: bytes=0-{len(expected) - 1}\r\n\r\n").encode("ascii"))
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(206, response.status)
                self.assertEqual(f"bytes 0-{len(expected) - 1}/{len(expected)}",
                                 response.getheader("Content-Range"))
                self.assertEqual(file["sha256"], response.getheader("ETag"))
                self.assertGreater(self.request("GET", "/test/updates")["updates"], updates)
                self.assertEqual("complete", self.request("GET", "/recording/status")["file"]["state"])
                # A stalled reader must not hold up another download on the sender.
                other = self.request("GET", file["url"], expected=206,
                                     headers={"Range": f"bytes=0-{len(expected) - 1}"})
                self.assertEqual(expected, other)
                # Deletion hides the receipt while the stream retains its open file.
                self.request("DELETE", file["url"], expected=204)
                self.request("GET", file["url"], expected=410)
                data = response.read()
                self.assertEqual(expected, data)
                self.assertEqual(file["sha256"], hashlib.sha256(data).hexdigest())

            # The next keep-alive response must start after the entire streamed body.
            connection.sendall(b"GET /recording/capabilities HTTP/1.1\r\n"
                               b"Host: 127.0.0.1\r\nConnection: close\r\n\r\n")
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(200, response.status)
                self.assertEqual(self.instance_id, json.loads(response.read())["instance_id"])
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Nonzero range offsets and a partial final buffer must preserve the exact file bytes.
    def test_streamed_range_offsets(self):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        expected = bytes(range(256)) * 4096
        for start, end in [(17, 131102), (65535, 65536), (file["size"] - 1, file["size"] - 1)]:
            with self.subTest(start=start, end=end):
                data = self.request("GET", file["url"], expected=206, headers={"Range": f"bytes={start}-{end}"})
                self.assertEqual(expected[start:end + 1], data)

    # A reboot releases service ownership without invalidating a worker's active file.
    def test_download_survives_service_restart(self):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            connection.settimeout(5)
            connection.connect(("127.0.0.1", self.port))
            connection.sendall((f'GET {file["url"]} HTTP/1.1\r\n'
                                "Host: 127.0.0.1\r\nConnection: close\r\n"
                                f'Range: bytes=0-{file["size"] - 1}\r\n\r\n').encode("ascii"))
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(206, response.status)
                self.request("GET", "/test/restart")
                type(self).instance_id = self.request("GET", "/recording/capabilities")["instance_id"]
                self.assertEqual(file["sha256"], hashlib.sha256(response.read()).hexdigest())
                self.assertEqual(b"", connection.recv(1))
        self.request("GET", file["url"], expected=410)
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Stream readers must run on the worker in bounded reads, including short reads and empty bodies.
    def test_stream_reader_lifecycle(self):
        for length in (0, 1, 65536, 131089):
            with self.subTest(length=length):
                before = self.request("GET", "/test/stream-stats")
                with self.open_stream(length, read_limit=997) as (_, response):
                    data = response.read()
                self.assertEqual((bytes(range(256)) * ((length + 255) // 256))[:length], data)
                stats = self.wait_stream_closed(before["closed"] + 1)
                self.assertEqual(length, stats["bytes"] - before["bytes"])
                self.assertEqual(0, stats["on_update"])
                self.assertEqual(0, stats["oversized_reads"])

    # Early EOF must fail the declared Content-Length instead of accepting a truncated body.
    def test_stream_read_failure(self):
        before = self.request("GET", "/test/stream-stats")
        with self.assertRaises(http.client.IncompleteRead) as failure:
            self.request("GET", "/test/stream/131089", headers={"X-Test-Fail-After": "123"})
        self.assertEqual(bytes(range(123)), failure.exception.partial)
        self.assertEqual(131089 - 123, failure.exception.expected)
        self.wait_stream_closed(before["closed"] + 1)
        self.request("GET", "/recording/capabilities")

    # A request followed by a write-side FIN must be processed before handling its hangup.
    def test_half_closed_request(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
            connection.settimeout(5)
            self.request("GET", "/test/pause-updates")
            try:
                self.paused.get(timeout=5)
                connection.connect(("127.0.0.1", self.port))
                connection.sendall(b"GET /recording/capabilities HTTP/1.1\r\n"
                                   b"Host: 127.0.0.1\r\nConnection: close\r\n\r\n")
                connection.shutdown(socket.SHUT_WR)
            finally:
                self.process.stdin.write("\n")
                self.process.stdin.flush()
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(200, response.status)
                self.assertEqual(self.instance_id, json.loads(response.read())["instance_id"])

    # A write-side FIN during backpressure must preserve the stream and other clients' responses.
    def test_half_closed_stream(self):
        before = self.request("GET", "/test/stream-stats")
        length = 16 * 1024 * 1024 + 17
        with self.open_stream(length) as (connection, response):
            connection.shutdown(socket.SHUT_WR)
            stats = self.request("GET", "/test/stream-stats")
            self.assertEqual(before["closed"], stats["closed"])
            self.assertLess(stats["bytes"] - before["bytes"], length)
            expected = bytes(range(256)) * 256
            remaining = length
            while remaining:
                size = min(remaining, len(expected))
                self.assertEqual(expected[:size], response.read(size))
                remaining -= size
            self.assertEqual(b"", connection.recv(1))
        self.wait_stream_closed(before["closed"] + 1)

    # A Content-Length beyond 32 bits must retain bounded reads and close once on disconnect.
    def test_stream_backpressure_and_disconnect(self):
        before = self.request("GET", "/test/stream-stats")
        length = 4 * 1024 * 1024 * 1024 + 17
        with self.open_stream(length) as (_, response):
            self.assertEqual(bytes(range(256)), response.read(256))
            stats = self.request("GET", "/test/stream-stats")
            self.assertGreater(stats["bytes"], before["bytes"])
            self.assertLess(stats["bytes"] - before["bytes"], length)
            self.assertEqual(before["closed"], stats["closed"])
            self.assertEqual(0, stats["on_update"])
            self.assertEqual(0, stats["oversized_reads"])
            self.request("GET", "/recording/capabilities")
        self.wait_stream_closed(before["closed"] + 1)

    # Continued writes must keep a stream alive beyond the original connection deadline.
    def test_stream_timeout_tracks_progress(self):
        with self.server_timeout(2):
            before = self.request("GET", "/test/stream-stats")
            # One-byte source reads keep this response in flight while the client
            # continuously consumes it, without sleeps or assumed transfer rates.
            with self.open_stream(1024 * 1024 * 1024, read_limit=1) as (_, response):
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    self.assertEqual(bytes(range(256)), response.read(256))
                self.assertEqual(before["closed"], self.request("GET", "/test/stream-stats")["closed"])
            self.wait_stream_closed(before["closed"] + 1)

    # Refreshing active responses must still close a stream whose client stops reading.
    def test_stalled_stream_times_out(self):
        with self.server_timeout(2):
            before = self.request("GET", "/test/stream-stats")
            with self.open_stream(1024 * 1024 * 1024) as (_, response):
                self.wait_stream_closed(before["closed"] + 1)
                with self.assertRaises(http.client.IncompleteRead):
                    response.read()

    # Completed responses must preserve keep-alive beyond the connection's original deadline.
    def test_keep_alive_timeout_tracks_progress(self):
        with self.server_timeout(2):
            with contextlib.closing(http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)) as connection:
                connection.connect()
                original_socket = connection.sock
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline:
                    connection.request("GET", "/recording/capabilities")
                    with connection.getresponse() as response:
                        self.assertEqual(200, response.status)
                        self.assertEqual(self.instance_id, json.loads(response.read())["instance_id"])
                    self.assertIs(original_socket, connection.sock)

    # Several unread streams must share the sender and resume independently without engine updates.
    def test_streams_continue_without_updates(self):
        before = self.request("GET", "/test/stream-stats")
        length = 1024 * 1024
        expected = bytes(range(256)) * 4096
        with contextlib.ExitStack() as stack:
            pending = [stack.enter_context(self.open_stream(length)) for _ in range(4)]
            try:
                self.request("GET", "/test/pause-updates")
                self.paused.get(timeout=5)
                for connection, response in reversed(pending):
                    self.assertEqual(expected, response.read())
                    self.assertEqual(b"", connection.recv(1))
            finally:
                self.process.stdin.write("\n")
                self.process.stdin.flush()
        self.wait_stream_closed(before["closed"] + 4)

    # Abandoned queued responses must release sockets and buffers instead of filling all slots.
    def test_abandoned_download_releases_connection(self):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        # Exceed the server's 16 connection slots without waiting for their timeout.
        for _ in range(20):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                connection.settimeout(5)
                connection.connect(("127.0.0.1", self.port))
                connection.sendall((f'GET {file["url"]} HTTP/1.1\r\n'
                                    "Host: 127.0.0.1\r\nConnection: close\r\n"
                                    f"X-Recording-Instance: {self.instance_id}\r\n"
                                    f'Range: bytes=0-{file["size"] - 1}\r\n\r\n').encode("ascii"))
                with http.client.HTTPResponse(connection) as response:
                    response.begin()
                    self.assertEqual(206, response.status)
            self.request("GET", "/recording/status")
        # Connection: close must also deliver the whole response when the client reads.
        data = self.request("GET", file["url"], expected=206,
                            headers={"Range": f'bytes=0-{file["size"] - 1}'})
        self.assertEqual(file["size"], len(data))
        self.assertEqual(file["sha256"], hashlib.sha256(data).hexdigest())

    # A whole-file download larger than socket buffers must finish after deletion with engine updates paused.
    def test_download_continues_without_updates(self):
        file = self.prepare_large_recording(16 * 1024 * 1024 + 17)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as connection:
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
            connection.settimeout(5)
            connection.connect(("127.0.0.1", self.port))
            connection.sendall((f'GET {file["url"]} HTTP/1.1\r\n'
                                "Host: 127.0.0.1\r\nConnection: close\r\n"
                                f"X-Recording-Instance: {self.instance_id}\r\n\r\n").encode("ascii"))
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(200, response.status)
                self.assertIsNone(response.getheader("Content-Range"))
                self.request("DELETE", file["url"], expected=204)
                self.request("GET", file["url"], expected=410)
                self.assertEqual(1, len(list(pathlib.Path(self.directory.name).glob("*.mp4"))))
                try:
                    self.request("GET", "/test/pause-updates")
                    self.paused.get(timeout=5)
                    digest = hashlib.sha256()
                    downloaded = 0
                    while chunk := response.read(65536):
                        downloaded += len(chunk)
                        digest.update(chunk)
                    self.assertEqual(file["size"], downloaded)
                    self.assertEqual(file["sha256"], digest.hexdigest())
                    self.assertEqual(b"", connection.recv(1))
                finally:
                    self.process.stdin.write("\n")
                    self.process.stdin.flush()
        self.request("GET", "/recording/status")
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Ordinary Send calls must queue automatically, with more stalled clients than
    # workers, and finish their owned responses even while engine updates are paused.
    def test_all_responses_share_workers(self):
        expected = bytes(range(256)) * 4096
        updates = self.request("GET", "/test/updates")["updates"]
        with contextlib.ExitStack() as stack:
            pending = []
            for _ in range(4):
                connection = stack.enter_context(socket.socket(socket.AF_INET, socket.SOCK_STREAM))
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                connection.settimeout(5)
                connection.connect(("127.0.0.1", self.port))
                connection.sendall(b"GET /test/large-response HTTP/1.1\r\n"
                                   b"Host: 127.0.0.1\r\nConnection: close\r\n\r\n")
                response = stack.enter_context(http.client.HTTPResponse(connection))
                response.begin()
                self.assertEqual(200, response.status)
                pending.append((connection, response))

            self.assertGreater(self.request("GET", "/test/updates")["updates"], updates)
            self.assertEqual(expected, self.request("GET", "/test/large-response"))
            try:
                self.request("GET", "/test/pause-updates")
                self.paused.get(timeout=5)
                # Read the last queued response first while earlier clients stay stalled.
                for connection, response in reversed(pending):
                    self.assertEqual(expected, response.read())
                    self.assertEqual(b"", connection.recv(1))
            finally:
                self.process.stdin.write("\n")
                self.process.stdin.flush()
        self.assertGreater(self.request("GET", "/test/updates")["updates"], updates)

    # Server deletion must deliver its shutdown reply and join the sender even
    # when clients are stalled with unread responses or unfinished rejected uploads.
    def test_shutdown_with_stalled_download(self):
        self.request("GET", "/test/large-recording")
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        file = self.wait_status(complete_file=True)["file"]
        with contextlib.ExitStack() as stack:
            for _ in range(4):
                connection = stack.enter_context(socket.socket(socket.AF_INET, socket.SOCK_STREAM))
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                connection.settimeout(5)
                connection.connect(("127.0.0.1", self.port))
                connection.sendall((f'GET {file["url"]} HTTP/1.1\r\n'
                                    "Host: 127.0.0.1\r\nConnection: close\r\n"
                                    f"X-Recording-Instance: {self.instance_id}\r\n"
                                    f'Range: bytes=0-{file["size"] - 1}\r\n\r\n').encode("ascii"))
                response = stack.enter_context(http.client.HTTPResponse(connection))
                response.begin()
                self.assertEqual(206, response.status)
            upload = stack.enter_context(socket.create_connection(("127.0.0.1", self.port), timeout=5))
            upload.sendall(b"POST /recording/start HTTP/1.1\r\n"
                           b"Host: 127.0.0.1\r\nContent-Length: 2147483647\r\n\r\n")
            response = stack.enter_context(http.client.HTTPResponse(upload))
            response.begin()
            self.assertEqual(413, response.status)
            response.read()
            self.assertEqual(b"", upload.recv(1))
            try:
                type(self).tearDownClass()
            finally:
                type(self).setUpClass()

    # A direct caller must not steal the previous HTTP completion or transfer file ownership.
    def test_shared_recording_ownership(self):
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("GET", "/test/replace-recording")
        status = self.wait_status()
        self.assertTrue(status["active"])
        self.assertIsNone(status["file"])
        self.assertNotEqual(receipt["operation_id"], status["operation_id"])
        expected = b"standalone recording test\n"
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        deadline = time.monotonic() + 10
        try:
            while time.monotonic() < deadline:
                connection.request("GET", receipt["file"]["url"],
                                   headers={"Range": f"bytes=0-{len(expected) - 1}"})
                response = connection.getresponse()
                data = response.read()
                if response.status != 409:
                    self.assertEqual(206, response.status)
                    self.assertEqual(expected, data)
                    break
            else:
                self.fail("The previous HTTP recording was not finalized")
        finally:
            connection.close()
        self.request("POST", "/recording/stop", {"operation_id": status["operation_id"]}, 202)
        self.wait_status()
        self.request("GET", "/test/restart")
        # Service cleanup only deletes its own recording. The caller retains its MP4.
        caller = pathlib.Path(self.directory.name) / "caller.mp4"
        self.assertEqual(expected, caller.read_bytes())
        caller.unlink()

    # A 204 must end at its headers so the next keep-alive response starts with a status line.
    def test_delete_preserves_keep_alive(self):
        receipt = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
        url = self.wait_status(complete_file=True)["file"]["url"]
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
            with connection.makefile("rb") as response:
                connection.sendall((f"DELETE {url} HTTP/1.1\r\n"
                                    "Host: 127.0.0.1\r\n"
                                    f"X-Recording-Instance: {self.instance_id}\r\n\r\n").encode("ascii"))
                self.assertEqual(b"HTTP/1.1 204 No Content\r\n", response.readline())
                headers = http.client.parse_headers(response)
                self.assertIsNone(headers.get("Transfer-Encoding"))
                self.assertIsNone(headers.get("Content-Length"))
                connection.sendall(("GET /recording/status HTTP/1.1\r\n"
                                    "Host: 127.0.0.1\r\n"
                                    f"X-Recording-Instance: {self.instance_id}\r\n"
                                    "Connection: close\r\n\r\n").encode("ascii"))
                self.assertEqual(b"HTTP/1.1 200 OK\r\n", response.readline())
                response.read()
        self.request("GET", url, expected=410)

    # Header casing must not reject valid commands or bypass stale-instance checks.
    def test_case_insensitive_headers(self):
        for instance_name, type_name, range_name in [
            ("x-recording-instance", "content-type", "range"),
            ("x-ReCoRdInG-InStAnCe", "cOnTeNt-TyPe", "rAnGe"),
        ]:
            with self.subTest(instance_name=instance_name):
                headers = {instance_name: self.instance_id, type_name: "application/json"}
                self.request("POST", "/recording/start", "{}", 202, headers, instance=False)
                status = self.wait_status()
                self.assertTrue(status["active"])
                stale = {instance_name: "old-instance"}
                self.request("GET", "/recording/status", expected=410, headers=stale, instance=False)
                body = json.dumps({"operation_id": status["operation_id"]})
                self.request("POST", "/recording/stop", body, 202, headers, instance=False)
                status = self.wait_status(complete_file=True)
                url = status["file"]["url"]
                download_headers = {instance_name: self.instance_id,
                                    range_name: f'bytes=0-{status["file"]["size"] - 1}'}
                data = self.request("GET", url, expected=206, headers=download_headers, instance=False)
                self.assertEqual(b"standalone recording test\n", data)
                self.request("DELETE", url, expected=410, headers=stale, instance=False)
                self.request("DELETE", url, expected=204,
                             headers={instance_name: self.instance_id}, instance=False)

    # Oversized requests must return a complete 413 and close without waiting for their bodies.
    def test_oversized_body_rejected_without_drain(self):
        for method, path, size, prefix in [
            ("POST", "/recording/start", 1024, b""),
            ("POST", "/recording/start", 1024, b" " * 1024),
            ("POST", "/recording/start", 65536, b" " * 65536),
            ("POST", "/recording/stop", 0x7fffffff, b"{"),
            ("GET", "/recording/capabilities", 1024, b""),
            ("DELETE", "/recording/files/missing", 1024, b""),
        ]:
            with self.subTest(method=method, path=path, size=size, prefix_size=len(prefix)):
                with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
                    connection.sendall((f"{method} {path} HTTP/1.1\r\n"
                                        "Host: 127.0.0.1\r\n"
                                        f"Content-Length: {size}\r\n\r\n").encode("ascii") + prefix)
                    response = http.client.HTTPResponse(connection)
                    response.begin()
                    self.assertEqual(413, response.status)
                    self.assertEqual("close", response.getheader("Connection"))
                    self.assertEqual("body_too_large", json.loads(response.read())["error"]["code"])
                    self.assertEqual(b"", connection.recv(1))
                self.assertFalse(self.request("GET", "/recording/status")["active"])

    # Upload bytes already in flight must not reset a 413, even after its write-side FIN.
    def test_oversized_body_in_flight(self):
        body = b" " * (256 * 1024)
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
            connection.sendall(("POST /recording/start HTTP/1.1\r\n"
                                "Host: 127.0.0.1\r\n"
                                f"Content-Length: {len(body)}\r\n\r\n").encode("ascii"))
            with http.client.HTTPResponse(connection) as response:
                response.begin()
                self.assertEqual(413, response.status)
                self.assertEqual("body_too_large", json.loads(response.read())["error"]["code"])
                self.assertEqual(b"", connection.recv(1))
                # Synchronize on an engine update after the response, then keep
                # uploading while updates are paused to exercise the worker.
                self.request("GET", "/test/pause-updates")
                try:
                    self.paused.get(timeout=5)
                    connection.sendall(body)
                    connection.shutdown(socket.SHUT_WR)
                finally:
                    self.process.stdin.write("\n")
                    self.process.stdin.flush()
        self.assertFalse(self.request("GET", "/recording/status")["active"])

    # Rejected uploads must expire on a fixed deadline even if more bytes keep arriving.
    def test_rejected_upload_times_out(self):
        with self.server_timeout(2):
            with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
                connection.sendall(b"POST /recording/start HTTP/1.1\r\n"
                                   b"Host: 127.0.0.1\r\nContent-Length: 2147483647\r\n\r\n")
                with http.client.HTTPResponse(connection) as response:
                    response.begin()
                    self.assertEqual(413, response.status)
                    response.read()
                    self.assertEqual(b"", connection.recv(1))
                    deadline = time.monotonic() + 5
                    while time.monotonic() < deadline:
                        try:
                            connection.sendall(b" " * 1024)
                        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                            # Winsock may report a local abort after the close deadline.
                            break
                        self.request("GET", "/recording/capabilities")
                    else:
                        self.fail("Rejected upload kept its connection alive past the close deadline")

    # Incomplete small bodies must yield to other requests, retain their headers, and resume normally.
    def test_partial_body_does_not_block_updates(self):
        for body, prefix_size in [(b"{}", 0), (b"{}", 1), (b'{"fps":30}' + b" " * 1013, 1)]:
            with self.subTest(size=len(body), prefix_size=prefix_size):
                with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
                    connection.sendall(("POST /recording/start HTTP/1.1\r\n"
                                        "Host: 127.0.0.1\r\n"
                                        "Content-Type: application/json\r\n"
                                        f"X-Recording-Instance: {self.instance_id}\r\n"
                                        f"Content-Length: {len(body)}\r\n\r\n").encode("ascii") + body[:prefix_size])
                    try:
                        # A completed request is the progress signal; no sleep is needed.
                        self.request("GET", "/recording/capabilities",
                                     headers={"X-Recording-Instance": "unrelated", "Content-Type": "text/plain"})
                    finally:
                        connection.sendall(body[prefix_size:])
                    response = http.client.HTTPResponse(connection)
                    response.begin()
                    self.assertEqual(202, response.status)
                    receipt = json.loads(response.read())
                    # The completed request must leave the connection reusable.
                    connection.sendall(b"GET /recording/capabilities HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                    response = http.client.HTTPResponse(connection)
                    response.begin()
                    self.assertEqual(200, response.status)
                    self.assertEqual(self.instance_id, json.loads(response.read())["instance_id"])
                self.wait_status()
                self.request("POST", "/recording/stop", {"operation_id": receipt["operation_id"]}, 202)
                self.wait_status(complete_file=True)

    # Fragmented headers must also yield rather than blocking before the recording handler runs.
    def test_partial_headers_do_not_block_updates(self):
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
            connection.sendall(b"GET /recording/capabilities HTTP/1.1\r\nHost: 127.0.0.1\r\n")
            try:
                self.request("GET", "/recording/capabilities")
            finally:
                connection.sendall(b"\r\n")
            response = http.client.HTTPResponse(connection)
            response.begin()
            self.assertEqual(200, response.status)
            self.assertEqual(self.instance_id, json.loads(response.read())["instance_id"])

    # Disconnecting mid-body must release the connection instead of spinning on end-of-stream.
    def test_abandoned_body_releases_connection(self):
        for _ in range(20):
            with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
                connection.sendall(("POST /recording/start HTTP/1.1\r\n"
                                    "Host: 127.0.0.1\r\n"
                                    "Content-Length: 2\r\n\r\n{").encode("ascii"))
                self.request("GET", "/recording/capabilities")
                connection.shutdown(socket.SHUT_RDWR)
            self.assertFalse(self.request("GET", "/recording/status")["active"])

    # Reject invalid JSON, unknown/duplicate keys and invalid bounds before starting capture.
    def test_request_validation(self):
        invalid = [[], {"path": "/tmp/unowned.mp4"}, {"fps": 0}, {"fps": 61}, {"width": 10},
                   {"height": 240}, {"width": 640, "height": 360}, {"scale": 0.5}, {"audio": "true"},
                   {"fps": None}, {"fps": True}, {"fps": {"value": 30}}]
        for value in invalid:
            with self.subTest(value=value):
                self.request("POST", "/recording/start", json.dumps(value), 400, {"Content-Type": "application/json"})
        for body in ['{"fps":30,"fps":40}', '{"fps":30,"\\u0066ps":40}', '{"fps":01}',
                     '{"fps":1.5}', '{"fps":1,}', '{', 'null', '{} trailing', '{}\0',
                     '{"fps":18446744073709551616}']:
            with self.subTest(body=body):
                self.request("POST", "/recording/start", body, 400, {"Content-Type": "application/json"})
        self.request("POST", "/recording/start", "{}", 415, {"Content-Type": "text/plain"})
        self.request("POST", "/recording/start", " " * 1024, 413)
        self.request("POST", "/recording/stop", {}, 400)
        self.request("POST", "/recording/start?fps=30", {}, 400)
        self.request("GET", "/recording/start", expected=405)
        self.request("GET", "/recording/missing", expected=404)
        self.request("GET", "/recording/status", "{}", 400)
        self.assertFalse(self.request("GET", "/recording/status")["active"])
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Capability checks reject unsupported capture/audio and missing windows without reserving files.
    def test_capabilities(self):
        caps = self.request("GET", "/recording/capabilities")
        self.assertTrue(caps["supported"])
        self.assertFalse(caps["audio"])
        self.request("POST", "/recording/start", {"audio": True}, 501)
        self.request("GET", "/test/no-window")
        self.assertFalse(self.request("GET", "/recording/capabilities")["supported"])
        self.request("POST", "/recording/start", {}, 500)
        self.request("GET", "/test/unsupported")
        self.request("POST", "/recording/start", {}, 501)
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

    # Audio-capable backends default to audio and preserve explicit opt-in/opt-out in receipts.
    def test_audio_options(self):
        self.request("GET", "/test/audio-support")
        self.assertTrue(self.request("GET", "/recording/capabilities")["audio"])
        for options, expected_audio in [({}, True), ({"audio": True}, True), ({"audio": False}, False)]:
            with self.subTest(options=options):
                self.request("POST", "/recording/start", options, 202)
                status = self.wait_status()
                self.assertTrue(status["active"])
                self.assertEqual(expected_audio, status["audio"])
                self.request("POST", "/recording/stop", {"operation_id": status["operation_id"]}, 202)
                status = self.wait_status(complete_file=True)
                self.assertTrue(status["finalized"])
                self.assertEqual(expected_audio, status["audio"])

    # Failed starts release partial files; reboot invalidates receipts and cleans active recordings.
    def test_failure_and_restart_cleanup(self):
        self.request("GET", "/test/fail-start")
        self.request("POST", "/recording/start", {}, 202)
        status = self.wait_status()
        self.assertEqual("failed", status["state"])
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))
        self.request("GET", "/test/restart")
        type(self).instance_id = self.request("GET", "/recording/capabilities")["instance_id"]
        old = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.request("GET", "/test/restart")
        type(self).instance_id = self.request("GET", "/recording/capabilities")["instance_id"]
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))
        current = self.request("POST", "/recording/start", {}, 202)
        self.assertGreater(current["operation_id"], old["operation_id"])
        self.request("POST", "/recording/stop", {"operation_id": old["operation_id"]}, 410)
        self.request("GET", old["file"]["url"], expected=410)

    # Mutations require recording-owned identity; reboots invalidate it independently of operation IDs.
    def test_instance_preconditions(self):
        original = self.instance_id
        self.assertTrue(original)
        self.assertEqual(original, self.request("GET", "/recording/status")["instance_id"])
        missing = self.request("POST", "/recording/start", {}, 400, instance=False)
        self.assertEqual("missing_instance", missing["error"]["code"])
        self.request("GET", "/test/restart")
        type(self).instance_id = self.request("GET", "/recording/capabilities")["instance_id"]
        self.assertNotEqual(original, self.instance_id)
        stale = {"X-Recording-Instance": original}
        self.assertEqual(self.instance_id, self.request("GET", "/recording/capabilities", headers=stale)["instance_id"])
        self.request("GET", "/recording/status", expected=410, headers=stale)
        result = self.request("POST", "/recording/start", {}, 410, stale)
        self.assertEqual("stale_instance", result["error"]["code"])
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))
        # Only the recording identity is checked, independently of automation identity.
        current = self.request("POST", "/recording/start", {}, 202, {"X-Automation-Runtime": "unrelated"})
        self.assertEqual(self.instance_id, current["instance_id"])
        self.wait_status()
        body = {"operation_id": current["operation_id"]}
        self.request("POST", "/recording/stop", body, 400, instance=False)
        self.request("POST", "/recording/stop", body, 410, stale)
        self.assertTrue(self.request("GET", "/recording/status")["active"])
        self.request("POST", "/recording/stop", body, 202)
        status = self.wait_status(complete_file=True)
        url = status["file"]["url"]
        self.request("DELETE", url, expected=400, instance=False)
        self.request("DELETE", url, expected=410, headers=stale)
        self.request("GET", url, expected=206, headers={"Range": f'bytes=0-{status["file"]["size"] - 1}'})
        self.request("DELETE", url, expected=204)

    # Reused counters in replacement processes must not let an old receipt stop a new recording.
    def test_stale_instance_after_process_restart(self):
        cls = type(self)
        cls.tearDownClass()
        cls.setUpClass()
        old = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        cls.tearDownClass()
        cls.setUpClass()
        current = self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.assertEqual(old["operation_id"], current["operation_id"])
        self.assertNotEqual(old["instance_id"], current["instance_id"])
        response = self.request("POST", "/recording/stop", {"operation_id": old["operation_id"]}, 410,
                                {"X-Recording-Instance": old["instance_id"]})
        self.assertEqual("stale_instance", response["error"]["code"])
        self.assertTrue(self.request("GET", "/recording/status")["active"])
        self.request("POST", "/recording/stop", {"operation_id": current["operation_id"]}, 202)
        self.assertTrue(self.wait_status(complete_file=True)["finalized"])

    # The registered ENGINE_DELETE callback joins capture before teardown; later finalizers are harmless.
    def test_engine_delete_finalizes_before_teardown(self):
        self.request("POST", "/recording/start", {}, 202)
        self.wait_status()
        self.assertEqual({"active": True, "finalizes": 0, "late_finalizes": 0},
                         self.request("GET", "/test/deactivate"))
        expected = {"active": False, "finalizes": 1, "late_finalizes": 0}
        self.assertEqual(expected, self.request("GET", "/test/engine-delete"))
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))
        self.assertEqual({}, self.request("GET", "/openapi.json")["paths"])
        self.request("GET", "/recording/status", expected=404)
        self.assertEqual(expected, self.request("GET", "/test/finalize"))
        self.assertEqual(expected, self.request("GET", "/test/finalize"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, remaining = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[__file__, *remaining])
