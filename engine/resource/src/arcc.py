#! /usr/bin/env python
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



import stat, os, sys, struct, dlib, signal
from optparse import OptionParser

ENCRYPTED_EXTS = [".luac", ".scriptc", ".gui_scriptc", ".render_scriptc"]
KEY = "aQj8CScgNP4VsfXK"
VERSION = 4
HASH_MAX_LENGTH = 64 # 512 bits
HASH_LENGTH = 18

class Entry(object):
    def __init__(self, root, filename, compress):
        rel_name = os.path.relpath(filename, root)
        rel_name = rel_name.replace('\\', '/')

        size = os.stat(filename)[stat.ST_SIZE]
        f = open(filename, 'rb')
        self.filename = '/' + rel_name
        if compress == True:
            tmp_buf = f.read()
            max_compressed_size = dlib.dmLZ4MaxCompressedSize(size)
            self.resource = dlib.dmLZ4CompressBuffer(tmp_buf, size, max_compressed_size)
            self.compressed_size = len(self.resource)
            # Store uncompressed if gain is less than 5%
            # We believe that the shorter load time will compensate in this case.
            comp_ratio = sys.float_info.max
            if size == 0:
                print("Warning! Size of %s is 0" % (self.filename))
            else:
                comp_ratio = float(self.compressed_size)/float(size)
            if comp_ratio > 0.95:
                self.resource = tmp_buf
                self.compressed_size = 0xFFFFFFFFL
        else:
            self.resource = f.read()
            self.compressed_size = 0xFFFFFFFFL

        if os.path.splitext(filename)[-1] in ENCRYPTED_EXTS:
            self.flags = 1
            self.resource = dlib.dmEncryptXTeaCTR(self.resource, KEY)
        else:
            self.flags = 0
        self.size = size
        f.close()

    def SetResourceOffset(self, offset):
        self.resource_offset = offset

    def __repr__(self):
        return '%s(%d)' % (self.filename, self.size)


class EntryData(object):
    def __init__(self, root, filename, compress, hashpostfix):
        rel_name = os.path.relpath(filename, root)
        rel_name = rel_name.replace('\\', '/')

        size = os.stat(filename)[stat.ST_SIZE]
        f = open(filename, 'rb')
        self.path = '/' + rel_name
        self.path_size = len(self.path)
        the_hash_str = ('awesome hash here' + str(hashpostfix)).ljust(HASH_MAX_LENGTH)
        the_hash_bytes = bytearray(the_hash_str)#b'awesomehash'
        self.hash = the_hash_bytes
        self.hash_size = len(the_hash_bytes)
        if compress == True:
            tmp_buf = f.read()
            max_compressed_size = dlib.dmLZ4MaxCompressedSize(size)
            self.resource = dlib.dmLZ4CompressBuffer(tmp_buf, size, max_compressed_size)
            self.compressed_size = len(self.resource)
            # Store uncompressed if gain is less than 5%
            # We believe that the shorter load time will compensate in this case.
            comp_ratio = sys.float_info.max
            if size == 0:
                print("Warning! Size of %s is 0" % (self.path))
            else:
                comp_ratio = float(self.compressed_size)/float(size)
            if comp_ratio > 0.95:
                self.resource = tmp_buf
                self.compressed_size = 0xFFFFFFFFL
        else:
            self.resource = f.read()
            self.compressed_size = 0xFFFFFFFFL

        if os.path.splitext(filename)[-1] in ENCRYPTED_EXTS:
            self.flags = 1
            self.resource = dlib.dmEncryptXTeaCTR(self.resource, KEY)
        else:
            self.flags = 0
        self.size = size
        f.close()

    def SetResourceOffset(self, offset):
        self.resource_offset = offset

    def __repr__(self):
        return '%s(%d)' % (self.path, self.size)

def align_file(file, align):
    new_pos = file.tell() + (align - 1)
    new_pos &= ~(align-1)
    file.seek(new_pos)

def entry_data_hash_cmp(a, b):
    if (a.hash > b.hash):
        return 1
    elif (a.hash == b.hash):
        return 0
    else:
        return -1

def set_output_path(rel_path, full_path):
    return rel_path + os.path.basename(full_path)

