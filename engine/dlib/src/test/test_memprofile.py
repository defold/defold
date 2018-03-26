import unittest
import memprofile

class TestDlib(unittest.TestCase):

    def testMemprofile(self):
        profile = memprofile.load('memprofile.trace', 'build/default/src/test/test_memprofile')

        print("MAWE DEBUG: (remove once we've fixed the issue!)")

        print("    SYMBOLS")
        for k, s in profile.symbol_table.iteritems():
            print("%lu\t\t%s" % (k, s))

        print("    TRACES")
        for k, t in profile.traces.iteritems():
            print("  %s\t%s" % (k, t))

        print("    SUMMARY")
        for k, s in profile.summary.iteritems():
            print("  %s\t%s" % (k, str(s)) )

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
