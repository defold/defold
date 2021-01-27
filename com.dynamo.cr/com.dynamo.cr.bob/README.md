Bob
===
Defold Bob the Builder build system.

Paths
-----

In general paths in bob are separated with forward slashes and paths do not have an end separator,
i.e. /foo/bar instead of /foo/bar/

Library Cache
------------

Caching of libraries is based on the Git SHA1, to the actual commit

* The SHA1 is stored in the zip file comment
* The SHA1 is used as ETag and If-None-Match/"304 Not modified" for cache validation
* Note that the SHA1, in general, is not identical to the requested version as the requested version can be
  symbolic, e..g HEAD, 1.0, etc. Moreover, for tags, the underlying SHA1 to the actual commit in question is used
  and not the SHA1 for the tag-object.
