import sys, re

def main():
	log_path = sys.argv[1]
	with open('log.html', 'w') as outf:
		outf.write('<html><body>\n')
		with open(log_path, 'r') as f:
			for line in f:
				m = re.match(r"(?P<step>[\S]+) (?P<arg>[\S]+)", line)
				if m is not None:
					step = m.group('step')
					arg = m.group('arg')
					if step == 'Screen-capture':
						outf.write('<img src="%s"></img>\n' % (arg))
					else:
						outf.write('<p>%s</p>\n' % (line))
				else:
					outf.write('<p>%s</p>\n' % (line))
		outf.write('</body></html>\n')

if __name__ == "__main__":
	main()
