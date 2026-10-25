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

package com.dynamo.bob.pipeline;

import java.util.function.Consumer;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.gamesys.proto.Gui.SceneDesc;

/** Source-format migration context for a GUI custom node plugin. */
public final class GuiSceneMigrationContext {
    private final IResource resource;
    private final SceneDesc.Builder sceneBuilder;
    private final Consumer<String> addCacheKey;

    GuiSceneMigrationContext(IResource resource, SceneDesc.Builder sceneBuilder, Consumer<String> addCacheKey) {
        this.resource = resource;
        this.sceneBuilder = sceneBuilder;
        this.addCacheKey = addCacheKey;
    }

    /** The source GUI, which may be a template used by the GUI being built. */
    public IResource getResource() {
        return resource;
    }

    /**
     * The parsed source, before defaults, inheritance, dependency discovery or
     * resource path replacement. Modify only the formats owned by this plugin.
     * Preserve sparse overrides; do not fill in absent property defaults here.
     */
    public SceneDesc.Builder getSceneBuilder() {
        return sceneBuilder;
    }

    /** Include a migration version so changes to its rules invalidate compiled GUIs. */
    public void addExtraCacheKey(String key) {
        addCacheKey.accept(key);
    }

    public CompileExceptionError error(String message) {
        return new CompileExceptionError(resource, 0, message);
    }
}
