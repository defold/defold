
class TestContext:
	env = None
	all_task_gen = None
	def initialize(self, bld_env, bld_tasks):
		self.env = bld_env
		self.all_task_gen = bld_tasks

def create_test_context():
    return TestContext()

def is_valid(ctx):
	return ctx != None

def initialize_test_context(ctx, bld):
	if ctx == None:
		return
	ctx.initialize(bld.env, bld.get_all_task_gen())
