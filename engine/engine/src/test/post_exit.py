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

import sys
sys.path = ['../script/build/src/script'] + sys.path
import sys_ddf_pb2, http.client

m = sys_ddf_pb2.Exit()
m.code = 6

conn = http.client.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/exit", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
