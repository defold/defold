#! /usr/bin/env python
import os
from optparse import OptionParser

if __name__ == "__main__":
    usage = "usage: %prog [options] infile"
    parser = OptionParser(usage)
    parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error("Output file not specified (-o)")

    dynamo_home = os.environ["DYNAMO_HOME"]
    classpath = "%s/share/java/bob-light.jar" % dynamo_home
    ret = os.system("java -cp %s com.dynamo.bob.pipeline.TextureGenerator %s %s" % (classpath, args[0], options.output_file))
    if ret != 0:
        raise Exception("Failed to compile texture (%d)" % ret)

