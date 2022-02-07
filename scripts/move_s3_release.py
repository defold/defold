#!/usr/bin/env python
# Copyright 2021 The Defold Foundation
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

# add build_tools folder to the import search path
import sys, os
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
sys.path.append(os.path.join(normpath(join(dirname(abspath(__file__)), '..')), "build_tools"))

import optparse
import s3

CDN_UPLOAD_URL="s3://d.defold.com/archive"

if __name__ == '__main__':
    boto_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto-2.28.0-py2.7.egg'))
    sys.path.insert(0, boto_path)
    usage = '''usage: %prog [options] command(s)

Commands:
move_release - Move a release on S3 to a new channel

Multiple commands can be specified
'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--sha1', dest='sha1',
                      default = None,
                      help = 'SHA1 of the release to move')

    parser.add_option('--to-channel', dest='to_channel',
                      default = None,
                      help = 'Target channel to move release to')

    default_archive_path = CDN_UPLOAD_URL
    parser.add_option('--archive-path', dest='archive_path',
                      default = default_archive_path,
                      help = 'S3 path where archives are uploaded')

    options, args = parser.parse_args()

    if not args:
        parser.print_help()
        exit(1)

    for cmd in args:
        if cmd == "move_release":
            if not options.sha1:
                print("No SHA1 specified")
                exit(1)
            if not options.to_channel:
                print("No channel specified")
                exit(1)
            s3.move_release(options.archive_path, options.sha1, options.to_channel)

    print('Done')
