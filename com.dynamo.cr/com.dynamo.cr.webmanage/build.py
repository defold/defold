#! /usr/bin/env python

import os
from glob import glob
from jinja2 import Environment, FileSystemLoader

def build():
    env = Environment(loader=FileSystemLoader('templates'))

    for t in glob('templates/*.html'):
        t = os.path.basename(t)
        if t == 'base.html':
            continue

        template = env.get_template(t)
        print t
        with open('war/%s' % t, 'w') as f:
            print >>f, template.render()

if __name__ == '__main__':
    build()
