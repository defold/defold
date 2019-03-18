import sys
sys.path = ['../script/build/default/src/script'] + sys.path
import sys_ddf_pb2, httplib

m = sys_ddf_pb2.Reboot()
m.arg1 = '--config=dmengine.unload_builtins=0'
m.arg2 = 'src/test/build/default/game.projectc'

conn = httplib.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/reboot", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
