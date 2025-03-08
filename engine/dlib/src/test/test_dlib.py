# Copyright 2020-2025 The Defold Foundation
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
import dlib
from os import path

class TestDlib(unittest.TestCase):

    def testHash(self):
        h1 = dlib.dmHashBuffer32("foo")
        h2 = dlib.dmHashBuffer64("foo")

        self.assertEqual(0xd861e2f7L, h1)
        self.assertEqual(0x97b476b3e71147f7L, h2)

    def testLZ4(self):
        #
        # Test to compress/decompress a short text...
        #
        uncompressed = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas ornare non massa a imperdiet. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus lorem diam, ultricies et dapibus vel, interdum consequat metus. Cras suscipit bibendum dignissim. Maecenas non lectus enim. Integer rhoncus fringilla felis in dictum. Nunc justo risus, volutpat id elementum molestie, eleifend sit amet tellus. Mauris accumsan ornare justo, sed cursus justo bibendum et. Vestibulum massa metus, rutrum id libero a, ultrices varius arcu. Sed sed condimentum enim. Mauris ac sodales lacus, at lacinia est. Nunc sed libero ac neque cursus venenatis. Phasellus laoreet est quis massa cursus."
        uncompressed_len = len(uncompressed)
        max_compressed_size = dlib.dmLZ4MaxCompressedSize(uncompressed_len)
        self.assertGreater(max_compressed_size, 0)

        compressed = dlib.dmLZ4CompressBuffer(uncompressed, uncompressed_len, max_compressed_size)
        decompressed = dlib.dmLZ4DecompressBuffer(compressed, uncompressed_len)
        self.assertEqual(decompressed, uncompressed)

        #
        # Test to decompress lz4 encoded file
        #
        foo_file_size = path.getsize("data/foo.lz4")
        foo_f = open("data/foo.lz4", "rb")
        foo_compressed = foo_f.read(foo_file_size)
        foo_f.close()
        foo_decompressed = dlib.dmLZ4DecompressBuffer(foo_compressed, 3)
        self.assertEqual(foo_decompressed, "foo")

    def testCrypt(self):
        key = "12345678abcdefgh"
        s = "ABCDEFGH12345678XYZ"
        enc = dlib.dmEncryptXTeaCTR(s, key)
        self.assertEqual("\x81\xb4\xa1\x04\x2d\xac\xe5\xcb\x77\x89\xec\x11\x61\xc3\xdc\xfa\xb9\xa3\x25", enc)
        dec = dlib.dmDecryptXTeaCTR(enc, key)
        self.assertEqual(s, dec)

if __name__ == '__main__':
    unittest.main()
