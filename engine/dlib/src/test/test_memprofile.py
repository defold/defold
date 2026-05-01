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
