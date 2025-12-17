#!/bin/sh

[ -n "$srcdir" ] || srcdir=`dirname "$0"`
[ -n "$srcdir" ] || srcdir=.

ORIGDIR=`pwd`
cd $srcdir

./bootstrap || exit $?

cd $ORIGDIR
if [ -z "$NOCONFIGURE" ]; then
  echo "Running $srcdir/configure $@"
  "$srcdir/configure" "$@" || exit $?
  echo "Now it is ready to 'make'"
fi
