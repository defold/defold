import sys, os, httplib
sys.path.append(os.path.join(os.environ['DYNAMO_HOME'], 'lib', 'python', 'gameobject'))
sys.path.append(os.path.join('build', 'default', 'proto'))
import engine_ddf_pb2, lua_ddf_pb2

m = engine_ddf_pb2.RunScript()
m.module.source.filename = 'test.luac'
m.module.source.script = 'print("42"); msg.post("@system:", "exit", {code=42})'
m.module.source.bytecode = ''

conn = httplib.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/run_script", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
