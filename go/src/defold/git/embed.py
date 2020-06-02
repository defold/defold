# Copyright 2020 The Defold Foundation
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

from cStringIO import StringIO
import zlib
import base64

def to_go(name, in_name, out_name):
    in_file = open(in_name, 'rb')
    out_file = open(out_name, 'wb')

    header_str = '''
package git
var %s = "'''
    out_file.write(header_str % name)

    data = in_file.read()
    data = zlib.compress(data)
    out_file.write(base64.standard_b64encode(data))
    out_file.write('"\n')
    in_file.close()
    out_file.close()

if __name__ == '__main__':
    to_go('gitCompressedBase64', 'git-bin/git.darwin', 'git_darwin.go')
    to_go('gitCompressedBase64', 'git-bin/git.linux', 'git_linux.go')
    to_go('gitUploadPackCompressedBase64', 'git-bin/git-upload-pack.darwin', 'git-upload-pack_darwin.go')
    to_go('gitUploadPackCompressedBase64', 'git-bin/git-upload-pack.linux', 'git-upload-pack_linux.go')
