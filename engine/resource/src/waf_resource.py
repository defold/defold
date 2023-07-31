# Copyright 2020-2023 The Defold Foundation
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

import os, re
import waflib.Task, waflib.TaskGen
from waflib.TaskGen import extension, feature, after, before
from waflib.Logs import error
import waflib.Utils

CONST_ARCHIVEBUILDER      = '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.archive.ArchiveBuilder'
CONST_ARCHIVEBUILDER_ARGS = '${ARCHIVEBUILDER_ROOT} ${ARCHIVEBUILDER_OUTPUT} ${ARCHIVEBUILDER_FLAGS} ${SRC}'
waflib.Task.task_factory('resource_archive', '%s %s' % (CONST_ARCHIVEBUILDER, CONST_ARCHIVEBUILDER_ARGS),
    color='PINK', shell=False)

@feature('barchive')
@before('apply_core')
def apply_barchive_before(self):
    waflib.Utils.def_attrs(self, source_root = None)
    waflib.Utils.def_attrs(self, resource_name = None)
    waflib.Utils.def_attrs(self, use_compression = False)

@feature('barchive')
@after('apply_core')
def apply_barchive_after(self):
    if self.source_root is None:
        return error('source_root is not specified!')
    if self.resource_name is None:
        return error('resource_name is not specified!')

    builder = self.create_task('resource_archive')
    builder.inputs = []

    has_live_update = False

    # JG: This is a workaround for how this process worked in waf 1.59
    #     where we expected the tasks to already be available.
    #     I'm not 100% sure why the new waf doesn't pick them up automatically,
    #     so instead we generate the process manually here.
    self.source = waflib.Utils.to_list(self.source)
    for x in self.source:
        if isinstance(x, str):
            x = self.path.make_node(x)

        has_live_update = has_live_update or 'liveupdate' in x.name

        hook = self.get_hook(x)
        hook(self, x)

    # We need to clear the source so we don't generate outputs more than once
    self.source = []

    for task in self.tasks:
        if task != builder:
            builder.set_run_after(task)
            builder.inputs.extend(task.outputs)

    extensions = ['dmanifest', 'arci', 'arcd', 'public', 'manifest_hash']
    if has_live_update:
        extensions.append('zip')

    builder.outputs = []
    for extension in extensions:
        current_filepath = '%s.%s' % (self.resource_name, extension)
        current_output = self.path.find_or_declare(current_filepath)
        builder.outputs.append(current_output)

    classpath    = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar', 'src/java']
    builder.env['CLASSPATH'] = os.pathsep.join(classpath)

    arg_root     = self.path.get_bld().abspath()
    arg_output   = os.path.join(self.path.get_bld().abspath(), self.resource_name)

    builder.env.append_value('ARCHIVEBUILDER_ROOT', [arg_root])
    builder.env.append_value('ARCHIVEBUILDER_OUTPUT', [arg_output])
    builder.env.append_value('ARCHIVEBUILDER_FLAGS', ['-m'])
    if self.use_compression:
        builder.env.append_value('ARCHIVEBUILDER_FLAGS', ['-c'])
