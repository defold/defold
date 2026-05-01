# Copyright 2020-2026 The Defold Foundation
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


BASIS_DIR=$1

if [ -z $BASIS_DIR ]; then
    echo "You must pass in the directory where to find the basis_universal repo"
    exit 1
fi

# from the script dir

cp -v -r $BASIS_DIR/encoder/ ./encoder
cp -v -r $BASIS_DIR/transcoder/ ./transcoder

git apply -v defold.patch