# Copyright 2020-2025 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# USAGE:
# Make sure you have $DYNAMO_HOME setup and built bob-light.jar, then run:
# ./create_prebuilts.sh
#
# It will build the project and copy+rename the Spine files needed by the tests.

# Move to mesh project directory
cd mesh_prebuilt_project/

# Build project using bob-light
java -jar $DYNAMO_HOME/share/java/bob-light.jar build

# Copy and rename prebuilt files to test folder
cp build/default/mesh/no_data.bufferc ../mesh/no_data.prebuilt_bufferc
cp build/default/mesh/no_data.meshc ../mesh/no_data.prebuilt_meshc
cp build/default/mesh/triangle.bufferc ../mesh/triangle.prebuilt_bufferc
cp build/default/mesh/triangle.meshc ../mesh/triangle.prebuilt_meshc
