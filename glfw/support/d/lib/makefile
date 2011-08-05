all: glfwdll.lib opengl32.lib glu32.lib

IFLAGS =

glfwdll.lib: glfwdll.def
	implib $(IFLAGS) $@ glfwdll.def

opengl32.lib: opengl32.def
	implib $(IFLAGS) $@ opengl32.def

glu32.lib: glu32.def
	implib $(IFLAGS) $@ glu32.def
