import os, sys, shutil

if os.path.exists('tmp'):
    shutil.rmtree('tmp')

os.mkdir('tmp')
