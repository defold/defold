import unittest, sys

sys.path.append('src')
import bob

class TestBob(unittest.TestCase):

    def assertSetEquals(self, l1, l2):
        self.assertEquals(set(l1), set(l2))

    def test_collect_inputs(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/*.c'],
            includes = ['test_data/include'])
build(p)
'''
        l = bob.exec_script(script)
        p = l['p']
        self.assertEqual(set(['test_data/main.c', 'test_data/util.c']),
                         set(p['inputs']))


    def test_create_tasks(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/main.c'],
            includes = ['test_data/include'])
build(p)
'''
        l = bob.exec_script(script)
        tasks = l['p']['tasks']
        self.assertEqual(1, len(tasks))
        t = tasks[0]

        self.assertSetEquals(['test_data/main.c'], t['inputs'])
        self.assertSetEquals(['build/test_data/main.o'], t['outputs'])

    def test_scan(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/util.c'],
            includes = ['test_data/include'])
build(p)
'''
        l = bob.exec_script(script)
        t = l['p']['tasks'][0]

        self.assertSetEquals(['test_data/util.h', 'test_data/include/misc.h'],
                             t['dependencies'])

    def test_file_signatures(self):
        script = '''
p = project(bld_dir = "build",
            globs = ['test_data/*.c'],
            includes = ['test_data/include'])
build(p)
'''
        l = bob.exec_script(script)
        sigs = l['p']['file_signatures']
        files = set(['test_data/util.c',
                     'test_data/util.h',
                     'test_data/main.c',
                     'test_data/include/misc.h'])

        self.assertSetEquals(files, sigs.keys())
        from bob import sha1_file
        for x in files:
            self.assertEquals(sha1_file(x), sigs[x])

    def test_task_signatures(self):
        script1 = '''
p = project(bld_dir = "build",
            globs = ['test_data/main.c'],
            includes = ['test_data/include'])
build(p)
'''
        l1 = bob.exec_script(script1)

        script2 = '''
p = project(bld_dir = "build",
            globs = ['test_data/main.c'],
            includes = ['test_data/include', '/opt/include'])
build(p)
'''
        l2 = bob.exec_script(script2)

        t1 = l1['p']['tasks'][0]
        t2 = l2['p']['tasks'][0]
        self.assertNotEquals(t1['sig'], t2['sig'])

if __name__ == '__main__':
    unittest.main()
