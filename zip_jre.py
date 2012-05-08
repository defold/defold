#!/usr/bin/python

import sys, os, zipfile
from os.path import join, relpath

PREFIX='Defold'
ZIP=sys.argv[1]
JRE_ROOT=sys.argv[2]
zip = zipfile.ZipFile(ZIP, 'a')

for root, dirs, files in os.walk(JRE_ROOT):
    for f in files:
        full = join(root, f)
        rel = relpath(full, JRE_ROOT)

        with open(full, 'rb') as file:
            data = file.read()
            path = join(PREFIX, rel)
            zip.writestr(path, data)

zip.close()