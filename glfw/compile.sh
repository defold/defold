#!/bin/sh

##########################################################################
# compile.sh - Unix/X11 configuration script
# $Date: 2007-09-20 23:16:57 $
# $Revision: 1.16 $
#
# This is a minimalist configuration script for GLFW, which is used to
# determine the availability of certain features.
#
# This script is not very nice at all (especially the Makefile generation
# is very ugly and hardcoded). Hopefully it will be cleaned up in the
# future, but for now it does a pretty good job.
##########################################################################

##########################################################################
# Check arguments
##########################################################################
silent=no
for arg in "$@"; do
{
  case "$arg" in
    # Silent?
    -q | -quiet | --quiet | --quie | --qui | --qu | --q \
    | -silent | --silent | --silen | --sile | --sil)
      silent=yes ;;
  esac;
}
done;


##########################################################################
# Misc.
##########################################################################

self=$0

# File descriptor usage:
# 0 standard input
# 1 file creation
# 2 errors and warnings
# 5 compiler messages saved in config.log
# 6 checking for... messages and results
exec 5>./config.log
if [ "x$silent" = xyes ]; then
  exec 6>/dev/null
else
  exec 6>&1
fi

echo "\
This file contains any messages produced by compilers while
running $self, to aid debugging if $self makes a mistake.
" 1>&5


##########################################################################
# Default compiler settings
##########################################################################
if [ "x$CC" = x ]; then
  CC=cc
fi

# These will contain flags needed by both the GLFW library and programs
# They are also used by the compile and link tests below
# Note that CFLAGS and LFLAGS remain unmodified and are checked again
# before file generation
GLFW_CFLAGS="$CFLAGS"
GLFW_LFLAGS="$LFLAGS -lGL"

# These will contain flags needed by the GLFW library
GLFW_LIB_CFLAGS=
GLFW_LIB_LFLAGS=

# These will contain flags needed by programs using GLFW
GLFW_BIN_CFLAGS=
GLFW_BIN_LFLAGS=

# This will contain flags needed by the GLFW shared library
SOFLAGS=


##########################################################################
# Add system-specific flags
##########################################################################
echo -n "Checking what kind of system this is... " 1>&6

case "x`uname 2> /dev/null`" in
xLinux)
  GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_USE_LINUX_JOYSTICKS"
  SOFLAGS="-shared -Wl,-soname,libglfw.so"
  echo "Linux" 1>&6
  ;;
xDarwin)
  SOFLAGS="-flat_namespace -undefined suppress"
  echo "Mac OS X" 1>&6
  ;;
*)
  SOFLAGS="-shared -soname libglfw.so"
  echo "Generic Unix" 1>&6
  ;;
esac


##########################################################################
# Check for X11 library directory
##########################################################################
echo -n "Checking for X11 libraries location... " 1>&6

if [ -r "/usr/X11/lib" ]; then
  GLFW_LFLAGS="$GLFW_LFLAGS -L/usr/X11/lib"
  GLFW_CFLAGS="-I/usr/X11/include $GLFW_CFLAGS"
  echo "/usr/X11/lib" 1>&6
elif [ -r "/usr/X11R7/lib" ]; then
  GLFW_LFLAGS="$GLFW_LFLAGS -L/usr/X11R7/lib"
  GLFW_CFLAGS="-I/usr/X11R7/include $GLFW_CFLAGS"
  echo "/usr/X11R7/lib" 1>&6
elif [ -r "/usr/X11R6/lib" ]; then
  GLFW_LFLAGS="$GLFW_LFLAGS -L/usr/X11R6/lib"
  GLFW_CFLAGS="-I/usr/X11R6/include $GLFW_CFLAGS"
  echo "/usr/X11R6/lib" 1>&6
elif [ -r "/usr/X11R5/lib" ]; then
  GLFW_LFLAGS="$GLFW_LFLAGS -L/usr/X11R5/lib"
  GLFW_CFLAGS="-I/usr/X11R5/include $GLFW_CFLAGS"
  echo "/usr/X11R5/lib" 1>&6
