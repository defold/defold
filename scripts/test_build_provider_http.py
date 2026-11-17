import importlib.util
from pathlib import Path
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "engine/resource/src/test/test_provider_http.py"
SPEC = importlib.util.spec_from_file_location("test_provider_http", SCRIPT)
provider_http = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(provider_http)


class ProviderHttpServerTests(unittest.TestCase):
    # The loopback fixture must bind an ephemeral port without relying on hostname resolution.
    def test_loopback_server_starts_without_hostname_lookup(self):
        with mock.patch("socket.getfqdn", side_effect=AssertionError("Unexpected hostname lookup")):
            with provider_http.TimedHTTPServer(("127.0.0.1", 0), provider_http.Handler) as server:
                self.assertEqual("127.0.0.1", server.server_name)
                self.assertGreater(server.server_port, 0)
                self.assertEqual((server.server_name, server.server_port), server.socket.getsockname())


if __name__ == "__main__":
    unittest.main()
