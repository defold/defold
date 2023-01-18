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

Bob can be easily debugged with IntelliJ IDE. You will need to build bob.jar and download IntelliJ Community version first.

1. Start IDE and from the initial welcome screen choose 'Open' project. Navigate to Bob's source directory `com.dynamo.cr/com.dynamo.cr.bob` inside Defold's source tree.

2. In case there is popup that detected maven/eclipse configuration and asks about it, just choose maven. It should make no difference.

You should now see Bob source tree code on the left. You will now have to create a _Run configuration_.

### Add Run Configuration

A _Run configuration_ has to be specified that will execute bob.jar. Set it up as if it would be executed on the command line.

1. From the "Configurations" dropdown on the top bar (next to the hammer) click "Edit configuration" to open the "Run/Debug Configuraitons" dialog.

2. Click "Add new/JAR Application" and set a proper name for it.

3. In the "Configuration" tab use the following settings:

	* `Path to JAR` - Absolute path to bob.jar. For example, /home/.../defold/com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar
	* `Program arguments` - Command line arguments given when executing bob. For example, `--platform x86_64-linux distclean build --archive bundle --variant debug`. It really depends on what parts of Bob's functionallity you want to debug.
	* `Working directory` - Path of a Defold project root, since the command line arguments tell bob to build a project.
	* `JRE` - Pick the JVM used by Defold.
	
Leave the rest of the fields empty and click "Apply".
	
### Run/Debug Bob
	
Hit _run/play_ or _debug_ button to execute the _Run configuration_ created. Check the console at the bottom for typical bob output. Next, browse bob's source code to set breakpoints, pause execution, check watches etc.


	

