#! /usr/bin/env python

import stat, os, sys, struct, dlib
from optparse import OptionParser

ENCRYPTED_EXTS = [".luac", ".scriptc", ".gui_scriptc", ".render_scriptc"]
KEY = "aQj8CScgNP4VsfXK"
VERSION = 4

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
            self.resource = dlib.dmLZ4CompressBuffer(tmp_buf, max_compressed_size)
            self.compressed_size = len(self.resource)
            # Store uncompressed if gain is less than 5%
            # We believe that the shorter load time will compensate in this case.
            comp_ratio = sys.float_info.max
            if size == 0:
                print "Warning! Size of %s is 0" % (self.filename)
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

    def __repr__(self):
        return '%s(%d)' % (self.filename, self.size)

def align_file(file, align):
    new_pos = file.tell() + (align - 1)
    new_pos &= ~(align-1)
    file.seek(new_pos)

def compile(input_files, options):
    # Sort file-names. Names must be sorted for binary search at run-time.
    input_files.sort()

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
    parser.add_option('-c', dest='compress', action='store_true', help='Use compression', metavar='COMPRESSION', default=False)
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error('Output file not specified (-o)')

    try:
        compile(args, options)
    except:
        # Try to remove the outfile in case of any errors
        if os.path.exists(options.output_file):
            try:
                os.remove(options.output_file)
            except:
                pass
        raise
