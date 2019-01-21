import sys
sys.path = ['build/default/proto'] + sys.path
import resource_ddf_pb2, httplib

m = resource_ddf_pb2.Reload()
m.resources.append("/def-3652/def-3652.collectionc")

conn = httplib.HTTPConnection("localhost", int(sys.argv[1]))
conn.request("POST", "/post/@resource/reload", m.SerializeToString())
response = conn.getresponse()
data = response.read()
conn.close()
assert response.status == 200
