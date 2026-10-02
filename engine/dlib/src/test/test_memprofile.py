import unittest
import memprofile

def debug_print(profile):
    print("MAWE DEBUG: (remove once we've fixed the issue!)")

    print("    SYMBOLS")
    for k, s in profile.symbol_table.items():
        print("%lu\t\t%s" % (k, s))

    print("    TRACES")
    for k, t in profile.traces.items():
        print("  %s\t%s" % (k, t))

    print("    SUMMARY")
    for k, s in profile.summary.items():
        print("  %s\t%s" % (k, str(s)) )


class TestDlib(unittest.TestCase):

    def testMemprofile(self):
        profile = memprofile.load('memprofile.trace', 'build/src/test/test_memprofile')

        try:
            for k, s in profile.summary.items():
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
        except:
            debug_print(profile)
            raise

if __name__ == '__main__':
    unittest.main()
