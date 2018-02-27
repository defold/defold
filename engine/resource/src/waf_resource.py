import os, re
import Task, TaskGen
from TaskGen import extension, feature, after, before
from Logs import error
import Utils

CONST_ARCHIVEBUILDER      = '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.archive.ArchiveBuilder'
CONST_ARCHIVEBUILDER_ARGS = '${ARCHIVEBUILDER_ROOT} ${ARCHIVEBUILDER_OUTPUT} ${ARCHIVEBUILDER_FLAGS} ${SRC}'
Task.simple_task_type('resource_archive', '%s %s' % (CONST_ARCHIVEBUILDER, CONST_ARCHIVEBUILDER_ARGS),
    color='PINK', shell=False)

@feature('barchive')
@before('apply_core')
def apply_barchive_before(self):
    Utils.def_attrs(self, source_root = None)
    Utils.def_attrs(self, resource_name = None)
    Utils.def_attrs(self, use_compression = False)

@feature('barchive')
@after('apply_core')
def apply_barchive_after(self):
    if self.source_root is None:
        return error('source_root is not specified!')
    if self.resource_name is None:
        return error('resource_name is not specified!')

    builder = self.create_task('resource_archive')

    builder.inputs  = []
    for task in self.tasks:
        if task != builder:
            builder.set_run_after(task)
            builder.inputs.extend(task.outputs)

    builder.outputs = []
    for extension in ['dmanifest', 'arci', 'arcd', 'public']:
        current_filepath = '%s.%s' % (self.resource_name, extension)
        current_output = self.path.find_or_declare(current_filepath)
        builder.outputs.append(current_output)

    classpath    = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar', 'default/src/java']
    builder.env['CLASSPATH'] = os.pathsep.join(classpath)

    arg_root     = self.source_root
    arg_output   = os.path.abspath(os.path.join('build', self.path.bldpath(self.env), self.resource_name))

    builder.env.append_value('ARCHIVEBUILDER_ROOT', [arg_root])
    builder.env.append_value('ARCHIVEBUILDER_OUTPUT', [arg_output])
    if self.use_compression:
        builder.env.append_value('ARCHIVEBUILDER_FLAGS', ['-c'])
