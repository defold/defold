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

import sys, os, os.path, glob, urllib, urllib.request, codecs, time, hashlib
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse

def mangle(url):
    url = urlparse(url)
    return 'defold%s' % url.path.replace('/', '-') # we avoid putting the possibly secret url in the output messages

def log(msg):
    print(msg)
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
                # remove stale tmp and treat as miss
                try: os.remove(match)
                except: pass
                return None
            key_hex = match.rsplit('-', 1)[1]
            os.utime(match, None)
            try:
                return (match, codecs.decode(key_hex, 'hex'))
            except Exception:
                # malformed key -> still return file but no key
                return (match, b'')
        return None

    def _accomodate(self, size):
        matches = glob.glob('%s/*' % (self.root))
        matches.sort(key=lambda p: os.path.getmtime(p), reverse=True)
        total_size = 0
        for p in matches:
            try:
                total_size += os.path.getsize(p)
                if total_size + size > self.max_size:
                    os.remove(p)
            except Exception as e:
                log(str(e))

    def put(self, url, key, size):
        path = self._url_to_path(url)
        pattern = '%s-*' % (path)
        for p in glob.glob(pattern):
            try: os.remove(p)
            except Exception as e: log(str(e))
        self._accomodate(size)
        key_enc = codecs.encode((key or "").encode(), 'hex').decode('ascii')
        return '%s-%s' % (path, key_enc or '00')  # ensure a suffix

def _validate_file(path, expected_size=None, expected_md5=None):
    """
    Return True if file matches expectations; False otherwise.
    - expected_size: int or None
    - expected_md5: hex string or None
    """
    try:
        st = os.stat(path)
        if expected_size is not None and st.st_size != expected_size:
            return False
        if expected_md5:
            md5 = hashlib.md5()
            with open(path, 'rb') as f:
                for chunk in iter(lambda: f.read(1024 * 1024), b''):
                    md5.update(chunk)
            if md5.hexdigest().lower() != expected_md5.lower():
                return False
        return True
    except Exception:
        return False

def _extract_md5_from_headers(headers):
    # Prefer Content-MD5 if present (base64 per RFC 1864); some servers use hex in ETag though.
    # We'll accept either:
    # - Content-MD5: base64 -> convert to hex for comparison
    # - ETag: if it looks like a hex string (no quotes/W/), use as MD5 hint
    md5_hex = None
    content_md5 = headers.get('Content-MD5')
    if content_md5:
        try:
            import base64
            md5_hex = base64.b64decode(content_md5).hex()
        except Exception:
            pass
    if not md5_hex:
        etag = headers.get('ETag') or headers.get('Etag') or ''
        # Strip quotes and weak validators
        etag = etag.strip().strip('"').removeprefix('W/').strip('"')
        # ETag is not guaranteed to be an MD5, but commonly is; only use if hex-like length
        if all(c in '0123456789abcdefABCDEF' for c in etag) and len(etag) in (32,):
            md5_hex = etag
    return md5_hex

def download(url, cb=None, cb_count=10, retries=5, timeout=60):
    """
    Download with retries and validation.
    Returns absolute cache file path on success.
    Raises RuntimeError on failure.
    """
    c = Cache('~/.dcache', 10**9 * 4)
    attempt = 0
    last_err = None

    # If we have a cached file AND the server says 304, we’ll reuse it
    # but also validate size if server returns Content-Length on 304 in later HEAD (we skip HEAD to keep simple).
    # Instead, we validate cached file against saved suffix metadata post-304 when possible.
    while attempt < retries:
        attempt += 1
        hit = c.get(url)
        headers = {}
        if hit and hit[1]:
            try:
                headers['If-None-Match'] = hit[1].decode('utf-8', 'ignore')
            except Exception:
                pass

        req = urllib.request.Request(url, None, headers)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as response:
                code = getattr(response, 'code', None) or response.getcode()
                if code == 304 and hit:
                    # Trust cached, but still confirm it’s a readable file
                    if os.path.exists(hit[0]) and os.path.getsize(hit[0]) > 0:
                        return hit[0]
                    else:
                        # cached file missing/corrupt; fall through to fresh GET by removing If-None-Match on next loop
                        hit = None
                        last_err = RuntimeError("Cached file missing or empty after 304")
                        continue

                if code != 200:
                    last_err = RuntimeError(f"Unexpected HTTP status {code}")
                    raise last_err

                size_hdr = response.headers.get('Content-Length')
                expected_size = int(size_hdr) if size_hdr is not None else None
                etag = response.headers.get('ETag', '')
                md5_hint = _extract_md5_from_headers(response.headers)

                # Allocate cache path and temp
                # Use expected_size=0 as minimum to make room for small files
                path = c.put(url, etag or '', expected_size or 0)
                tmp = path + '_tmp'

                # Remove any old tmp
                try:
                    if os.path.exists(tmp):
                        os.remove(tmp)
                except Exception:
                    pass

                # Stream to tmp, reporting progress
                n = 0
                cb_i = -1  # force first progress at 0% when size known
                with open(tmp, 'wb') as f:
                    for buf in iter(lambda: response.read(1024 * 1024), b''):
                        n += len(buf)
                        f.write(buf)
                        if cb is not None:
                            if expected_size:
                                rate = n / float(expected_size)
                                step = int(rate * cb_count)
                                if step > cb_i:
                                    cb_i = step
                                    cb(n, expected_size)
                            else:
                                # Unknown length: call occasionally with total=None
                                if n // (1024 * 1024) != (n - len(buf)) // (1024 * 1024):
                                    cb(n, None)

                # Validation
                if expected_size is not None and n != expected_size:
                    raise IOError(f"Incomplete download: got {n} bytes, expected {expected_size}")

                if md5_hint:
                    md5 = hashlib.md5()
                    with open(tmp, 'rb') as f:
                        for chunk in iter(lambda: f.read(1024 * 1024), b''):
                            md5.update(chunk)
                    if md5.hexdigest().lower() != md5_hint.lower():
                        raise IOError("Checksum mismatch (ETag/Content-MD5)")

                # Success: atomically move into place
                os.replace(tmp, path)
                return path

        except HTTPError as e:
            if e.code == 304 and hit:
                if os.path.exists(hit[0]) and os.path.getsize(hit[0]) > 0:
                    return hit[0]
                last_err = RuntimeError("304 returned but cached file missing/corrupt")
            else:
                last_err = e
        except (URLError, TimeoutError, ConnectionError, IOError) as e:
            last_err = e
        except Exception as e:
            last_err = e
        finally:
            # Clean up temp on failure to avoid reusing partial files
            hit_now = c.get(url)
            if hit_now:
                tmp_candidate = hit_now[0] + '_tmp'
                try:
                    if os.path.exists(tmp_candidate):
                        os.remove(tmp_candidate)
                except Exception:
                    pass

        # Backoff before retry
        if attempt < retries:
            delay = min(2 ** (attempt - 1), 30)  # 1,2,4,8,16,30...
            log(f"Download failed (attempt {attempt}/{retries}): {last_err}. Retrying in {delay}s...")
            time.sleep(delay)

    # All attempts failed
    raise RuntimeError(f"Failed to download {url} after {retries} attempts: {last_err}")