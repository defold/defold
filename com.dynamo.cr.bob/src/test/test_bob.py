import unittest, sys

sys.path.append('src')
import bob

class TestBob(unittest.TestCase):

    def assertSetEquals(self, l1, l2):
        self.assertEquals(set(l1), set(l2))

    def test_collect_inputs(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/*.c'])
build(p)
'''
        l = bob.exec_script(script)
        p = l['p']
        self.assertEqual(set(['test_data/main.c', 'test_data/util.c']),
                         set(p['inputs']))


    def test_create_tasks(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/main.c'])
build(p)
'''
        l = bob.exec_script(script)
        tasks = l['p']['tasks']
        self.assertEqual(1, len(tasks))
        t = tasks[0]

        self.assertSetEquals(['test_data/main.c'], t['inputs'])
        self.assertSetEquals(['build/test_data/main.o'], t['outputs'])

if __name__ == '__main__':
    unittest.main()
