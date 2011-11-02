# This file will be moved to gamesys or similar

import os
from bob import add_task, task, change_ext, execute_command, find_cmd, register

def texture_file(prj, tsk):
    execute_command(prj, tsk, 'python ${texc} ${inputs[0]} -o ${outputs[0]}')

def texture_factory(prj, input):
    return add_task(prj, task(function = texture_file,
                              name = 'texture',
                              inputs = [input],
                              outputs = [change_ext(prj, input, '.texturec')]))

def fontmap_file(prj, tsk):
    execute_command(prj, tsk, 'java -classpath ${classpath} com.dynamo.render.Fontc ${inputs[0]} ${content_root} ${outputs[0]}')

def fontmap_factory(prj, input):
    return add_task(prj, task(function = fontmap_file,
                              name = 'fontmap',
                              inputs = [input],
                              outputs = [change_ext(prj, input, '.fontmapc')]))

def conf(prj):
    find_cmd(prj, 'texc.py', var='texc')
    register(prj, '.png .tga', texture_factory)
    register(prj, '.font', fontmap_factory)

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        raise Exception('DYNAMO_HOME not set')

    prj['dynamo_home'] = dynamo_home

    classpath = [ dynamo_home + x for x in ['/ext/share/java/protobuf-java-2.3.0.jar',
                                            '/share/java/ddf.jar',
                                            '/share/java/render.jar',
                                            '/share/java/fontc.jar']]
    prj['classpath'] = os.pathsep.join(classpath)
    prj['content_root'] = '.'

