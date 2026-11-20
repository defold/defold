# Bob in IntelliJ IDEA

Follow the [Bob build guide](README.md) to build Bob and the required engine artifacts before setting up IntelliJ IDEA.

1. Open the checked-in `com.dynamo.cr` project in [IntelliJ IDEA](https://www.jetbrains.com/idea/download).
2. In **File → Project Structure → Project**, select JDK 25 and its language level. The [project settings](.idea/misc.xml) specify both.
3. In the Gradle tool window, link [Bob](com.dynamo.cr.bob/build.gradle) and [Bob’s tests](com.dynamo.cr.bob.test/build.gradle) if needed. Run Bob’s `install` task to prepare the JAR. The [IntelliJ module file](.idea/com.dynamo.cr.iml) defines the source and resource roots.

## Testing

To be able to run single tests, change default template for JUnit tests:

1. Open `Edit Configurations`
2. Click `Edit Configuration Template` ![Edit configuration template](https://github.com/defold/defold/assets/2209596/1d052a0c-3e25-4dec-86f6-3d2ba42207e1)
3. Pick JUnit and specify working directory as `$MODULE_WORKING_DIR$/com.dynamo.cr.bob.test` ![working directory](https://github.com/defold/defold/assets/2209596/6fb73a5f-dadb-41b0-8343-c7443d177f03)

## Debugging

Once `bob.jar` has been built, it can be debugged with IntelliJ IDEA.

### Run Configuration

A _Run configuration_ has to be specified that will execute bob.jar. Set it up as if executing it on the command line.

1. From the _Configurations_ dropdown on the top bar (next to the hammer) click _Edit configuration_ to open the _Run/Debug Configurations_ dialog.

2. Click _Add new/JAR Application_ and set a proper name for it.

3. In the _Configuration_ tab use the following settings:

- `Path to JAR` - Absolute path to `bob.jar`. For example, `/home/.../defold/com.dynamo.cr/com.dynamo.cr.bob/dist/bob.jar`.
- `Program arguments` - Command-line arguments given when executing Bob. For example, `--platform x86_64-linux distclean build --archive bundle --variant debug`. Choose the arguments for the functionality you want to debug.
- `Working directory` - Path of a Defold project root, since Bob builds a project from its working directory.
- `JRE` - JVM used by Defold.

Leave the rest of the fields empty and click "Apply".

![Run Configuration](https://github.com/defold/defold/assets/2209596/027891c4-7f16-4eb1-8382-81b98ca4525c)

### Run/Debug Bob

Hit _run/play_ or _debug_ button to execute the _Run configuration_ created. Check the console at the bottom for typical bob output. Next, browse bob's source code to set breakpoints, pause execution, check watches etc.
