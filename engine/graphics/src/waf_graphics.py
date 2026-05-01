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

import sys, os
import waflib.Task, waflib.TaskGen
from waflib.TaskGen import extension

def configure(conf):
    conf.find_file('texc.py', var='TEXC', mandatory = True)

waflib.Task.task_factory('texture', 'python ${TEXC} ${SRC} -o ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=True)

@extension('.png', '.jpg')
def png_file(self, node):
    texture = self.create_task('texture')
    texture.set_inputs(node)
    out = node.change_ext('.texturec')
    texture.set_outputs(out)
