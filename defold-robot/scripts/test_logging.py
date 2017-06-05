import sys, re

def command0():
	sys.exit(0)

commands = {
	0: command0,
}

def main():
	with open('test.1.log', 'w', 0) as f:
		while True:
			arg = raw_input('Command:')
			command = commands.get(int(arg))
			if command:
				command()
			else:
				f.write('Command %s\n' % (arg))

if __name__ == "__main__":
	main()
