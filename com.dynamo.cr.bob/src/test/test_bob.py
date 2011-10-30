import unittest, sys

sys.path.append('src')
import bob

class TestBob(unittest.TestCase):

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

if __name__ == '__main__':
    unittest.main()
