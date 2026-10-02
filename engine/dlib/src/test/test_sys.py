import shutil, os
shutil.rmtree('testdir', True)
if not os.path.exists('testdir'):
    os.mkdir("testdir")

