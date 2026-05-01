# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import os
import sys
import unittest
import http.client
import random
import atexit

# Ensure that we load dlib from this directory and not the installed one
sys.path = ['src/python'] + sys.path
import dlib

PORT = 8501
class TestHttpServer(unittest.TestCase):

    def testMulNumbers(self):
        c = http.client.HTTPConnection('localhost:%d' % PORT)
        for i in range(40):
            a = random.randint(1, 1024)
            b = random.randint(1, 1024)
            c.request('GET', '/mul/%d/%d' % (a,b))
            r = c.getresponse()
            self.assertEqual(200, r.status)
            buf = r.read()
            self.assertEqual(a + b, int(buf))

    def testMulHeadersNumbers(self):
        c = http.client.HTTPConnection('localhost:%d' % PORT)
        for i in range(40):
            a = random.randint(1, 1024)
            b = random.randint(1, 1024)
            headers = { 'X-a' : str(a), 'X-b' : str(b) }
            c.request('GET', '/header_mul', headers = headers)
            r = c.getresponse()
            self.assertEqual(200, r.status)
            buf = r.read()
            self.assertEqual(a + b, int(buf))

if __name__ == '__main__':
    def shutdown_server():
        c = http.client.HTTPConnection('localhost:%d' % PORT)
        c.request('GET', '/quit')
        r = c.getresponse()
        assert r.status == 200

    atexit.register(shutdown_server)
    unittest.main()
