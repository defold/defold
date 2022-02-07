# Copyright 2022 The Defold Foundation
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

from TaskGen import extension
import cc, cxx

# objective-c++ support
EXT_OBJCXX = ['.mm']
@extension(EXT_OBJCXX)
def objcxx_hook(self, node):
    tsk = cxx.cxx_hook(self, node)
    tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCXX'])
    tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])

# objective-c support
EXT_OBJC = ['.m']
@extension(EXT_OBJC)
def objc_hook(self, node):
    tsk = cc.c_hook(self, node)
    tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCC'])
    tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])
