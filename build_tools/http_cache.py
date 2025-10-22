# Copyright 2020-2025 The Defold Foundation
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

import sys, os, os.path, glob, urllib, urllib.request, codecs
from urllib.error import HTTPError
from urllib.parse import urlparse

def mangle(url):
    url = urlparse(url)
    #return '%s%s' % (url.hostname.replace('.', '_'), url.path.replace('/', '-'))
    return 'defold%s' % url.path.replace('/', '-') # we avoid putting the possibly secret url in the output messages

def log(msg):
    print (msg)
    sys.stdout.flush()
    sys.stderr.flush()

class Cache(object):
    def __init__(self, root, max_size):
        self.root = os.path.expanduser(root)
        if not os.path.exists(self.root):
            os.makedirs(self.root)
        self.max_size = max_size

    def _url_to_path(self, url):
        return os.path.join(self.root, mangle(url))

    def get(self, url):
        path = self._url_to_path(url)
        pattern = '%s-*' % (path)
        matches = glob.glob(pattern)

        if matches:
            match = matches[0]
            if match.endswith('_tmp'):
                os.remove(match)
                return None
            key = match.rsplit('-', 1)[1]
            os.utime(match, None)
            return (match, codecs.decode(key, 'hex'))
        else:
            return None

    def _accomodate(self, size):
        matches = glob.glob('%s/*' % (self.root))
        matches.sort(key = lambda p: os.path.getmtime(p), reverse = True)
        total_size = 0
        for p in matches:
            total_size += os.path.getsize(p)
            if total_size + size > self.max_size:
                os.remove(p)

    def put(self, url, key, size):
        path = self._url_to_path(url)
        pattern = '%s-*' % (path)
        matches = glob.glob(pattern)
        for p in matches:
            try:
                os.remove(p)
            except Exception as e:
                log(str(e))
        self._accomodate(size)
        return '%s-%s' % (path, codecs.encode(key.encode(), 'hex').decode('ascii'))

def download(url, cb = None, cb_count = 10):
    c = Cache('~/.dcache', 10**9 * 4)
    hit = c.get(url)
    headers = {}
    if hit:
        headers = {'If-None-Match' : '%s' % (hit[1].decode('utf-8'))}
    req = urllib.request.Request(url, None, headers)
    try:
        response = urllib.request.urlopen(req)
        if response.code == 200:
            size = int(response.headers.get('Content-Length', 0))
            key = response.headers.get('ETag', '')
            path = c.put(url, key, size)
            tmp = path + '_tmp'
            with open(tmp, 'wb') as f:
                buf = response.read(1024 * 1024)
                n = 0
                cb_i = 0
                while buf:
                    n += len(buf)
                    rate = n / float(size)
                    if cb is not None:
                        if cb_i < int(rate * cb_count):
                            cb_i = int(rate * cb_count)
                            cb(n, size)
                    f.write(buf)
                    buf = response.read(1024 * 1024)
            os.rename(tmp, path)
            return path
        else:
            return None
    except HTTPError as e:
        if e.code == 304:
            return hit and hit[0]
        else:
            return None
