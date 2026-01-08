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

"""Generate the trig_lookup.h file

Synopsis:
    python gen_trig_lookup.py --bits bits

Options:
    bits    The amount of bits to use for the resolution in the lookup table, 16 by default and also max.
"""

import sys
import getopt
import os
import math

def generate(path, bits):
    size = 2**bits
    data = ""
    table = "\n    {"
    entry = "\n        {value}f,"
    table = table + "".join([entry.format(value = math.cos(v*2.0*math.pi/size)) for v in range(0, size)]) + "\n    }"
    table_mask = size - 1
    remainder = 2**(16-bits)
    frac_mask = remainder - 1
    weight = 1.0 / remainder

    files = {"trig_lookup_template.h" : "trig_lookup.h", "trig_lookup_template.cpp" : "trig_lookup.cpp"}
    for k,v in files.iteritems():
        template = path + "/" + k
        target = path + "/../src/dlib/" + v
        with open(template, 'r') as f:
            data = f.read()
            data = data.format(size = size, table = table, table_mask = table_mask, frac_mask = frac_mask, weight = weight, bits = bits)
        with open(target, 'w') as f:
            f.write(data)

def main():
    # parse command line options
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hb", ["help", "bits="])
    except getopt.error, msg:
        print(msg)
        print("for help use --help")
        sys.exit(2)
    bits = 16
    # process options
    for o, a in opts:
        if o in ("-h", "--help"):
            print(__doc__)
            sys.exit(0)
        if o in ("-b", "--bits"):
            if bits > 16:
                print(__doc__)
                sys.exit(0)
            bits = int(a)

    path = os.path.dirname(sys.argv[0])
    generate(path, bits)

if __name__ == "__main__":
    main()