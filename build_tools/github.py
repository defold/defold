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

import hashlib
import mimetypes
import logging
import os
import time
from log import log

URL_GRAPHQL_API = "https://api.github.com/graphql"
URL_REST_API    = "https://api.github.com"

def _enable_verbose_logging():
    try:
        import http.client as http_client
    except ImportError:
        # Python 2
        import httplib as http_client
    http_client.HTTPConnection.debuglevel = 1

    # You must initialize logging, otherwise you'll not see debug output.
    logging.basicConfig()
    logging.getLogger().setLevel(logging.DEBUG)
    requests_log = logging.getLogger("requests.packages.urllib3")
    requests_log.setLevel(logging.DEBUG)
    requests_log.propagate = True

def _fix_url(url):
    if url.startswith("http"):
        return url
    return URL_REST_API + ("/" if not url.startswith("/") else "") + url

def _create_headers(headers, token):
    if not headers:
        headers = {}
    headers["Accept"] = "application/vnd.github+json"
    headers["Authorization"] = "token %s" % (token)
    headers["X-GitHub-Api-Version"] = "2022-11-28"
    return headers

# use GraphQL API
def query(query, token, headers = None, variables = None):
    import requests
    try:
        url = URL_GRAPHQL_API
        if query.strip().startswith("query"):
            json = { 'query': query, "variables": variables }
        elif query.strip().startswith("mutation"):
            json = { 'query': query, "variables": variables }
        else:
            json = { 'query': "query " + query, "variables": variables }
        headers = _create_headers(headers, token)
        response = requests.post(url, json = json, headers = headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def get(url, token, headers = None):
    import requests
    try:
        headers = _create_headers(headers, token)
        response = requests.get(_fix_url(url), headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def compare_commits(repository, base, head, token):
    import requests
    headers = _create_headers({}, token)
    if not token:
        del headers["Authorization"]
    url = _fix_url("/repos/%s/compare/%s...%s" % (repository, base, head))
    response = requests.get(url, headers = headers, timeout = (10, 60))
    # An unavailable comparison must stop publication, not be treated as a new release.
    response.raise_for_status()
    return response.json()

def post(url, token, data = None, json = None, files = None, headers = None):
    import requests
    try:
        headers = _create_headers(headers, token)
        response = requests.post(_fix_url(url), data = data, json = json, files = files, headers = headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def put(url, token, data = None, json = None, headers = None):
    import requests
    try:
        headers = _create_headers(headers, token)
        response = requests.put(_fix_url(url), data = data, json = json, headers = headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def patch(url, token, data = None, json = None, headers = None):
    import requests
    try:
        headers = _create_headers(headers, token)
        response = requests.patch(_fix_url(url), data = data, json = json, headers = headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def delete(url, token, headers = None):
    import requests
    try:
        headers = _create_headers(headers, token)
        response = requests.delete(_fix_url(url), headers = headers)
        response.raise_for_status()
        if response.content and response.content != "":
            return response.json()
        else:
            return None
    except Exception as err:
        print(err)
        return None

def _get_release_assets(url, headers):
    import requests
    assets = []
    page = 1
    while True:
        response = requests.get(url, headers = headers, params = {"per_page": 100, "page": page}, timeout = (10, 60))
        response.raise_for_status()
        batch = response.json()
        assets.extend(batch)
        if len(batch) < 100:
            return assets
        page += 1

def _get_file_sha256(filepath):
    digest = hashlib.sha256()
    with open(filepath, 'rb') as data:
        for chunk in iter(lambda: data.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

def upload_release_asset(release, token, filepath, name, max_attempts = 5):
    import requests
    headers = _create_headers({}, token)
    content_type, _ = mimetypes.guess_type(name)
    upload_headers = dict(headers, **{"Content-Type": content_type or "application/octet-stream"})
    upload_url = release["upload_url"].split("{", 1)[0]
    expected_size = os.path.getsize(filepath)
    expected_digest = None
    assets = release.get("assets", [])

    for attempt in range(1, max_attempts + 1):
        delay = 5 * 2 ** (attempt - 1)
        try:
            if attempt > 1:
                # A failed upload can leave a starter asset, or a completed upload
                # whose response was lost. Refresh the metadata before retrying.
                assets = _get_release_assets(release["assets_url"], headers)
            for asset in assets:
                if asset.get("name") == name:
                    if (attempt > 1 and asset.get("state") == "uploaded" and
                            asset.get("size") == expected_size and asset.get("digest")):
                        if expected_digest is None:
                            expected_digest = "sha256:" + _get_file_sha256(filepath)
                        if asset["digest"] == expected_digest:
                            log("GitHub release asset %s is already uploaded and its SHA-256 matches" % name)
                            return asset
                    log("Deleting %s - %s" % (asset["id"], name))
                    response = requests.delete(asset["url"], headers = headers, timeout = (10, 60))
                    if response.status_code != 404:
                        response.raise_for_status()

            log("Uploading to GitHub %s (asset %s, attempt %d/%d)" % (upload_url, name, attempt, max_attempts))
            # Reopen the file so retries always send the entire asset.
            with open(filepath, 'rb') as data:
                # Requests also uses the connect timeout while sending the body.
                # Allow large uploads five minutes for blocked writes and response reads.
                response = requests.post(upload_url, params = {"name": name}, data = data,
                                         headers = upload_headers, timeout = (300, 300))
            response.raise_for_status()
            asset = response.json()
            if asset.get("state") == "uploaded" and asset.get("size") == expected_size:
                return asset
            log("GitHub did not confirm a complete upload of %s (state=%s, size=%s, expected=%s)" %
                (name, asset.get("state"), asset.get("size"), expected_size))
        except requests.RequestException as err:
            log("GitHub release asset %s failed (attempt %d/%d): %s" % (name, attempt, max_attempts, err))
            response = err.response
            if response is not None:
                status = response.status_code
                error_data = {}
                if status in (403, 422):
                    try:
                        error_data = response.json()
                    except ValueError:
                        pass
                    if not isinstance(error_data, dict):
                        error_data = {}
                duplicate = status == 422 and any(error.get("code") == "already_exists"
                                                   for error in error_data.get("errors", []) if isinstance(error, dict))
                # GitHub uses 403 for both permission errors and rate limits.
                rate_limited = status == 429 or (status == 403 and (
                    response.headers.get("Retry-After") is not None or
                    response.headers.get("X-RateLimit-Remaining") == "0" or
                    "rate limit" in str(error_data.get("message", "")).lower()))
                if not (500 <= status < 600 or status == 408 or rate_limited or duplicate):
                    return None
                if rate_limited:
                    delay = 60 * 2 ** (attempt - 1)
                    if response.headers.get("X-RateLimit-Remaining") == "0":
                        try:
                            # Wait past the reset second before making another API request.
                            delay = max(delay, int(response.headers["X-RateLimit-Reset"]) - int(time.time()) + 1)
                        except (KeyError, ValueError):
                            pass
                try:
                    delay = max(delay, int(response.headers.get("Retry-After", "0")))
                except ValueError:
                    pass
            elif not isinstance(err, (requests.ConnectionError, requests.Timeout,
                                      requests.exceptions.ChunkedEncodingError, requests.exceptions.JSONDecodeError)):
                return None

        if attempt < max_attempts:
            log("Retrying GitHub release asset %s in %s seconds" % (name, delay))
            time.sleep(delay)
    return None
