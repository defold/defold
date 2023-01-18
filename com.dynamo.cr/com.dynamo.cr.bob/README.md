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

Debugging
---------

Once bob.jar has been built, it can be debugged with IntelliJ IDE (Community IntelliJ IDEA 2022.3.1 was used in this guide).

1. Start the IDE and on the initial welcome screen choose _Open_' project. Navigate to Bob's source directory `com.dynamo.cr/com.dynamo.cr.bob` inside Defold's source tree.

2. In case there is popup that detected maven/eclipse configuration and asks about it, just choose _maven_. It should make no difference.

You should now see Bob's source code tree on the left.

### Run Configuration

A _Run configuration_ has to be specified that will execute bob.jar. Set it up as if executing it on the command line.

1. From the _Configurations_ dropdown on the top bar (next to the hammer) click _Edit configuration_ to open the _Run/Debug Configurations_ dialog.

2. Click _Add new/JAR Application_ and set a proper name for it.

3. In the _Configuration_ tab use the following settings:

	* `Path to JAR` - Absolute path to bob.jar. For example, /home/.../defold/com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar
	* `Program arguments` - Command line arguments given when executing bob. For example, `--platform x86_64-linux distclean build --archive bundle --variant debug`. It really depends on what parts of Bob's functionallity you want to debug.
	* `Working directory` - Path of a Defold project root, since the command line arguments tells bob to build a project.
	* `JRE` - JVM used by Defold.
	
Leave the rest of the fields empty and click "Apply".
	
### Run/Debug Bob
	
Hit _run/play_ or _debug_ button to execute the _Run configuration_ created. Check the console at the bottom for typical bob output. Next, browse bob's source code to set breakpoints, pause execution, check watches etc.


	

