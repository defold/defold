import os
import unittest
import httplib
import random
import atexit

class TestHttpServer(unittest.TestCase):

    def testDifferentResponseLengths(self):
        c = httplib.HTTPConnection('localhost:8500')
        for i in range(40):
            l = random.randint(1, 1024)
            c.request('GET', '/respond_with_n/%d' % l)
            r = c.getresponse()
            self.assertEqual(200, r.status)
            buf = r.read()
            self.assertEqual(l, len(buf))
            for i, x in enumerate(buf):
                char = ord('a') + ((i*97 + l) % (ord('z') - ord('a')))
                self.assertEqual(x, chr(char))

    def testMulNumbers(self):
        c = httplib.HTTPConnection('localhost:8500')
        for i in range(40):
            a = random.randint(1, 1024)
            b = random.randint(1, 1024)
            c.request('GET', '/mul/%d/%d' % (a,b))
            r = c.getresponse()
            self.assertEqual(200, r.status)
            buf = r.read()
            self.assertEqual(a + b, int(buf))

    def testContentType(self):
        c = httplib.HTTPConnection('localhost:8500')
        for i in range(40):
            c.request('GET', '/test_html')
            r = c.getresponse()
            self.assertEqual(200, r.status)
            self.assertEqual('text/html', r.getheader('Content-Type'))
            buf = r.read()

    def test404(self):
        c = httplib.HTTPConnection('localhost:8500')
        for i in range(10):
            c.request('GET', '/does_not_exists')
            r = c.getresponse()
            self.assertEqual(404, r.status)
            tmp = r.read()
        c.close()

if __name__ == '__main__':
    def shutdown_server():
        c = httplib.HTTPConnection('localhost:8500')
        c.request('GET', '/quit')

    atexit.register(shutdown_server)
    unittest.main()
