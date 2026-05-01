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

from waflib.TaskGen import extension
import waflib.Tools.c, waflib.Tools.cxx

# objective-c++ support
@extension('.mm')
def objcxx_hook(self, node):
    tsk = waflib.Tools.cxx.cxx_hook(self, node)
    tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCXX'])
    tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])

# objective-c support
@extension('.m')
def objc_hook(self, node):
    tsk = waflib.Tools.c.c_hook(self, node)
    tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCC'])
    tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])