elif [ -r "/opt/X11R6/lib" ]; then
  # This location is used on QNX
  GLFW_LFLAGS="$GLFW_LFLAGS -L/opt/X11R6/lib"
  GLFW_CFLAGS="-I/opt/X11R6/include $GLFW_CFLAGS"
  echo "/opt/X11R6/lib" 1>&6
elif [ -r "/usr/X/lib" ]; then
  GLFW_LFLAGS="$GLFW_LFLAGS -L/usr/X/lib"
  GLFW_CFLAGS="-I/usr/X/include $GLFW_CFLAGS"
  echo "/usr/X/lib" 1>&6
else
  # TODO: Detect and report X11R7 in /usr/lib
  echo "unknown (assuming linker will find them)" 1>&6
fi


##########################################################################
# Compilation commands
##########################################################################
compile='$CC -c $GLFW_CFLAGS conftest.c 1>&5'
link='$CC -o conftest $GLFW_CFLAGS conftest.c $GLFW_LFLAGS 1>&5'


##########################################################################
# Check if we are using GNU C (or something claiming it is)
##########################################################################
echo -n "Checking whether we are using GNU C... " 1>&6
echo "$self: checking whether we are using GNU C" >&5

cat > conftest.c <<EOF
#ifdef __GNUC__
  yes;
#endif
EOF

