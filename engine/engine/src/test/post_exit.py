import sys
sys.path = ['../script/build/default/src/script'] + sys.path
import sys_ddf_pb2, httplib

m = sys_ddf_pb2.Exit()
m.code = 6

conn = httplib.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@system/exit", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
