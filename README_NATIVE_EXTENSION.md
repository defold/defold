# Native Extensions

## Brief

Native extensions allows you to create plugins to the engine, using the Lua interface, native code and also
functionality Defold provides via the Defold SDK.


## Defold SDK

The defold sdk contains public headers and libraries from the engine.
The internal structure of the sdk's are matching the structure in the **DYNAMO_HOME** folder.

E.g. (don't use these specific ones, since they might grow "old", they're simply here for copy paste reasons)

  http://d.defold.com/archive/bdbf6ddaefb79c22214c86f945ea91d7fa6171de/engine/defoldsdk.zip
  http://d.defold.com/archive/bdbf6ddaefb79c22214c86f945ea91d7fa6171de/engine/x86_64-darwin/defoldsdk.zip

### Build SDK

The sdk is built, packaged and uploaded for each platform during the *build_engine* step in build.py.
And when the *archive_engine* step is run, all platform sdk's are downloaded and merged into one zip file.

You can build each platform individually:

    defold$ ./scripts/build.py build_platform_sdk --platform=x86_64-darwin

### build.yml

An important part of the sdk is the *defold/share/extender/build.yml* which controls the compiler settings for the server.
As we include this file in every sdk, we make it simple to update. You can update it simply by running:

    $ waf install

### During development

Note that during the development of the SDK, you can run the local server, pointing it to your DYNAMO_HOME variable!
This of course requires you to have build the engine beforehand:

    defold$ ./scripts/build.py build_engine --platform=x86_64-darwin

Set up the path for the DYNAMO_HOME variable:

    export DYNAMO_HOME=/Users/mathiaswesterdahl/work/defold/tmp/dynamo_home
  

### Editor 1 against live server

To use Editor 1 against the production extender service, you need to specify `-Ddefold.sha1=<SHA1>` as one of the run arguments for the editor. `<SHA1>` should be set to a commit hash that has been built on CI (since this is where the SDK is built and uploaded).

## Local build server

There are two ways of launching a local server, via gradle or docker.
Both require the same git repository, and both respect the DYNAMO_HOME environment variable.

### Server repository

The local server can be built and started using the **defold/extender** repository.

    $ git clone https://github.com/defold/extender.git

### Gradle

To launch the server locally, via gradle:

    $ cd extender
    extender$ ./gradlew build
    extender$ ./gradlew bootRun

### Local Docker

The easiest way to debug is to use the docker container.
First, install [Docker](https://www.docker.com/)

The gradle server also runs inside a docker container:

    $ cd extender
    extender$ ./scripts/build.sh
    extender$ ./scripts/run.sh
  
#### Debugging inside docker

It's quite easy to get access to your docker container:

    extender$ ./scipts/debug.sh

## Editor 2

The native extensions are only supported via Editor 2.

### Build

The setup steps are described [here](./editor/README.md)

### Run

    editor$ lein run

### Preferences

In the preferences, select "Enable Native Extensions"

### Project layout

The project layout is quite straight forward.
Here's a simple example

    VideoPlayer
      main
        ...
      include
        mylib.h
      lib
        x86_64-osx
          libmylib.a
      src
        myluamodule.cpp

#### Current issues

* Don't put headers in the *src/* folder! They currently get built as .cpp files
  

### Build

Simply build with Command/Ctrl + B
Any build errors will show up in the Editor 2 console.

### Debugging

Currently, the downloaded (built) engine, is created as a temporary file.
This makes it very hard to debug.

