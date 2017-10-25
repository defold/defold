import os, sys

ANDROID_HOME="/Users/mathiaswesterdahl/android/android-ndk-r10e"

EXT=['.a', '.so']

def search_dir(path):
       libs = set()
       for root, dirs, files in os.walk(path):
               for f in files:
                       fullpath = os.path.join(root, f)
                       if 'x86' in fullpath:
                               continue
                       if 'mips' in fullpath:
                               continue
                       if 'samples' in fullpath:
                               continue
                       name, ext = os.path.splitext(f)
                       if not ext in EXT:
                               continue
                       if name.startswith("lib"):
                               name = name[3:]

                       name = name.replace('+', '\\\\+')

                       libs.add(name)
       return libs

libs = sorted(search_dir(ANDROID_HOME))

print "allowedLibs: [", ",".join( map(lambda x: "\"%s\"" % x, libs ) ), "]"