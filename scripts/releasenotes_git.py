#!/usr/bin/env python

import os, sys, subprocess, re

ISSUE_PATTERN='^([a-fA-F0-9]+)\s(DEF-\d+|DEFEDIT-\d+|)(.*)'

def run(cmd):
	p = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE)
	p.wait()
	out, err = p.communicate()
	if p.returncode != 0:
		raise Exception("Failed to run: " + cmd)

	return out

def get_version():
	# get the latest commit to the VERSION file
	sha1 = run('git log -n 1 --pretty=format:%H -- VERSION')
	with open('VERSION', 'rb') as f:
		d = f.read()
		tokens = d.split('.')
		minor = int(tokens[2])
		minor = minor
		return (sha1, "%s.%s.%d" % (tokens[0], tokens[1], minor))
	return (sha1, "<unknown>")

def get_each_change(line):
	m = re.search(ISSUE_PATTERN, line)
	if not m:
		print line
		return
	sha1 = m.group(1)
	issue = m.group(2)
	desc = m.group(3)

	print sha1, issue

	out = run("git log %s -1" % sha1)
	print out

def get_each_link(line):
	m = re.search(ISSUE_PATTERN, line)
	if not m:
		return
	issue = m.group(2)
	desc = m.group(3)

	print "  * [`%s`](https://jira.int.midasplayer.com/browse/%s) - **Fixed**: %s" % (issue, issue, desc)


def get_all_changes(version):
	out = run("git log %s..HEAD --oneline" % version)
	lines = out.split('\n')

	print out
	print "#" + "*" * 64

	# Note: currently doesn't catch merges of the form:
	# “xxxxxxxxx Merge pull request #XXXX from defold/DEF-NUMBERsomething”
	def keep(s):
		if not s:
			return False
		if 'Merge pull request' in s:
			return False
		if 'Merge branch' in s:
			return False
		m = re.search(ISSUE_PATTERN, s)
		if m:
			return True
		return False

	lines = [l for l in lines if keep(l)]
	lines = sorted(lines)

	print ""
	print ""

	for l in lines:
		get_each_change(l)

	print ""
	print ""
	print "# Engine"

	for l in lines:
		if 'DEF-' in l:
			get_each_link(l)

	print "# Editor"

	for l in lines:
		if 'DEFEDIT-' in l:
			get_each_link(l)



if __name__ == '__main__':
	sha1, version = get_version()
	if sha1 is None:
		print >>sys.stderr, "Failed to open VERSION"
		sys.exit(1)

	print "Found version", version, sha1
	get_all_changes(sha1)
