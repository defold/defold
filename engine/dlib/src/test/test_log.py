# Copyright 2020-2026 The Defold Foundation
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

import socket, sys

port = int(sys.argv[1])

def recv_line(s):
    ret = ''
    c = s.recv(1).decode()
    while c != '\n':
        ret += c
        c = s.recv(1).decode()
    return ret

def connect():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect(('localhost', port))

    code, msg = recv_line(s).split(' ', 1)
    if int(code) == 0:
        return s
    else:
        return None

# open connections until failure
sockets = []
while True:
    s = connect()
    if s:
        sockets.append(s)
    else:
        break

# keep socket 5
sock = sockets[5]
del sockets[5]

for s in sockets:
    s.shutdown(socket.SHUT_RDWR)
    s.close()

# test_log.cpp is waiting, write 255 to continue
sys.stdout.buffer.write(bytes([255]))
sys.stdout.flush()

l1 = recv_line(sock)
l2 = recv_line(sock)
sock.shutdown(socket.SHUT_RDWR)
sock.close()

assert l1 == 'WARNING:DLIB: a warning 123', 'got %s' % l1
assert l2 == 'ERROR:DLIB: an error 456', 'got %s' % l2
