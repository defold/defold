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

import java.io.IOException;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.gamesys.proto.Gui.NodeDesc;

/**
 * A GUI custom node plugin. Property registration remains static.
 * Plugins implementing lifecycle callbacks must have a public no-argument
 * constructor. Bob creates one instance per custom type and GUI build task,
 * shared by source migration and that task's nodes and layouts, but never by
 * concurrent GUI tasks.
 * All create/build callbacks in a GUI task receive the same GuiBuildContext.
 * Nodes have defaults and overrides resolved. Template child IDs include their
 * instance prefix, and an empty layout name identifies the default layout.
 */
public interface IGuiCustomNode {
    /**
     * Converts plugin-owned source formats to ordinary custom nodes and generic
     * resources. Called once per parsed GUI source in this task, including
     * templates, before defaults, overrides and dependencies are resolved.
     * All registered migrations run, even if no node of their type exists yet.
     * Do not read other resources or validate selections here; use create/build.
     */
    default void migrateScene(GuiSceneMigrationContext context) throws CompileExceptionError {
    }

    /**
     * Declares inputs, subtasks and extra cache keys before Bob calculates the
     * GUI task signature. Called for each resolved node, including template
     * children and layout overrides, even when the build will be cached.
     * Compiled dependency contents are not available yet.
     */
    default void create(GuiBuildContext context, Task.TaskBuilder<?> taskBuilder, NodeDesc node, String layoutName) throws IOException, CompileExceptionError {
    }

    /**
     * Validates or transforms a resolved node before serialization. Called after
     * dependencies have built, only when the GUI task is not skipped or restored
     * from cache. Resources read here must be declared as inputs in create().
     * Modify the supplied node builder, keeping its ID and custom type unchanged.
     * Added custom properties may use string IDs; Bob converts them to hashes.
     */
    default void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) throws IOException, CompileExceptionError {
    }
}
