#!/usr/bin/env bash
# Copyright 2020-2024 The Defold Foundation
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



export SHA1=04dca7911ea255f37be799c18d74c305b921c1a6
export SHA1_SHORT=${SHA1:0:7}
export VERSION=2.1.0-${SHA1_SHORT}
export PRODUCT=luajit
export TARGET_FILE=${PRODUCT}-${VERSION}
