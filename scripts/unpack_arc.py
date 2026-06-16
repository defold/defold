#!/usr/bin/env python3
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
ARCHIVE_V6_FLAGS_SHIFT = 60
ARCHIVE_V6_OFFSET_MASK = 0x0fffffffffffffff

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
    usage = '''usage: %prog [options] input output_dir

    input - May be either a directory containing game.arc* files or
            a json file containing path mappings using these keys

                "game.projectc"
                "game.dmanifest"
                "game.arcd"
                "game.arci"

    output_dir - A directory where all unpacked files end up
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--uncompress', dest='uncompress',
                      default = False,
                      help = 'Whether to automatically uncompress files')

    options, args = parser.parse_args()

    if len(args) < 2:
        parser.print_help()
        sys.exit(1)

    resources = args[0]
    if len(args) > 1:
        output = args[1]
        os.makedirs(output, 0o777, True)
    else:
        output = False

    arcd_path = None
    arcd_content = None
    arcd_basedir = None
    if os.path.isdir(resources):
        with open(os.path.join(resources, "game.projectc"), "rb") as f:
            project = f.read()
        with open(os.path.join(resources, "game.dmanifest"), "rb") as f:
            manifest = f.read()
        arcd_path = os.path.join(resources, "game.arcd")
        with open(os.path.join(resources, "game.arci"), "rb") as f:
            arci = f.read()
    else:
        def find_content(contents, name):
            for content in contents:
                if content['name'] == name:
                    return content
            return None

        def gather_pieces(basedir, contents, name):
            content = find_content(contents, name)
            if content:
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
            arcd_basedir = os.path.dirname(resources)
            project = gather_pieces(arcd_basedir, obj['content'], "game.projectc")
            manifest = gather_pieces(arcd_basedir, obj['content'], "game.dmanifest")
            arcd_content = find_content(obj['content'], "game.arcd")
            arci = gather_pieces(arcd_basedir, obj['content'], "game.arci")

    if output:
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

    archiveVersion = struct.unpack_from(">I", arci, 0)[0]
    entryCount = struct.unpack_from(">I", arci, 16)[0]
    entryOffset = struct.unpack_from(">I", arci, 20)[0]
    hashesOffset = struct.unpack_from(">I", arci, 24)[0]
    hashLength = struct.unpack_from(">I", arci, 28)[0]
    if archiveVersion == 5:
        entrySize = 16
    elif archiveVersion == 6:
        entrySize = 16
    else:
        raise Exception("Unsupported archive index version: %d" % archiveVersion)

    def read_arcd(offset, size):
        if arcd_path:
            with open(arcd_path, "rb") as f:
                f.seek(offset)
                return bytearray(f.read(size))
        if arcd_content:
            data = bytearray(size)
            mv = memoryview(data)
            pieces = arcd_content['pieces']
            end = offset + size
            for i, piece in enumerate(pieces):
                piece_offset = piece['offset']
                if i + 1 < len(pieces):
                    piece_end = pieces[i + 1]['offset']
                else:
                    piece_end = arcd_content['size']

                read_offset = max(offset, piece_offset)
                read_end = min(end, piece_end)
                if read_offset >= read_end:
                    continue

                piece_path = os.path.join(arcd_basedir, piece['name'])
                with open(piece_path, "rb") as f:
                    f.seek(read_offset - piece_offset)
                    f.readinto(mv[read_offset - offset:read_end - offset])
            return data
        raise Exception("No game.arcd data found")

    for i in range(0, entryCount):
        hashOffset = hashesOffset+(i*64)
        hash = arci[hashOffset:hashOffset+hashLength]
        for url, h in hash_map.items():
            if hash == h:
                currentEntryOffset = entryOffset + (i * entrySize)
                if archiveVersion == 5:
                    offset = struct.unpack_from(">I", arci, currentEntryOffset + 0)[0]
                    uncompressed_size = struct.unpack_from(">I", arci, currentEntryOffset + 4)[0]
                    compressed_size = struct.unpack_from(">I", arci, currentEntryOffset + 8)[0]
                    flags = struct.unpack_from(">I", arci, currentEntryOffset + 12)[0]
                else:
                    offset_and_flags = struct.unpack_from(">Q", arci, currentEntryOffset + 0)[0]
                    offset = offset_and_flags & ARCHIVE_V6_OFFSET_MASK
                    flags = offset_and_flags >> ARCHIVE_V6_FLAGS_SHIFT
                    uncompressed_size = struct.unpack_from(">I", arci, currentEntryOffset + 8)[0]
                    compressed_size = struct.unpack_from(">I", arci, currentEntryOffset + 12)[0]
                size = compressed_size
                if compressed_size == 0xFFFFFFFF:
                    size = uncompressed_size
                #print("Index found %s %d-%d" % (url, offset, size))

                if output:
                    data = read_arcd(offset, size)
                    try:
                        if flags & 1:
                            #print("encrypted")
                            xtea_decryptCTR(bytearray(b'aQj8CScgNP4VsfXK'), data)
                        if compressed_size != 0xFFFFFFFF:
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
                elif compressed_size != 0xFFFFFFFF:
                    print("Found %s %d-%d(%d) [Compressed%s]" % (url, offset, size, compressed_size, " Encrypted" if flags & 1 else ""))
                else:
                    print("Found %s %d-%d%s" % (url, offset, size, " [Encrypted]" if flags & 1 else ""))
