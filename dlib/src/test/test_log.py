import socket, sys

port = int(sys.argv[1])

def recv_line(s):
    ret = ''
    c = s.recv(1)
    while c != '\n':
        ret += c
        c = s.recv(1)
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
sys.stdout.write(chr(255))
sys.stdout.flush()

l1 = recv_line(sock)
l2 = recv_line(sock)
sock.shutdown(socket.SHUT_RDWR)
sock.close()

assert l1 == 'WARNING:DLIB: a warning 123', 'got %s' % l1
assert l2 == 'ERROR:DLIB: an error 456', 'got %s' % l2
