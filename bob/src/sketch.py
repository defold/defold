from glob import glob
from os.path import splitext

def set_default(d, **kwargs):
    for k, v in kwargs.iteritems():
        if not d.has_key(k):
            d[k] = v
    return d

def project(**kwargs):
    return set_default(dict(kwargs), globs = [])

def task(**kwargs):
    return set_default(dict(kwargs))

def change_ext(input, ext):
    return splitext(input)[0] + ext

def collect_inputs(p):
    inputs = set()
    for g in p["globs"]:
        inputs = inputs.union(set(glob(g)))
    p['inputs'] = list(inputs)

def create_tasks(p):
    tasks = []
    for input in p['inputs']:
        tg = p['task_gens'][splitext(input)[1]]
        tasks.append(tg(input))
    p['tasks'] = tasks

def build(p):
    collect_inputs(p)
    create_tasks(p)

##################

def header_scanner():
    pass

def sprite_file(input, output, transform):
    write_proto(transform_sprite(load_proto(input)), output)

def sprite(input):
    return task(function = sprite_file,
                inputs = [input],
                outputs = [change_ext(input, ".spritec")])

def c_lang_file(input, output, transform):
    print 'c_lang_file!', input, output, trasnform
    #write_proto(transform_sprite(load_proto(input)), output)

def c_lang(input):
    return task(function = c_lang_file,
                inputs = [input],
                outputs = [change_ext(input, ".o")])

p = project(bld_dir = "build",
            task_gens = {'.sprite': sprite,
                         '.c' : c_lang },
            scanners = [[ header_scanner, ".h"]],
            globs = ['test/*.c'])

build(p)
import pprint

pprint.pprint(p)
#print g_project

#(def project
#  { :bld-dir "build"
#    :tasks-gens [sprite, gameobject]
#    :scanners [ [glsl-scanner ".vp"]
#                [glsl-scanner ".fp"] ]
#    :globs "content/**" } )
