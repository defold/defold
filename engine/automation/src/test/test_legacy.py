"""A legacy registration must fail before either extension initializes."""
import os
import socket
import subprocess
import sys

with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    port = reservation.getsockname()[1]
result = subprocess.run([sys.argv[1], 'build/default/game.projectc'], cwd=sys.argv[2],
                        env=dict(os.environ, DM_SERVICE_PORT=str(port)),
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
output = result.stdout.decode(errors='replace')
assert 'Remove the legacy Automation Bridge native extension' in output, output
assert 'AUTOMATION_FIXTURE_INITIALIZED' not in output, output
