#!/usr/bin/env python
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

import os
import glob
import run
from waflib import ConfigSet
from waflib import Build

def load_build_context(directory):
    cachedir = os.path.join(directory,Build.CACHE_DIR)
    return ConfigSet.ConfigSet(os.path.join(cachedir,'_cache.py'))

def glob_files(patterns, cwd=None):
    oldcwd=os.getcwd()
    os.chdir(cwd)
    out=[]
    try:
        if isinstance(patterns,str):
            patterns=[patterns]
        for pattern in patterns:
            out.extend(glob.glob(pattern))
    finally:
        os.chdir(oldcwd)
    return out

def create_tar(files, cwd, target):
    cmd = 'tar zcf %s %s' % (target, ' '.join(files))
    run.command(cmd.split(), cwd=cwd)
