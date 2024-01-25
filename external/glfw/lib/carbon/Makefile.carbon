##########################################################################
# Makefile for GLFW on Mac OS X using GCC (Apple SDK).
#-------------------------------------------------------------------------
# To compile GLFW using this makefile, run:
#  make -f Makefile.carbon
##########################################################################

##########################################################################
# Installation prefix (default to /usr/local)
##########################################################################
PREFIX ?= /usr/local


##########################################################################
# Default: Build GLFW static and shared library
##########################################################################
all: libglfw.a libglfw.dylib


##########################################################################
# Compiler settings
##########################################################################
CC           = gcc
CFLAGS       = -c -I. -I.. -Wall -O2 -fno-common -g -m32


##########################################################################
# Library builder settings
##########################################################################
AR          = ar
SED         = sed
INSTALL     = install
ARFLAGS     = -rcs
RANLIB      = ranlib
DYLIBFLAGS  = -framework AGL -framework Carbon -framework OpenGL \
              -dynamiclib -Wl,-single_module -compatibility_version 1 \
              -current_version 1 -install_name @executable_path/libglfw.dylib -m32


##########################################################################
# Install GLFW header and static library
##########################################################################
install: libglfw.a libglfw.pc
	$(INSTALL) -d $(PREFIX)/lib
	$(INSTALL) -c -m 644 libglfw.a $(PREFIX)/lib/libglfw.a
	$(RANLIB) $(PREFIX)/lib/libglfw.a
	$(INSTALL) -d $(PREFIX)/include/GL
	$(INSTALL) -c -m 644 ../../include/GL/glfw.h $(PREFIX)/include/GL/glfw.h
	$(INSTALL) -d $(PREFIX)/lib/pkgconfig
	$(INSTALL) -c -m 644 libglfw.pc $(PREFIX)/lib/pkgconfig/libglfw.pc


##########################################################################
# Object files for the GLFW library
##########################################################################
OBJS = \
       enable.o \
       fullscreen.o \
       glext.o \
       image.o \
       init.o \
       input.o \
       joystick.o \
       stream.o \
       tga.o \
       thread.o \
       time.o \
       window.o \
       carbon_enable.o \
       carbon_fullscreen.o \
       carbon_glext.o \
       carbon_init.o \
       carbon_joystick.o \
       carbon_thread.o \
       carbon_time.o \
       carbon_window.o


##########################################################################
# Rule for building libglfw.pc
##########################################################################
libglfw.pc: libglfw.pc.in
	$(SED) -e 's,\@PREFIX\@,$(PREFIX),' libglfw.pc.in > libglfw.pc


##########################################################################
# Rule for building static library
##########################################################################
libglfw.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@


##########################################################################
# Rule for building shared library
##########################################################################
libglfw.dylib: $(OBJS)
	$(CC) -o $@ $(DYLIBFLAGS) $(OBJS)


##########################################################################
# Rule for cleaning up generated files
##########################################################################
clean:
	@rm -f *.o libglfw.a libglfw.dylib libglfw.pc


##########################################################################
# Rules for building library object files
##########################################################################
enable.o: ../enable.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../enable.c

fullscreen.o: ../fullscreen.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../fullscreen.c

glext.o: ../glext.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../glext.c

image.o: ../image.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../image.c

init.o: ../init.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../init.c

input.o: ../input.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../input.c

joystick.o: ../joystick.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../joystick.c

stream.o: ../stream.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../stream.c

tga.o: ../tga.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../tga.c

thread.o: ../thread.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../thread.c

time.o: ../time.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../time.c

window.o: ../window.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ ../window.c

carbon_enable.o: carbon_enable.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_enable.c

carbon_fullscreen.o: carbon_fullscreen.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_fullscreen.c

carbon_glext.o: carbon_glext.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_glext.c

carbon_init.o: carbon_init.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_init.c

carbon_joystick.o: carbon_joystick.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_joystick.c

carbon_thread.o: carbon_thread.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_thread.c

carbon_time.o: carbon_time.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_time.c

carbon_window.o: carbon_window.c ../internal.h platform.h
	$(CC) $(CFLAGS) -o $@ carbon_window.c

