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
  symbolic, e.g. HEAD, 1.0, etc. Moreover, for tags, the underlying SHA1 to the actual commit in question is used
  and not the SHA1 for the tag-object.

Setup IDEA
---------
First of all, follow the engine setup instructions and build the engine at least once, as well as Bob.

1. Download and install IntelliJ IDEA Community Edition from here: https://www.jetbrains.com/idea/download .

2. Start the IDE and on the initial welcome screen choose _Open_ project. Navigate to Bob's source directory `com.dynamo.cr`.

3. In case there is a popup that detected maven/eclipse configuration and asks about it, just choose _maven_. It should make no difference. Open `com.dynamo.cr.bob/build.gradle` and run `install` task.

4. Open `File -> Project Structure` and in `Project Settings -> Project` add JDK 25

5. Make sure Compiler output specified ![Compiler output](https://github.com/user-attachments/assets/83cd08c7-6206-4461-b1f9-9121d4c1a4e4)

6. In `Project Settings -> Modules -> Sources` select `com.dynamo.cr.bob/generated` and `com.dynamo.cr.bob/src` folders and click `Mark as: Sources`  ![Mark as: Sources](https://github.com/defold/defold/assets/2209596/fbc660ce-25a8-4612-a5a0-47d3615d4d98)

7. Select `com.dynamo.cr.bob.test/src` folder and click `Mark as: Tests` ![Mark as: Tests](https://github.com/defold/defold/assets/2209596/88d4bde5-5d37-4f6b-8781-ffcc57519f2e)

8. Add `com.dynamo.cr.bob` as Resource Folder and `com.dynamo.cr.bob.test/test` ![resources](https://github.com/defold/defold/assets/2209596/49cec23b-f15c-411b-bdfc-495f78f44936)

9. Select `com.dynamo.cr.bob.test/build` folder and click `Mark as: Excluded` ![Excluded](https://github.com/defold/defold/assets/2209596/a3c9bf3a-0989-46c1-a47d-448205d0b4fc)

10. Change language level to `21 - Record patterns, pattern matching for switch` ![Change language level](https://github.com/defold/defold/assets/2209596/39e61b02-3867-4ca8-9d74-a960561aadfe)

11. In  `Project Settings -> Modules -> Dependencies` click `+` then  `1 JARs or Directories...` and choose `com.dynamo.cr/com.dynamo.cr.bob/lib` and `com.dynamo.cr/com.dynamo.cr.bob.test/lib` ![Dependencies](https://github.com/defold/defold/assets/2209596/dd86e706-b91f-475b-b43d-aaac596ffa1f)

If you got `Unknown command-line option '-b'` error, then:
<img width="2480" height="1482" alt="image" src="https://github.com/user-attachments/assets/48dfe9a6-0b20-4594-a87c-63ac9d4d63ca" />


Testing
---------
To be able to run single tests, change default template for JUnit tests:
1. Open `Edit Configurations`
2. Click `Edit Configuration Template` ![Edit configuration template](https://github.com/defold/defold/assets/2209596/1d052a0c-3e25-4dec-86f6-3d2ba42207e1)
3. Pick JUnit and specify working derictory as `$MODULE_WORKING_DIR$/com.dynamo.cr.bob.test` ![working derictory](https://github.com/defold/defold/assets/2209596/6fb73a5f-dadb-41b0-8343-c7443d177f03)

Debugging
---------

Once bob.jar has been built, it can be debugged with IntelliJ IDE.

### Run Configuration

A _Run configuration_ has to be specified that will execute bob.jar. Set it up as if executing it on the command line.

1. From the _Configurations_ dropdown on the top bar (next to the hammer) click _Edit configuration_ to open the _Run/Debug Configurations_ dialog.

2. Click _Add new/JAR Application_ and set a proper name for it.

3. In the _Configuration_ tab use the following settings:

	* `Path to JAR` - Absolute path to bob.jar. For example, /home/.../defold/com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar
	* `Program arguments` - Command line arguments given when executing bob. For example, `--platform x86_64-linux distclean build --archive bundle --variant debug`. It really depends on what parts of Bob's functionality you want to debug.
	* `Working directory` - Path of a Defold project root, since the command line arguments tells bob to build a project.
	* `JRE` - JVM used by Defold.
	
Leave the rest of the fields empty and click "Apply".

![Run Configuration](https://github.com/defold/defold/assets/2209596/027891c4-7f16-4eb1-8382-81b98ca4525c)

	
### Run/Debug Bob
	
Hit _run/play_ or _debug_ button to execute the _Run configuration_ created. Check the console at the bottom for typical bob output. Next, browse bob's source code to set breakpoints, pause execution, check watches etc.
