# Gradle build script for android-support-v4.jar

This gradle build script will pull down needed packages from
Google Maven repository to package a custom `android-support-v4.jar`.
It will also automatically put the result into a `tar.gz` package
ready to be used in Defols build pipeline.

## Usage
Update the package versions as needed under `dependencies` inside
`build.gradle` then just run `gradle build` from this directory.
It will output the files called `android-support-v4-armv7-android.tar.gz`
and `android-support-v4-arm64-android.tar.gz` that can be moved
to the packages folder inside the defold repo.
