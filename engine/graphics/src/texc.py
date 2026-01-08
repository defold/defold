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
        raise Exception("Failed to compile texture (err: %d): %s" % (ret, args[0]))

