import shutil, os
shutil.rmtree('tmp', True)
if not os.path.exists('tmp'):
    os.mkdir("tmp")

