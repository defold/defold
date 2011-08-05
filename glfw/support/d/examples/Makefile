##########################################################################
# This makefile builds the example programs using the Digital Mars D
# compiler. You need to copy the .lib files from support\d\lib to your
# Digital Mars C/C++ compiler/linker lib folder (dm\lib) before compiling
# the programs.
##########################################################################

DC     = dmd
IMPORTSDIR = ..\imports
DFLAGS = -O -I$(IMPORTSDIR)
LIBDIR = ..\lib
LIBS = $(LIBDIR)\glfwdll.lib $(LIBDIR)\glu32.lib $(LIBDIR)\opengl32.lib


all: boing.exe gears.exe listmodes.exe mipmaps.exe mthello.exe mtbench.exe \
     particles.exe pong3d.exe splitview.exe triangle.exe wave.exe


clean:
	del *.exe *.obj *.map


boing.exe: boing.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

gears.exe: gears.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

listmodes.exe: listmodes.d $(IMPORTSDIR)\glfw.d
	$(DC) $(DFLAGS) -of$@ $<d $(IMPORTSDIR)\glfw.d $(LIBS)

mipmaps.exe: mipmaps.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

mthello.exe: mthello.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

mtbench.exe: mtbench.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

particles.exe: particles.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

pong3d.exe: pong3d.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

splitview.exe: splitview.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

triangle.exe: triangle.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

wave.exe: wave.d
	$(DC) $(DFLAGS) -of$@ $<d $(LIBS)

