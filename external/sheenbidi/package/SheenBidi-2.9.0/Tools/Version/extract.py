#!/usr/bin/env python3
import sys
import re

header_file = sys.argv[1]

with open(header_file, 'r', encoding='utf-8') as f:
    content = f.read()

match = re.search(r'SHEENBIDI_VERSION_STRING\s+"([^"]+)"', content)
if match:
    print(match.group(1))
else:
    print("UNKNOWN")
