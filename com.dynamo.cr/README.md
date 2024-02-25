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

Setup IDEA
---------

1. Download and install IntelliJ IDEA Community Edition from here: https://www.jetbrains.com/idea/download .

2. Start the IDE and on the initial welcome screen choose _Open_ project. Navigate to Bob's source directory `com.dynamo.cr`.

3. In case there is a popup that detected maven/eclipse configuration and asks about it, just choose _maven_. It should make no difference.

4. Open `File -> Project Structure` and in `Project Settings -> Project` add JDK 17

5. In `Project Settings -> Modules -> Sources` select `com.dynamo.cr.bob/generated`, `com.dynamo.cr.bob/src` and `com.dynamo.cr.common/src` folders and click `Mark as: Sources`  ![Mark as: Sources](https://github.com/defold/defold/assets/2209596/0566d962-bd80-49a5-b246-c10703c0310b)

6. Select `com.dynamo.cr.bob.test/src` and `com.dynamo.cr.common.test/src` folders and click `Mark as: Tests` ![Mark as: Tests](https://github.com/defold/defold/assets/2209596/0e8a29fc-dc5b-41d0-b6a5-ea0f19e4d489)

7. Change language level to `8 - Lambdas, type annotations etc.` ![Change language level](https://github.com/defold/defold/assets/2209596/40d089fd-8317-4b06-8db3-488b9dedafbc)

8. In  `Project Settings -> Modules -> Dependencies` click `+` then  `1 JARs or Directories...` and choose `com.dynamo.cr/com.dynamo.cr.common/ext` and repeat for `com.dynamo.cr/com.dynamo.cr.bob/lib` and `com.dynamo.cr/com.dynamo.cr.bob.test/lib` ![Dependencies](https://github.com/defold/defold/assets/2209596/71638224-2adf-4cd1-8e79-ad5224bcd39b)

9. Right click to `build.xml` and choose very last option `Add as Ant Build File` for each of the following:
	   `com.dynamo.cr/com.dynamo.cr.bob/build.xml`
	   `com.dynamo.cr/com.dynamo.cr.bob.test/build.xml`
	   `com.dynamo.cr/com.dynamo.cr.common/build.xml`
	   ![build.xml](https://github.com/defold/defold/assets/2209596/7bada507-64b3-4d42-b177-21ec90b9271a)

10. In `Ant` window click to `Properties`, choose `Properties` and add Name:`env.DYNAMO_HOME` Value:`../../tmp/dynamo_home`, switch to `Execution` make sure right JDK picked ![Properties](https://github.com/defold/defold/assets/2209596/ba83cc75-95a2-4597-901b-e742f85f121d) ![Properties2](https://github.com/defold/defold/assets/2209596/590f5b1d-17ed-4893-897d-df54b1731798)

11. Repeat it for the rest of `build.xml` but for `com.dynamo.cr/com.dynamo.cr.bob.test/build.xml` also add Name:`DM_BOB_BUNDLERTEST_ONLY_HOST` and Value:`1` ![DM_BOB_BUNDLERTEST_ONLY_HOST](https://github.com/defold/defold/assets/2209596/8bf04931-4eca-40a4-abaa-2a23a8709c92)

12. Now you can Run Ant jobs from IDEA, just pick needed `build.xml` and Run it in Ant window ![Build](https://github.com/defold/defold/assets/2209596/b336c230-b148-4568-ab05-a5dfdb13c62b)

13. Logs with the progress of building can be found in `View -> Tool Windows -> Messages` ![Logs](https://github.com/defold/defold/assets/2209596/4b805b05-a070-4787-b70b-6f6f9da7f215)


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
