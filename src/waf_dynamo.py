import os, sys

def configure(conf):
    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")

    dynamo_ext = os.path.join(dynamo_home, "ext")

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "linux" or platform == "darwin":
        conf.env['CXXFLAGS']='-g -D__STDC_LIMIT_MACROS -DDDF_EXPOSE_DESCRIPTORS'
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS']
        conf.env.append_value('LINKFLAGS', '/DEBUG')

    # TODO: WTF!!
    conf.env.append_value('CPPPATH', "../src")
    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include", platform))
    conf.env.append_value('CPPPATH', ".")

    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_home, "lib"))

    