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