def compile(input_files, options):
    # Sort file-names. Names must be sorted for binary search at run-time and for correct hash assignment for tests.
    input_files.sort()

    if not options.output_file:
        oifn = set_output_path(options.rel_path, options.output_file_index)
        out_index = open(oifn, 'wb+')
        odfn = set_output_path(options.rel_path, options.output_file_data)
        out_data = open(odfn, 'wb+')

        # TODO magic number
        out_index.seek(0)
        out_index.write(struct.pack('!I', VERSION)) # Version
        out_index.write(struct.pack('!I', 0)) # Pad
        out_index.write(struct.pack('!Q', 0)) # Userdata
        out_index.write(struct.pack('!I', 0)) # EntryCount (placeholder, actual value written later)
        out_index.write(struct.pack('!I', 0)) # EntryOffset (placeholder, actual value written later)
        out_index.write(struct.pack('!I', 0)) # HashOffset (placeholder, actual value written later)
        out_index.write(struct.pack('!I', HASH_LENGTH))

        entry_count = 0
        entry_datas = []

        # write resource data to datafile
        num_input_files = len(input_files)
        out_data.seek(0)
        for i,f in enumerate(input_files):
            align_file(out_data, 4)
            e = EntryData(options.root, f, options.compress, num_input_files - i)
            e.resource_offset = out_data.tell()
            out_data.write(e.resource)
            entry_datas.append(e)
            entry_count += 1
        out_data.close()

        # sort entrydatas on hash for binary search in runtime
        entry_datas.sort(entry_data_hash_cmp)

        # write hash data to linear array
        i = 0
        align_file(out_index, 4)
        hash_offset = out_index.tell()
        while i < entry_count:
            e = entry_datas[i]
            out_index.write(e.hash)
            i += 1

        # For writing offset to entrydatas in index file at the end
        entry_offset = out_index.tell()

        # write hashes to index file
        # assumes 64 byte per hash

        # write to index file
        i = 0
        align_file(out_index, 4)
        while i < entry_count:
            e = entry_datas[i]
            out_index.write(struct.pack('!I', e.resource_offset))
            out_index.write(struct.pack('!I', e.size))
            out_index.write(struct.pack('!I', e.compressed_size))
            out_index.write(struct.pack('!I', e.flags))
            i += 1

        out_index.seek(0)
        out_index.write(struct.pack('!I', VERSION)) # Version
        out_index.write(struct.pack('!I', 0)) # Pad
        out_index.write(struct.pack('!Q', 0)) # Userdata
        out_index.write(struct.pack('!I', entry_count)) # EntryCount
        out_index.write(struct.pack('!I', entry_offset)) # EntryOffset
        out_index.write(struct.pack('!I', hash_offset)) # EntryOffset
        out_index.write(struct.pack('!I', HASH_LENGTH))
        out_index.close()

    else:
        # this is relative here (to destination path in build/ dir), in my code about it is absolut
        out_file = open(options.output_file, 'wb')
        # Version
        out_file.write(struct.pack('!I', VERSION))
        # Pad
        out_file.write(struct.pack('!I', 0))
        # Userdata
        out_file.write(struct.pack('!Q', 0))
        # StringPoolOffset (dummy)
        out_file.write(struct.pack('!I', 0))
        # StringPoolSize (dummy)
        out_file.write(struct.pack('!I', 0))
        # EntryCount (dummy)
        out_file.write(struct.pack('!I', 0))
        # EntryOffset (dummy)
        out_file.write(struct.pack('!I', 0))

        entries = []
        string_pool_offset = out_file.tell()
        strings_offset = []
        for i,f in enumerate(input_files):
            e = Entry(options.root, f, options.compress)
            # Store offset to string
            strings_offset.append(out_file.tell() - string_pool_offset)
            # Write filename string
            out_file.write(e.filename)
            out_file.write(chr(0))
            entries.append(e)

        string_pool_size = out_file.tell() - string_pool_offset

        resources_offset = []
        for i,e in enumerate(entries):
            align_file(out_file, 4)
            resources_offset.append(out_file.tell())
            out_file.write(e.resource)

        align_file(out_file, 4)
        entry_offset = out_file.tell()
        for i,e in enumerate(entries):
            out_file.write(struct.pack('!I', strings_offset[i]))
            out_file.write(struct.pack('!I', resources_offset[i]))
            out_file.write(struct.pack('!I', e.size))
            out_file.write(struct.pack('!I', e.compressed_size))
            out_file.write(struct.pack('!I', e.flags))

        # Reset file and write actual offsets
        out_file.seek(0)
        # Version
        out_file.write(struct.pack('!I', VERSION))
        # Pad
        out_file.write(struct.pack('!I', 0))
        # Userdata
        out_file.write(struct.pack('!Q', 0))
        # StringPoolOffset
        out_file.write(struct.pack('!I', string_pool_offset))
        # StringPoolSize
        out_file.write(struct.pack('!I', string_pool_size))
        # EntryCount
        out_file.write(struct.pack('!I', len(entries)))
        # EntryOffset
        out_file.write(struct.pack('!I', entry_offset))

        out_file.close()

if __name__ == '__main__':
    usage = 'usage: %prog [options] files'
    parser = OptionParser(usage)
    parser.add_option('-r', dest='root', help='Root directory', metavar='ROOT', default='')
    parser.add_option('-o', dest='output_file', help='Output file', metavar='OUTPUT')
    parser.add_option('-i', dest='output_file_index', help='Index output file', metavar='OUTPUTINDEX')
    parser.add_option('-d', dest='output_file_data', help='Data output file', metavar='OUTPUTDATA')
    parser.add_option('-c', dest='compress', action='store_true', help='Use compression', metavar='COMPRESSION', default=False)
    parser.add_option('-p', dest='rel_path', help='Output relative target path')
    (options, args) = parser.parse_args()
    if not options.output_file and not options.output_file_index:
        parser.error('Output file not specified (-o)')

    try:
        compile(args, options)
    except:
        # Try to remove the outfile in case of any errors
        if options.output_file and os.path.exists(options.output_file):
            try:
                os.remove(options.output_file)
            except:
                pass
        raise
