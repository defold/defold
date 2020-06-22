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

import shutil, os
shutil.rmtree('tmp/cache', True)
shutil.rmtree('tmp/http_files', True)
os.mkdir('tmp/http_files')

def http_file(name, content):
    f = open('tmp/http_files/%s' % name, 'wb')
    f.write(content)
    f.close()

http_file('a.txt', 'You will find this data in a.txt and d.txt')
http_file('b.txt', 'Some data in file b')
http_file('c.txt', 'Some data in file c')
http_file('d.txt', 'You will find this data in a.txt and d.txt')

