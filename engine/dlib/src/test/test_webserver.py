import os
import sys
import unittest
import httplib
import random
import atexit

# Ensure that we load dlib from this directory and not the installed one
sys.path = ['src/python'] + sys.path
import dlib

PORT = 8501
class TestHttpServer(unittest.TestCase):

    def testMulNumbers(self):
        c = httplib.HTTPConnection('localhost:%d' % PORT)
        for i in range(40):
            a = random.randint(1, 1024)
            b = random.randint(1, 1024)
            c.request('GET', '/mul/%d/%d' % (a,b))
            r = c.getresponse()
            self.assertEqual(200, r.status)
            buf = r.read()
            self.assertEqual(a + b, int(buf))

    def testMulHeadersNumbers(self):
        c = httplib.HTTPConnection('localhost:%d' % PORT)
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
        c = httplib.HTTPConnection('localhost:%d' % PORT)
        c.request('GET', '/quit')
        r = c.getresponse()
        assert r.status == 200

    atexit.register(shutdown_server)
    unittest.main()
