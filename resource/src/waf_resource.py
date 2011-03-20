import os, re
import Task, TaskGen
from TaskGen import extension, feature, after, before
from Logs import error
import Utils

Task.simple_task_type('resource_archive', 'python ${ARCC} ${ARCCFLAGS} -o ${TGT} ${SRC}',
                      color='PINK',
                      shell=False)

@feature('archive')
@before('apply_core')
def apply_archive(self):
    Utils.def_attrs(self, archive_target = None)

@feature('archive')
@after('apply_core')
def apply_archive_file(self):
    if not self.archive_target:
        error('archive_target not specified')
        return

    out = self.path.find_or_declare(self.archive_target)
    arcc = self.create_task('resource_archive')
    inputs = []
    for t in self.tasks:
        if t != arcc:
            arcc.set_run_after(t)
            inputs += t.outputs
    arcc.inputs = inputs
    arcc.outputs = [out]
    arcc.env['ARCCFLAGS'] = ['-r', self.path.bldpath(self.env)]

def detect(conf):
    conf.find_file('arcc.py', var='ARCC', mandatory = True)
