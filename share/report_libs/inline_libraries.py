#!/usr/bin/env python

import os
import re
import base64
import mimetypes

JS_DIR     = "js/"
CSS_DIR    = "css/"
IMAGES_DIR = "images/"

js_concat = ""
css_concat = ""

# gather javascript files
for entry in os.listdir(JS_DIR):
    if entry.endswith(".js"):
        full_path = os.path.join(JS_DIR, entry)
        with open(full_path, 'r') as js_file:
            js_concat += "/* source: " + full_path + " */\n"
            js_concat += js_file.read() + "\n"

# gather css files
for entry in os.listdir(CSS_DIR):
    if entry.endswith(".css"):
        full_path = os.path.join(CSS_DIR, entry)
        with open(full_path, 'r') as css_file:
            css_concat += "/* source: " + full_path + " */\n"
            css_concat += css_file.read() + "\n"


# gather images and generate base64 representation
for entry in os.listdir(IMAGES_DIR):

    # look for url() usage where the path includes this entry
    url_re = re.compile("url\([\"\'][^\"\']+" + entry + "[\"\']\)")

    full_path = os.path.join(IMAGES_DIR, entry)
    file_name, file_extension = os.path.splitext(entry)
    mime_string = mimetypes.types_map[file_extension]
    with open(full_path, "rb") as image_file:
        encoded_string = base64.b64encode(image_file.read())
        url_data_string = "url(data:" + mime_string + ";base64," + encoded_string + ")"
        css_concat = url_re.sub(url_data_string, css_concat)


print('<style type="text/css">')
print(css_concat.strip())
print('</style>')
print('<script type="text/javascript">')
print(js_concat.strip())
print('</script>')