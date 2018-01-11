# waf_gamesys does not currently generate valid Spine/rigscene contents, so for some tests
# that need valid content for such files we resort to prebuilt content.
# This script will run bob-light.jar on a simple project located in spine_prebuilt_project/
# It will generate rigscene and other Spine related content files that is used by some tests.
#
# The resulting prebuilt files are checked into the repo until we can hook this script into
# the build pipeline.
#
# USAGE:
# Make sure you have $DYNAMO_HOME setup and built bob-light.jar, then run:
# ./create_prebuilts.sh
#
# It will build the project and copy+rename the Spine files needed by the tests.

# Move to project directory
cd spine_prebuilt_project/

# Build project using bob-light
java -jar $DYNAMO_HOME/share/java/bob-light.jar build

# Copy and rename prebuilt files to test folder
cp build/default/spine/prebuilt.animationsetc ../spine/prebuilt.prebuilt_animationsetc
cp build/default/spine/prebuilt.meshsetc ../spine/prebuilt.prebuilt_meshsetc
cp build/default/spine/prebuilt.rigscenec ../spine/prebuilt.prebuilt_rigscenec
cp build/default/spine/prebuilt.skeletonc ../spine/prebuilt.prebuilt_skeletonc
cp build/default/spine/prebuilt.texturec ../spine/prebuilt.prebuilt_texturec
cp build/default/spine/prebuilt.texturesetc ../spine/prebuilt.prebuilt_texturesetc
