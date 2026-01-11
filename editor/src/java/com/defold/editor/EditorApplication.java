// Copyright 2020-2026 The Defold Foundation
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

package com.defold.editor;

import org.projectodd.shimdandy.ClojureRuntimeShim;

/**
 * Project/Main Window abstraction. One class per project by using custom class loaders
 * @author chmu
 *
 */
public class EditorApplication {
    private ClojureRuntimeShim runtime;

    public EditorApplication(ClassLoader classLoader) {
        runtime = ClojureRuntimeShim.newRuntime(classLoader, "editor");
        if (Editor.isDev()) {
            runtime.invoke("editor.debug/init-debug");
        }
        runtime.invoke("editor.bootloader/load-boot");
    }

    public void waitForClojureNamespaces() {
        runtime.invoke("editor.bootloader/wait-until-editor-boot-loaded");
    }

    public void run(String[] args) {
        runtime.invoke("editor.bootloader/main", args);
    }
}
