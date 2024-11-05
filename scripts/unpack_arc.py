#!/usr/bin/env python3
# Copyright 2020-2024 The Defold Foundation
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

import os, sys

try:
    from google.protobuf import text_format
except:
    dynamo_home = os.environ.get('DYNAMO_HOME')
    sys.path.append(os.path.join(dynamo_home, "lib", "python"))
    sys.path.append(os.path.join(dynamo_home, "ext", "lib", "python"))

import struct
import hashlib
import optparse
import lz4.block
import resource.liveupdate_ddf_pb2

import traceback
import binascii
import json

XTEA_BLOCK_SIZE = 8
def xtea_decrypt(v, key, n=32):
    v0 = (v >> 32) & 0xFFFFFFFF;
    v1 = (v >> 0) & 0xFFFFFFFF;
    k = struct.unpack(">4L", key)
    sum = 0
    delta = 0x9e3779b9
    for round in range(n):
        v0 = (v0 + ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]))) & 0xFFFFFFFF;
        sum = (sum + delta) & 0xFFFFFFFF;
        v1 = (v1 + ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]))) & 0xFFFFFFFF;
    return struct.pack(">2L", v0, v1)
def xtea_decryptCTR(key, data, n=32):
    global XTEA_BLOCK_SIZE
    counter = 0;
    for i in range(0, len(data), XTEA_BLOCK_SIZE):
        enc_counter = xtea_decrypt(counter, key, n)
        for j in range(0, XTEA_BLOCK_SIZE):
            if i+j >= len(data):
                break
            data[i+j] = (data[i+j] ^ enc_counter[j]) & 0xFFFFFFFF
        counter += 1
    return data

if __name__ == "__main__":
    usage = '''usage: %prog [options] input
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--uncompress', dest='uncompress',
                      default = False,
                      help = 'Whether to automatically uncompress files')

    options, args = parser.parse_args()

    resources = args[0]
    output = args[1]
    os.makedirs(output, 0o777, True)

    if os.path.isdir(resources):
        with open(os.path.join(resources, "game.projectc"), "rb") as f:
            project = f.read()
        with open(os.path.join(resources, "game.dmanifest"), "rb") as f:
            manifest = f.read()
        with open(os.path.join(resources, "game.arcd"), "rb") as f:
            arcd = bytearray(f.read())
        with open(os.path.join(resources, "game.arci"), "rb") as f:
            arci = f.read()
    else:
        def gather_pieces(basedir, contents, name):
            for content in contents:
                if content['name'] == name:
                    data = bytearray(content['size'])
                    mv = memoryview(data)
                    for piece in content['pieces']:
                        piece_path = os.path.join(basedir, piece['name'])
                        with open(piece_path, 'rb') as d:
                            r = d.readinto(mv[piece['offset']:])
                    return data
            return None
        with open(resources, 'r') as f:
            obj = json.load(f)
            project = gather_pieces(os.path.dirname(resources), obj['content'], "game.projectc")
            manifest = gather_pieces(os.path.dirname(resources), obj['content'], "game.dmanifest")
            arcd = gather_pieces(os.path.dirname(resources), obj['content'], "game.arcd")
            arci = gather_pieces(os.path.dirname(resources), obj['content'], "game.arci")

    with open(output + "/game.projectc", "wb") as o:
        o.write(project)

    hash_map = {}
    ddf = resource.liveupdate_ddf_pb2.ManifestFile()
    ddf.ParseFromString(manifest)
    for descriptor in ddf.DESCRIPTOR.fields:
        value = getattr(ddf, descriptor.name)
        if descriptor.full_name == "dmLiveUpdateDDF.ManifestFile.data":
            data = resource.liveupdate_ddf_pb2.ManifestData()
            data.MergeFromString(value)
            resources = getattr(data, "resources")
            for r in resources:
                hash = getattr(getattr(r, "hash"), "data")
                url = getattr(r, "url")
                hash_map[url] = hash;

    entryCount = struct.unpack_from(">i", arci, 16)[0]
    entryOffset = struct.unpack_from(">i", arci, 20)[0]
    hashesOffset = struct.unpack_from(">i", arci, 24)[0]
    hashLength = struct.unpack_from(">i", arci, 28)[0]
    for i in range(0, entryCount):
        hashOffset = hashesOffset+(i*64)
        hash = arci[hashOffset:hashOffset+hashLength]
        for url, h in hash_map.items():
            if hash == h:
                offset = struct.unpack_from(">i", arci, entryOffset + (i * 16) + 0)[0]
                uncompressed_size = struct.unpack_from(">i", arci, entryOffset + (i * 16) + 4)[0]
                compressed_size = struct.unpack_from(">i", arci, entryOffset + (i * 16) + 8)[0]
                flags = struct.unpack_from(">i", arci, entryOffset + (i * 16) + 12)[0]
                size = compressed_size
                if compressed_size == -1:
                    size = uncompressed_size
                #print("Index found %s %d-%d" % (url, offset, size))

                data = arcd[offset:offset+size]
                try:
                    if flags & 1:
                        #print("encrypted")
                        xtea_decryptCTR(bytearray(b'aQj8CScgNP4VsfXK'), data)
                    if compressed_size != -1:
                        if options.uncompress:
                            data = lz4.block.decompress(data, uncompressed_size)
                        else:
                            url += ".lz4";

                except Exception as e:
                    print("Failed: ", e)
                    print(traceback.format_exc())

                output_file = output + url
                os.makedirs(os.path.dirname(output_file), 0o777, True)
                with open(output_file, "wb") as o:
                    o.write(data)
