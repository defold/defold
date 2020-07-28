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

import sys, os, httplib
sys.path.append(os.path.join(os.environ['DYNAMO_HOME'], 'lib', 'python', 'gameobject'))
sys.path.append(os.path.join('build', 'default', 'proto'))
import engine_ddf_pb2, lua_ddf_pb2

m = engine_ddf_pb2.RunScript()
m.module.source.filename = 'test.luac'
m.module.source.script = 'local test = require("init_script.init"); print("42"); msg.post("@system:", "exit", {code=test.func(10, 4.2)})'
m.module.source.bytecode = ''
m.module.resources.extend(['/init_script/init.luac'])
m.module.modules.extend(['init_script.init'])

conn = httplib.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/run_script", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