if { ac_try='$CC -E conftest.c'; { (eval echo $self: \"$ac_try\") 1>&5; (eval $ac_try) 2>&5; }; } | egrep yes >/dev/null 2>&1; then
  use_gcc=yes
else
  use_gcc=no
fi
rm -f conftest*

echo "$use_gcc" 1>&6


##########################################################################
# Check for X11 RandR availability
##########################################################################
echo -n "Checking for X11 RandR support... " 1>&6
echo "$self: Checking for X11 RandR support" >&5
has_xrandr=no

cat > conftest.c <<EOF
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

int main() {; return 0;}
EOF

if { (eval echo $self: \"$compile\") 1>&5; (eval $compile) 2>&5; }; then
  rm -rf conftest*
  has_xrandr=yes
else
  echo "$self: failed program was:" >&5
  cat conftest.c >&5
fi
rm -f conftest*

echo "$has_xrandr" 1>&6

if [ "x$has_xrandr" = xyes ]; then
  GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_XRANDR"
  GLFW_LIB_LFLAGS="$GLFW_LIB_LFLAGS -lXrandr"
fi


##########################################################################
# Check for X11 VidMode availability
##########################################################################
if [ "x$has_xrandr" = xno ]; then

  echo -n "Checking for X11 VidMode support... " 1>&6
  echo "$self: Checking for X11 VidMode support" >&5
  has_xf86vm=no

  cat > conftest.c <<EOF
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#if defined(__APPLE_CC__)
#error Not supported under Mac OS X
#endif

int main() {; return 0;}
EOF

  if { (eval echo $self: \"$compile\") 1>&5; (eval $compile) 2>&5; }; then
    rm -rf conftest*
    has_xf86vm=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
  rm -f conftest*

  echo "$has_xf86vm" 1>&6

  if [ "x$has_xf86vm" = xyes ]; then
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_XF86VIDMODE"
    GLFW_LIB_LFLAGS="$GLFW_LIB_LFLAGS -lXxf86vm -lXext"
  fi

fi


##########################################################################
# Check for pthread support
##########################################################################
echo -n "Checking for pthread support... " 1>&6
echo "$self: Checking for pthread support" >&5
has_pthread=no

cat > conftest.c <<EOF
#include <pthread.h>
int main() {pthread_t posixID; posixID=pthread_self(); return 0;}
EOF

# Save original values
CFLAGS_OLD="$GLFW_CFLAGS"
LFLAGS_OLD="$GLFW_LFLAGS"

# These will contain the extra flags, if any
CFLAGS_THREAD=
LFLAGS_THREAD=

# Try -pthread (most systems)
if [ "x$has_pthread" = xno ]; then
  CFLAGS_THREAD="-pthread"
  LFLAGS_THREAD="-pthread"
  GLFW_CFLAGS="$CFLAGS_OLD $CFLAGS_THREAD"
  GLFW_LFLAGS="$LFLAGS_OLD $LFLAGS_THREAD"
  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_pthread=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
fi

# Try -lpthread 
if [ "x$has_pthread" = xno ]; then
  CFLAGS_THREAD="-D_REENTRANT"
  LFLAGS_THREAD="-lpthread"
  GLFW_CFLAGS="$CFLAGS_OLD $CFLAGS_THREAD" 
  GLFW_LFLAGS="$LFLAGS_OLD $LFLAGS_THREAD"
  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_pthread=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
fi

# Try -lsocket (e.g. QNX)
if [ "x$has_pthread" = xno ]; then
  CFLAGS_THREAD=
  LFLAGS_THREAD="-lsocket"
  GLFW_CFLAGS="$CFLAGS_OLD $CFLAGS_THREAD"
  GLFW_LFLAGS="$LFLAGS_OLD $LFLAGS_THREAD"
  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_pthread=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
fi

# Restore original values
GLFW_CFLAGS="$CFLAGS_OLD"
GLFW_LFLAGS="$LFLAGS_OLD"

echo "$has_pthread" 1>&6

if [ "x$has_pthread" = xyes ]; then
  GLFW_CFLAGS="$GLFW_CFLAGS $CFLAGS_THREAD"
  GLFW_LFLAGS="$GLFW_LFLAGS $LFLAGS_THREAD"
  GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_PTHREAD"
fi


##########################################################################
# Check for sched_yield support
##########################################################################
if [ "x$has_pthread" = xyes ]; then

  echo -n "Checking for sched_yield... " 1>&6
  echo "$self: Checking for sched_yield" >&5
  has_sched_yield=no

  LFLAGS_OLD="$GLFW_LFLAGS"
  LFLAGS_THREAD=

  cat > conftest.c <<EOF
#include <pthread.h>
int main() {sched_yield(); return 0;}
EOF

  if { (eval echo $self: \"$compile\") 1>&5; (eval $compile) 2>&5; }; then
    has_sched_yield=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi

  if [ "x$has_sched_yield" = xno ]; then
    LFLAGS_THREAD="-lrt"
    GLFW_LFLAGS="$LFLAGS_OLD $LFLAGS_THREAD"
    if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
      rm -f conftest*
      has_sched_yield=yes
    else
      echo "$self: failed program was:" >&5
      cat conftest.c >&5
    fi
  fi

  GLFW_LFLAGS="$LFLAGS_OLD"

  echo "$has_sched_yield" 1>&6

  if [ "x$has_sched_yield" = xyes ]; then
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_SCHED_YIELD"
    GLFW_LIB_LFLAGS="$GLFW_LIB_LFLAGS $LFLAGS_THREAD"
  fi

fi


##########################################################################
# Check for glXGetProcAddressXXX availability
##########################################################################
echo -n "Checking for glXGetProcAddress variants... " 1>&6
echo "$self: Checking for glXGetProcAddress variants" >&5

has_glXGetProcAddress=no

if [ "x$has_glXGetProcAddress" = xno ]; then

  # Check for plain glXGetProcAddress
  cat > conftest.c <<EOF
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
int main() {void *ptr=(void*)glXGetProcAddress("glFun"); return 0;}
EOF

  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_glXGetProcAddress=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
  rm -f conftest*

  if [ "x$has_glXGetProcAddress" = xyes ]; then
    echo "glXGetProcAddress" 1>&6
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_GLXGETPROCADDRESS"
  fi
fi

if [ "x$has_glXGetProcAddress" = xno ]; then

  # Check for glXGetProcAddressARB
  cat > conftest.c <<EOF
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
int main() {void *ptr=(void*)glXGetProcAddressARB("glFun"); return 0;}
EOF

  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_glXGetProcAddress=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
  rm -f conftest*

  if [ "x$has_glXGetProcAddress" = xyes ]; then
    echo "glXGetProcAddressARB" 1>&6
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_GLXGETPROCADDRESSARB"
  fi
fi

if [ "x$has_glXGetProcAddress" = xno ]; then
  # Check for glXGetProcAddressEXT
  cat > conftest.c <<EOF
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
int main() {void *ptr=(void*)glXGetProcAddressEXT("glFun"); return 0;}
EOF

  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_glXGetProcAddress=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi
  rm -f conftest*

  if [ "x$has_glXGetProcAddress" = xyes ]; then
    echo "glXGetProcAddressEXT" 1>&6
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_GLXGETPROCADDRESSEXT"
  fi
fi

if [ "x$has_glXGetProcAddress" = xno ]; then
  echo "no" 1>&6
fi


##########################################################################
# Check for dlopen support if necessary
##########################################################################
if [ "x$has_glXGetProcAddress" = xno ]; then

  echo -n "Checking for dlopen... " 1>&6
  echo "$self: Checking for dlopen" >&5
  has_dlopen=no

  cat > conftest.c <<EOF
#include <dlfcn.h>
int main() {void *l=dlopen("libGL.so",RTLD_LAZY|RTLD_GLOBAL); return 0;}
EOF

  # First try without -ldl
  if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
    rm -rf conftest*
    has_dlopen=yes
  else
    echo "$self: failed program was:" >&5
    cat conftest.c >&5
  fi

  # Now try with -ldl if the previous attempt failed
  if [ "x$has_dlopen" = xno ]; then
    LFLAGS_OLD="$GLFW_LFLAGS"
    GLFW_LFLAGS="$GLFW_LFLAGS -ldl"
    if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
      rm -rf conftest*
      has_dlopen=yes
    else
      echo "$self: failed program was:" >&5
      cat conftest.c >&5
    fi
    GLFW_LFLAGS="$LFLAGS_OLD"
    if [ "x$has_dlopen" = xyes ]; then
      GLFW_LIB_LFLAGS="$GLFW_LIB_LFLAGS -ldl"
    fi
  fi
  rm -f conftest*

  echo "$has_dlopen" 1>&6

  if [ "x$has_dlopen" = xyes ]; then
    GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_DLOPEN"
  fi

fi


##########################################################################
# Check for sysconf support
##########################################################################
echo -n "Checking for sysconf... " 1>&6
echo "$self: Checking for sysconf" >&5
has_sysconf=no

cat > conftest.c <<EOF
#include <unistd.h>
#ifndef _SC_NPROCESSORS_ONLN
#ifndef _SC_NPROC_ONLN
#error Neither _SC_NPROCESSORS_ONLN nor _SC_NPROC_ONLN available
#endif
#endif
int main() {long x=sysconf(_SC_ARG_MAX); return 0; }
EOF

if { (eval echo $self: \"$link\") 1>&5; (eval $link) 2>&5; }; then
  rm -rf conftest*
  has_sysconf=yes
else
  echo "$self: failed program was:" >&5
  cat conftest.c >&5
fi
rm -f conftest*

echo "$has_sysconf" 1>&6

if [ "x$has_sysconf" = xyes ]; then
  GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_SYSCONF"
fi


##########################################################################
# Check for sysctl support
##########################################################################
echo -n "Checking for sysctl support... " 1>&6
echo "$self: Checking for sysctl support" >&5
has_sysctl=no

cat > conftest.c <<EOF
#include <sys/types.h>
#include <sys/sysctl.h>
#ifdef CTL_HW
#ifdef HW_NCPU
  yes;
#endif
#endif
EOF

if { ac_try='$CC -E conftest.c'; { (eval echo $self: \"$ac_try\") 1>&5; (eval $ac_try) 2>&5; }; } | egrep yes >/dev/null 2>&1; then
  has_sysctl=yes
fi
rm -f conftest*

echo "$has_sysctl" 1>&6

if [ "x$has_sysctl" = xyes ]; then
  GLFW_LIB_CFLAGS="$GLFW_LIB_CFLAGS -D_GLFW_HAS_SYSCTL"
fi


##########################################################################
# Last chance to change the flags before file generation
##########################################################################

GLFW_LIB_CFLAGS="-c -I. -I.. $GLFW_LIB_CFLAGS"
GLFW_BIN_CFLAGS="-I../include $GLFW_BIN_CFLAGS"

if [ "x$CFLAGS" = x ]; then
  if [ "x$use_gcc" = xyes ]; then
    GLFW_CFLAGS="$GLFW_CFLAGS -O2 -Wall"
  else
    GLFW_CFLAGS="$GLFW_CFLAGS -O"
  fi
fi

GLFW_LFLAGS="$GLFW_LFLAGS -lm"

GLFW_LIB_LFLAGS="$GLFW_LIB_LFLAGS -lX11"
GLFW_BIN_LFLAGS="-lGLU $GLFW_BIN_LFLAGS"


##########################################################################
# Create makefiles and pkg-config template file
##########################################################################

# ---------------------------------------------------------------------
# Create Makefile for GLFW library
# ---------------------------------------------------------------------

MKNAME='./lib/x11/Makefile.x11'

echo "Creating $MKNAME" 1>&6

echo "$self: Creating $MKNAME" >&5

cat > "$MKNAME" <<EOF
##########################################################################
# Automatically generated Makefile for GLFW
##########################################################################
CC           = $CC
CFLAGS       = $GLFW_LIB_CFLAGS $GLFW_CFLAGS
SOFLAGS      = $SOFLAGS
LFLAGS       = $GLFW_LIB_LFLAGS $GLFW_LFLAGS

EOF
cat './lib/x11/Makefile.x11.in' >>$MKNAME

# ---------------------------------------------------------------------
# Create Makefile for examples
# ---------------------------------------------------------------------

MKNAME='./examples/Makefile.x11'

echo "Creating $MKNAME" 1>&6

echo "$self: Creating $MKNAME" >&5

cat > "$MKNAME" <<EOF
##########################################################################
# Makefile for GLFW example programs on X11 (generated by compile.sh)
##########################################################################
CC     = $CC
CFLAGS = $GLFW_BIN_CFLAGS $GLFW_CFLAGS

LIB      = ../lib/x11/libglfw.a
SOLIB    = ../lib/x11/libglfw.so
LFLAGS   = \$(LIB) $GLFW_LIB_LFLAGS $GLFW_BIN_LFLAGS $GLFW_LFLAGS
SO_LFLAGS = \$(SOLIB) $GLFW_BIN_LFLAGS $GLFW_LFLAGS

EOF
cat './examples/Makefile.x11.in' >>$MKNAME

# ---------------------------------------------------------------------
# Create Makefile for test programs
# ---------------------------------------------------------------------

MKNAME='./tests/Makefile.x11'

echo "Creating $MKNAME" 1>&6

echo "$self: Creating $MKNAME" >&5

cat > "$MKNAME" <<EOF
##########################################################################
# Makefile for GLFW test programs on X11 (generated by compile.sh)
##########################################################################
CC     = $CC
CFLAGS = $GLFW_BIN_CFLAGS $GLFW_CFLAGS

LIB      = ../lib/x11/libglfw.a
SOLIB    = ../lib/x11/libglfw.so
LFLAGS   = \$(LIB) $GLFW_LIB_LFLAGS $GLFW_BIN_LFLAGS $GLFW_LFLAGS
SO_LFLAGS = \$(SOLIB) $GLFW_BIN_LFLAGS $GLFW_LFLAGS

EOF
cat './tests/Makefile.x11.in' >>$MKNAME

# ---------------------------------------------------------------------
# Create pkg-config template file (which is used to create libglfw.pc)
# ---------------------------------------------------------------------

MKNAME="./lib/x11/libglfw.pc.in"

echo "Creating $MKNAME" 1>&6

echo "$self: Creating $MKNAME" >&5

cat > "$MKNAME" <<EOF
prefix=@PREFIX@
exec_prefix=@PREFIX@
libdir=@PREFIX@/lib
includedir=@PREFIX@/include

Name: GLFW
Description: A portable framework for OpenGL development
Version: 2.7
URL: http://www.glfw.org/
Libs: -L\${libdir} -lglfw $LFLAGS_THREAD
Cflags: -I\${includedir} $CFLAGS_THREAD 
EOF

