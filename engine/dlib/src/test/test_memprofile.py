import unittest
import memprofile

class TestDlib(unittest.TestCase):

    def testMemprofile(self):
        profile = memprofile.load('memprofile.trace', 'build/default/src/test/test_memprofile')

        for k, s in profile.summary.iteritems():
            tmp = str(s.back_trace)
            if 'func1a' in tmp and 'func2' in tmp:
                self.assertEqual(16 * 8, s.nmalloc)
                self.assertTrue(16 * 16 * 8 <= s.malloc_total)
                self.assertTrue(16 * 24 * 8 >= s.malloc_total)
            elif 'func1a' in tmp and not 'func2' in tmp:
                self.assertEqual(16, s.nmalloc)
                self.assertTrue(16 * 512 <= s.malloc_total)
                self.assertTrue(16 * 532 >= s.malloc_total)
            elif 'func1b' in tmp and 'func2' in tmp:
                self.assertEqual(16 * 8, s.nmalloc)
                self.assertTrue(16 * 16 * 8 <= s.malloc_total)
                self.assertTrue(16 * 24 * 8 >= s.malloc_total)
            elif 'func1b' in tmp and not 'func2' in tmp:
                self.assertEqual(16, s.nmalloc)
                self.assertTrue(16 * 256 <= s.malloc_total)
                self.assertTrue(16 * 280 >= s.malloc_total)

if __name__ == '__main__':
    unittest.main()
