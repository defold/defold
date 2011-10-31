import unittest, sys
from os import unlink
from pprint import pprint

sys.path.append('src')
import bob

sys.path.append('build/default')
import test_bob_ddf_pb2

class TestBob(unittest.TestCase):

    def assertSetEquals(self, l1, l2):
        self.assertEquals(set(l1), set(l2))

    def test_collect_inputs(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/*.c'],
            includes = ['tmp/test_data/include'])
build(p, run=False)
'''
        l = bob.exec_script(script)
        p = l['p']
        self.assertEqual(set(['tmp/test_data/main.c', 'tmp/test_data/util.c', 'tmp/test_data/error.c']),
                         set(p['inputs']))


    def test_create_tasks(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/main.c'],
            includes = ['tmp/test_data/include'])
build(p, run=False)
'''
        l = bob.exec_script(script)
        tasks = l['p']['tasks']
        self.assertEqual(1, len(tasks))
        t = tasks[0]

        self.assertSetEquals(['tmp/test_data/main.c'], t['inputs'])
        self.assertSetEquals(['tmp_build/tmp/test_data/main.o'], t['outputs'])

    def test_scan(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/util.c'],
            includes = ['tmp/test_data/include'])
build(p, run=False)
'''
        l = bob.exec_script(script)
        t = l['p']['tasks'][0]

        self.assertSetEquals(['tmp/test_data/util.h', 'tmp/test_data/include/misc.h'],
                             t['dependencies'])

    def test_file_signatures(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/*.c'],
            includes = ['tmp/test_data/include'])
build(p, run=False)
'''
        l = bob.exec_script(script)
        sigs = l['p']['file_signatures']
        files = set(['tmp/test_data/util.c',
                     'tmp/test_data/util.h',
                     'tmp/test_data/main.c',
                     'tmp/test_data/error.c',
                     'tmp/test_data/include/misc.h'])

        self.assertSetEquals(files, sigs.keys())
        from bob import sha1_file
        for x in files:
            self.assertEquals(sha1_file(x), sigs[x])

    def test_task_signatures(self):
        script1 = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/main.c'],
            includes = ['tmp/test_data/include'])
build(p, run=False)
'''
        l1 = bob.exec_script(script1)

        script2 = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/main.c'],
            includes = ['tmp/test_data/include', '/opt/include'])
build(p, run=False)
'''
        l2 = bob.exec_script(script2)

        t1 = l1['p']['tasks'][0]
        t2 = l2['p']['tasks'][0]
        self.assertNotEquals(t1['sig'], t2['sig'])

    def test_c_compile(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/main.c'],
            includes = ['tmp/test_data/include'])
r = build(p)
'''
        l = bob.exec_script(script)
        info = l['r'][0]
        self.assertEquals(True, info['run'])
        self.assertEquals(0, info['code'])

        l = bob.exec_script(script)
        info = l['r'][0]
        self.assertEquals(False, info['run'])
        self.assertEquals(0, info['code'])

        unlink('tmp_build/tmp/test_data/main.o')
        l = bob.exec_script(script)
        info = l['r'][0]
        self.assertEquals(True, info['run'])
        self.assertEquals(0, info['code'])

        # update header
        f = open('tmp/test_data/include/misc.h', 'wb')
        f.write('//some new data')
        f.close()

        l = bob.exec_script(script)
        info = l['r'][0]
        self.assertEquals(True, info['run'])
        self.assertEquals(0, info['code'])

    def test_c_compile_error(self):
        script = '''
p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/error.c'],
            includes = ['tmp/test_data/include'])
r = build(p)
'''
        l = bob.exec_script(script)
        r = l['r']
        info = l['r'][0]
        self.assertEquals(True, info['run'])
        self.assert_(info['code'] > 0, 'code > 0')
        self.assertEquals('', info['stdout'])
        self.assertNotEquals('', info['stderr'])

        # run again. File file so it should recompile
        l = bob.exec_script(script)
        r = l['r']
        info = l['r'][0]
        self.assertEquals(True, info['run'])

        # "fix" file
        f = open('tmp/test_data/error.c', 'wb')
        f.write('\n')
        f.close()

        l = bob.exec_script(script)
        r = l['r']
        info = l['r'][0]
        self.assertEquals(True, info['run'])
        self.assertEquals(0, info['code'])

        self.assertEquals('', info['stdout'])
        self.assertEquals('', info['stderr'])

    def test_proto_task(self):
        script = '''

def transform_bob(msg):
    msg.resource = msg.resource + 'c'

p = project(bld_dir = "tmp_build",
            globs = ['tmp/test_data/*.bob'])
p['task_gens']['.bob'] = make_proto('test_bob_ddf_pb2', 'TestBob', transform_bob)
r = build(p)
'''
        l = bob.exec_script(script)
        r = l['r']
        info = l['r']

        tb = test_bob_ddf_pb2.TestBob()
        f = open('tmp_build/tmp/test_data/test.bobc', 'rb')
        tb.MergeFromString(f.read())
        f.close()

        self.assertEquals(tb.resource, u'some_resource.extc')

if __name__ == '__main__':
    import os, shutil
    if os.path.exists('tmp'):
        shutil.rmtree('tmp')
    os.mkdir('tmp')
    os.mkdir('tmp/build')
    if os.path.exists('tmp_build'):
        shutil.rmtree('tmp_build')
    shutil.copytree('test_data', 'tmp/test_data')
    unittest.main()
