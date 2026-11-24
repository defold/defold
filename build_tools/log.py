import sys

def _uprint(*objects, sep=' ', end='\n', file=sys.stdout):
    enc = file.encoding
    if enc == 'UTF-8':
        print(*objects, sep=sep, end=end, file=file)
    else:
        f = lambda obj: str(obj).encode(enc, errors='backslashreplace').decode(enc)
        print(*map(f, objects), sep=sep, end=end, file=file)

def log(msg):
    if type(msg) == bytes:
        try:
            print (msg.decode('utf-8'))
        except:
            try:
                print (msg.decode('utf-16'))
            except:
                pass
    else:
        _uprint(msg)
    sys.stdout.flush()
    sys.stderr.flush()
