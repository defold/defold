#!/usr/bin/env python
# encoding: utf-8
# Christoph Koke, 2013
#
# Based on https://github.com/weka-io/waf/blob/8ac4646d0966fe22f8ae8955c7e9b9d56ae8288e/waflib/extras/clang_compilation_database.py
# Original is BSD 3-Clause License

import sys, os, json, shlex, pipes
from waflib import Logs, TaskGen, Task
from waflib.Tools import c, cxx

Task.Task.keep_last_cmd = True

@TaskGen.feature('*')
@TaskGen.after_method('process_use')
def collect_compilation_db_tasks(self):
	try:
		clang_db = self.bld.clang_compilation_database_tasks
	except AttributeError:
		clang_db = self.bld.clang_compilation_database_tasks = []
		self.bld.add_post_fun(write_compilation_database)

	for task in getattr(self, 'compiled_tasks', []):
		if isinstance(task, (c.c, cxx.cxx)):
			clang_db.append(task)

def write_compilation_database(ctx):
	database_file = ctx.bldnode.make_node('compile_commands.json')
	Logs.info("Build commands will be stored in %s" % database_file.path_from(ctx.path))
	try:
		root = json.load(database_file)
	except IOError:
		root = []
	clang_db = dict((x["file"], x) for x in root)
	for task in getattr(ctx, 'clang_compilation_database_tasks', []):
		try:
			cmd = task.last_cmd
		except AttributeError:
			continue
		directory = getattr(task, 'cwd', ctx.variant_dir)
		f_node = task.inputs[0]
		filename = os.path.relpath(f_node.abspath(), directory)
		cmd = " ".join(map(pipes.quote, cmd))
		entry = {
			"directory": directory,
			"command": cmd,
			"file": filename,
		}
		clang_db[filename] = entry
	root = list(clang_db.values())
	database_file.write(json.dumps(root, indent=2))

