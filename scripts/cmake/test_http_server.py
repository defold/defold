# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import http.client
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'engine/script'))

import test_script_server


class HttpServerTests(unittest.TestCase):
    # Binding must not wait for reverse DNS, and port zero must report the actual assigned port.
    def test_bind_does_not_resolve_a_hostname(self):
        with mock.patch('socket.getfqdn', side_effect=AssertionError('Unexpected FQDN lookup')), \
             mock.patch('socket.gethostbyaddr', side_effect=AssertionError('Unexpected reverse DNS lookup')):
            for host in ('127.0.0.1', 'localhost'):
                with self.subTest(host=host):
                    with test_script_server.ThreadedHTTPServer((host, 0), test_script_server.Handler) as server:
                        self.assertEqual('127.0.0.1', server.server_address[0])
                        self.assertEqual(server.server_address[0], server.server_name)
                        self.assertEqual(server.server_address[1], server.server_port)
                        self.assertGreater(server.server_port, 0)

    # Bypassing HTTPServer.server_bind must preserve the request behavior used by script and gamesys tests.
    def test_server_handles_http_requests(self):
        server = test_script_server.Server(ip='127.0.0.1', port=0)
        connection = http.client.HTTPConnection('127.0.0.1', server.server.server_port, timeout=5)
        server.start()
        try:
            with mock.patch.object(test_script_server.Handler, 'log_message'):
                connection.request('GET', '/', headers={'X-A': 'A', 'X-B': 'B'})
                response = connection.getresponse()
                self.assertEqual(200, response.status)
                self.assertEqual('Dynamo 1.0', response.getheader('Server'))
                self.assertEqual(b'Hello AB', response.read())

                for method, expected in (('POST', b'PONGpayload'), ('PUT', b'PONG_PUTpayload')):
                    connection.request(method, '/', body=b'payload')
                    response = connection.getresponse()
                    self.assertEqual(200, response.status)
                    self.assertEqual(expected, response.read())

                connection.request('HEAD', '/')
                response = connection.getresponse()
                self.assertEqual(200, response.status)
                self.assertEqual('1234', response.getheader('Content-Length'))
                self.assertEqual(b'', response.read())
        finally:
            connection.close()
            server.stop()
            server.join(timeout=5)
            server.server.server_close()
        self.assertFalse(server.is_alive())


if __name__ == '__main__':
    unittest.main()
