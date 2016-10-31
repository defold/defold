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