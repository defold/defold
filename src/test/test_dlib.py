import unittest
import dlib

class TestDlib(unittest.TestCase):
    
    def testHash(self):
        h1 = dlib.dmHashBuffer32("foo")
        h2 = dlib.dmHashBuffer64("foo")
        
        self.assertEqual(0xe18f6896, h1) 
        self.assertEqual(0xe18f6896a8365dfbL, h2)

if __name__ == '__main__':
    unittest.main()
    