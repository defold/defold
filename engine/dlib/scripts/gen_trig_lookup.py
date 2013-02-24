"""Generate the trig_lookup.h file

Synopsis:
    python gen_trig_lookup.py --bits bits

Options:
    bits    The amount of bits to use for the resolution in the lookup table, 16 by default.
"""

import sys
import getopt
import os
import math

def generate(template, target):
    size = 2**16
    data = ""
    table = "\n    {"
    entry = "\n        {value},"
    table = table + "".join([entry.format(value = math.cos(v*2.0*math.pi/size)) for v in range(0, size)]) + "\n    }"

    with open(template, 'r') as f:
        data = f.read()
        data = data.format(size = size, table = table)
    with open(target, 'w') as f:
        f.write(data)

def main():
    # parse command line options
    try:
        opts, args = getopt.getopt(sys.argv[1:], "h", ["help"])
    except getopt.error, msg:
        print msg
        print "for help use --help"
        sys.exit(2)
    # process options
    for o, a in opts:
        if o in ("-h", "--help"):
            print __doc__
            sys.exit(0)

    path = os.path.dirname(sys.argv[0])
    template = path + "/trig_lookup_template.h"
    target = path + "/../src/dlib/trig_lookup.h"
    generate(template, target)

if __name__ == "__main__":
    main()