// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob;

public enum PlatformArchitectures {
    //
    MacOS(new String[] {"x86_64-macos"}, new String[] {"x86_64-macos"}),
    Windows32(new String[] {"x86-win32"}, new String[] {"x86-win32"}),
    Windows64(new String[] {"x86_64-win32"}, new String[] {"x86_64-win32"}),
    Linux(new String[] {"x86_64-linux"}, new String[] {"x86_64-linux"}),
    iOS(new String[] {"arm64-ios", "x86_64-ios"}, new String[] {"arm64-ios"}),
    Android(new String[] {"arm64-android", "armv7-android"}, new String[] {"armv7-android","arm64-android"}),
    Web(new String[] {"js-web", "wasm-web"}, new String[] {"js-web", "wasm-web"}),
    NX64(new String[] {"arm64-nx64"}, new String[] {"arm64-nx64"});

    String[] architectures;
    String[] defaultArchitectures;
    PlatformArchitectures(String[] architectures, String[] defaultArchitectures) {
        this.architectures = architectures;
        this.defaultArchitectures = defaultArchitectures;
    }

    String[] getArchitectures() {
        return architectures;
    }

    String[] getDefaultArchitectures() {
        return defaultArchitectures;
    }
}
