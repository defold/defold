import shutil, os
if not os.path.exists('tmp'):
    os.mkdir('tmp')
else:
    shutil.rmtree('tmp/cache', True)
f = open('tmp/afile', 'wb')
f.close()

