##########################################################################
# Makefile for GLFW test programs on Mac OS X using Apple GCC.
#-------------------------------------------------------------------------
# To compile the test files using this makefile, run:
#  make -f Makefile.carbon
##########################################################################

# Compiler settings
CC     = gcc
CFLAGS = -I../include -m32

# Linker settings
LIB       = ../lib/carbon/libglfw.a
SOLIB     = ../lib/carbon/libglfw.dylib
LFLAGS    = $(LIB) -framework AGL -framework OpenGL -framework Carbon -m32
SO_LFLAGS = $(SOLIB) -framework OpenGL -m32

# Default: Build all tests
all: Accuracy.app/Contents/MacOS/Accuracy \
     defaults \
     Dynamic.app/Contents/MacOS/Dynamic \
     Events.app/Contents/MacOS/Events \
     FSAA.app/Contents/MacOS/FSAA \
     FSInput.app/Contents/MacOS/FSInput \
     Iconify.app/Contents/MacOS/Iconify \
     joysticks \
     Peter.app/Contents/MacOS/Peter \
     ReOpen.app/Contents/MacOS/ReOpen \
     Tearing.app/Contents/MacOS/Tearing \
     version

Accuracy.app/Contents/MacOS/Accuracy: accuracy.c $(LIB)
	/bin/sh bundle.sh Accuracy
	$(CC) $(CFLAGS) accuracy.c $(LFLAGS) -o Accuracy.app/Contents/MacOS/Accuracy

defaults: defaults.c $(LIB)
	$(CC) $(CFLAGS) defaults.c $(LFLAGS) -o defaults

Dynamic.app/Contents/MacOS/Dynamic: dynamic.c $(SOLIB)
	/bin/sh bundle.sh Dynamic
	cp $(SOLIB) Dynamic.app/Contents/MacOS/
	$(CC) $(CFLAGS) dynamic.c $(SO_LFLAGS) -o Dynamic.app/Contents/MacOS/Dynamic

Events.app/Contents/MacOS/Events: events.c $(LIB)
	/bin/sh bundle.sh Events
	$(CC) $(CFLAGS) events.c $(LFLAGS) -o Events.app/Contents/MacOS/Events

FSAA.app/Contents/MacOS/FSAA: fsaa.c $(LIB)
	/bin/sh bundle.sh FSAA
	$(CC) $(CFLAGS) fsaa.c $(LFLAGS) -o FSAA.app/Contents/MacOS/FSAA

FSInput.app/Contents/MacOS/FSInput: fsinput.c $(LIB)
	/bin/sh bundle.sh FSInput
	$(CC) $(CFLAGS) fsinput.c $(LFLAGS) -o FSInput.app/Contents/MacOS/FSInput

Iconify.app/Contents/MacOS/Iconify: iconify.c $(LIB)
	/bin/sh bundle.sh Iconify
	$(CC) $(CFLAGS) iconify.c $(LFLAGS) -o Iconify.app/Contents/MacOS/Iconify

joysticks: joysticks.c $(LIB)
	$(CC) $(CFLAGS) joysticks.c $(LFLAGS) -o joysticks

Peter.app/Contents/MacOS/Peter: peter.c $(LIB)
	/bin/sh bundle.sh Peter
	$(CC) $(CFLAGS) peter.c $(LFLAGS) -o Peter.app/Contents/MacOS/Peter

ReOpen.app/Contents/MacOS/ReOpen: reopen.c $(LIB)
	/bin/sh bundle.sh ReOpen
	$(CC) $(CFLAGS) reopen.c $(LFLAGS) -o ReOpen.app/Contents/MacOS/ReOpen

Tearing.app/Contents/MacOS/Tearing: tearing.c $(LIB)
	/bin/sh bundle.sh Tearing
	$(CC) $(CFLAGS) tearing.c $(LFLAGS) -o Tearing.app/Contents/MacOS/Tearing

version: version.c $(LIB)
	$(CC) $(CFLAGS) version.c $(LFLAGS) -o version

clean:
	rm -rf Accuracy.app defaults Dynamic.app Events.app FSAA.app FSInput.app \
           Iconify.app joysticks Peter.app ReOpen.app Tearing.app version

