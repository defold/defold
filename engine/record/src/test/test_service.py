"""Real HTTP acceptance for the standalone recording service with a controlled backend."""

import argparse
import hashlib
import http.client
import json
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
    def setUpClass(cls):
        # Exercise start, hashing, download and cleanup in a non-ASCII storage path.
        cls.directory = tempfile.TemporaryDirectory(prefix="defold-record-test-é-录像-")
        cls.log = tempfile.TemporaryFile(mode="w+")
        cls.process = subprocess.Popen([SERVER, cls.directory.name], stdout=subprocess.PIPE,
                                       stderr=cls.log, text=True, bufsize=1)
        ready = queue.Queue()
        output = []

        def read_output():
            for line in cls.process.stdout:
                output.append(line)
                if line.startswith("RECORD_TEST_PORT="):
                    ready.put(int(line.split("=", 1)[1]))
                    return
            ready.put(None)

        threading.Thread(target=read_output, daemon=True).start()
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

    # Start/stop receipts, bounded downloads, checksums and deletion form a complete service.
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
        self.request("GET", url, expected=416)
        self.request("GET", url, expected=416, headers={"Range": "bytes=0-9999"})
        data = self.request("GET", url, expected=206, headers={"Range": f'bytes=0-{status["file"]["size"] - 1}'})
        self.assertEqual(b"standalone recording test\n", data)
        self.assertEqual(hashlib.sha256(data).hexdigest(), status["file"]["sha256"])
        self.request("DELETE", url, expected=204)
        self.request("GET", url, expected=410)
        self.assertEqual([], list(pathlib.Path(self.directory.name).iterdir()))

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
