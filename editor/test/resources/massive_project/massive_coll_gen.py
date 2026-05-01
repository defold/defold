# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

def gen():
	header = '''name: "default"
'''
	inst_tmpl = '''instances {{
  id: "root{0}"
  prototype: "/test.go"
  position {{
    x: {1}
    y: {2}
    z: 0.0
  }}
  rotation {{
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0
  }}
  scale: 1.0
}}'''
	data = "" + header
	for i in range(1000):
		x = (i%100) * 200
		y = (i/100) * 200
		data += inst_tmpl.format(i, x, y)
	with open('massive.collection', 'w') as f:
		f.write(data)

def main():
	gen()

if __name__ == "__main__":
    main()