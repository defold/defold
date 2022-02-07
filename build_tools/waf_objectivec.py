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
