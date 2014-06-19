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
    Utils.def_attrs(self, use_compression = False)

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
    if self.use_compression:
        arcc.env.append_value('ARCCFLAGS', ['-c'])

Task.simple_task_type('resource_jarchive', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.archive.ArchiveBuilder ${ARCCFLAGS} ${TGT} ${COMPRESS} ${SRC}',
                      color='PINK',
                      shell=False)

# Copy-paste of archive above
# This is the java-version of the archive builder
# Currently only used for unit-test and it will probably stay that way

@feature('jarchive')
@before('apply_core')
def apply_jarchive(self):
    Utils.def_attrs(self, archive_target = None)
    Utils.def_attrs(self, use_compression = False)

@feature('jarchive')
@after('apply_core')
def apply_jarchive_file(self):
    if not self.archive_target:
        error('archive_target not specified')
        return

    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob.jar',
                 # NOTE: Only needed when running within resource-project.
                 # Should be fixed somehow... in configure perhaps?
                 'default/src/java']


    out = self.path.find_or_declare(self.archive_target)
    arcc = self.create_task('resource_jarchive')
    arcc.env['CLASSPATH'] = os.pathsep.join(classpath)

    inputs = []
    for t in self.tasks:
        if t != arcc:
            arcc.set_run_after(t)
            inputs += t.outputs
    arcc.inputs = inputs
    arcc.outputs = [out]
    arcc.env['ARCCFLAGS'] = self.path.bldpath(self.env)
    if self.use_compression:
        arcc.env['COMPRESS'] = '-c';

def detect(conf):
    conf.find_file('arcc.py', var='ARCC', mandatory = True)
