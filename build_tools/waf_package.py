#!/usr/bin/env python
# Copyright 2020-2022 The Defold Foundation
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
import Constants
import Environment
import Node
import Utils

def load_build_context(directory):
    cachedir = os.path.join(directory,Constants.CACHE_DIR)
    return Environment.Environment(os.path.join(cachedir,'default.cache.py'))


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

# Original from  wafadmin/Build.py install_files:
# I couldn't get the original code to work as I needed, so I made a small change to it.
def package_install_files(self, path, files, env=None, chmod=Constants.O644, relative_trick=False, cwd=None):
    if env:
        assert isinstance(env,Environment.Environment),"invalid parameter"
    else:
        env=self.env
    if not path:return[]
    if not cwd:
        cwd=self.path
    if isinstance(files,str)and'*'in files:
        gl=cwd.abspath()+os.sep+files
        lst=glob.glob(gl)
    else:
        lst=Utils.to_list(files)
    destpath=self.get_install_path(path,env)
    Utils.check_dir(destpath)
    installed_files=[]
    for filename in lst:
        if isinstance(filename,str)and os.path.isabs(filename):
            alst=Utils.split_path(filename)
            destfile=os.path.join(destpath,alst[-1])
        else:
            if isinstance(filename,Node.Node):
                nd=filename
            else:
                # DEFOLD: This is the fix to make the relative paths work our way
                fullpath=os.path.join(self.path.abspath(),filename)
                filename=os.path.relpath(fullpath,cwd.abspath())
                # DEFOLD: <- end fix
                nd=cwd.find_resource(filename)
            if not nd:
                raise Utils.WafError("Unable to install the file %r (not found in %s)"%(filename,cwd))
            if relative_trick:
                destfile=os.path.join(destpath,filename)
                Utils.check_dir(os.path.dirname(destfile))
            else:
                destfile=os.path.join(destpath,nd.name)
            filename=nd.abspath(env)
        if self.do_install(filename,destfile,chmod):
            installed_files.append(destfile)
    return installed_files


def create_tar(files, cwd, target):
    cmd = 'tar zcf %s %s' % (target, ' '.join(files))
    run.command(cmd.split(), cwd=cwd)

