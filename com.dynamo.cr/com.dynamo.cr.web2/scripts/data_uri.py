#!/usr/bin/env python
import sys

data_uri = open(sys.argv[1], "rb").read().encode("base64").replace("\n", "")
img_tag = '<img src="data:image/png;base64,{0}">'.format(data_uri)
print img_tag
