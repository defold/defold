import os
import sys
import http.client
from test_python_paths import add_engine_test_proto_paths

add_engine_test_proto_paths()
import sys_ddf_pb2

m = sys_ddf_pb2.Reboot()
m.arg1 = '--config=dmengine.unload_builtins=0'
m.arg2 = 'build/src/test/build/default/game.projectc'
if not os.path.exists(m.arg2):
    m.arg2 = 'src/test/build/default/game.projectc'

conn = http.client.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/reboot", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
