#! /usr/bin/env python
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

class TestContext(object):
	env = None
	all_task_gen = None
	targets = None
	def initialize(self, bld_env, bld_tasks, bld_targets):
		self.env = bld_env
		self.all_task_gen = bld_tasks
		if bld_targets and isinstance(bld_targets, str):
			bld_targets = [bld_targets]
		else:
			bld_targets = []
		self.targets = bld_targets

	def get_all_task_gen(self):
		return self.all_task_gen

def create_test_context():
    return TestContext()

def is_valid(ctx):
	return ctx != None

def initialize_test_context(ctx, bld):
	if ctx == None:
		return
	ctx.initialize(bld.env, bld.get_all_task_gen(), bld.targets)
