
import sys

sys.path.insert(0, "src/gamesys/test")

import test_script_server

local_ip = "localhost"

PORT=9001
serv = test_script_server.Server(port=PORT, ip=local_ip)
serv.start()
