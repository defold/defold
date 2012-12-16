Bob
===

Defold Bob the Builder build system :-)

Bob can be used from command-line using plain Java without osgi-stuff but the
project itself is an osgi-bundle to be used in a eclipse-project.
It's important to never add eclipse or osgi-dependencies to core functionality.
When required an interface must be created, see IClassScanner and concrete
implementations.

Paths
-----

In general paths in bob are separated with forward slashes and paths do not have an end separator,
i.e. /foo/bar instead of /foo/bar/
