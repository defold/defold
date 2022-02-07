# Copyright 2020-2022 The Defold Foundation
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

# Copyright 2022 The Defold Foundation
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

import mimetypes

def _fix_url(url):
    if url.startswith("http"):
        return url
    return "https://api.github.com" + ("/" if not url.startswith("/") else "") + url

def get(url, token):
    import requests
    try:
        response = requests.get(_fix_url(url), headers={"Authorization": "token %s" % (token)})
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def post(url, token, data = None, json = None, files = None, headers = None):
    import requests
    try:
        if not headers:
            headers = {}
        headers["Authorization"] = "token %s" % (token)

        response = requests.post(_fix_url(url), data = data, json = json, files = files, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def put(url, token, data = None, json = None, headers = None):
    import requests
    try:
        if not headers:
            headers = {}
        headers["Authorization"] = "token %s" % (token)

        response = requests.put(_fix_url(url), data = data, json = json, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def patch(url, token, data = None, json = None, headers = None):
    import requests
    try:
        if not headers:
            headers = {}
        headers["Authorization"] = "token %s" % (token)

        response = requests.patch(_fix_url(url), data = data, json = json, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def delete(url, token):
    import requests
    try:
        response = requests.delete(_fix_url(url), headers={"Authorization": "token %s" % (token)})
        response.raise_for_status()
        if response.content and response.content != "":
            return response.json()
        else:
            return None
    except Exception as err:
        print(err)
        return None
