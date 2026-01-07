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


def do_GET(req):

    result = {}
    result['to_send'] = ""
    result['headers'] = {}

    if req.startswith('/test_progress'):
        result['to_send'] = "test_progress_complete"
        if req == '/test_progress_content_length':
            result['headers']['Content-Length'] = len(result['to_send'])
        elif req == '/test_progress_chunked':
            chunk_size = 64 * 1024
            chunk_count = 2
            result['to_send'] = 'A' * chunk_count * chunk_size
            result['headers']['Content-Length'] = chunk_count * chunk_size

    return result
