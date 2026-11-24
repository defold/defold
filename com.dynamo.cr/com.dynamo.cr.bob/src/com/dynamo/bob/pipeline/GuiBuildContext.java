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

import java.util.Map;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.Gui.NodeDescOrBuilder;
import com.dynamo.gamesys.proto.Gui.Property;

/** Context shared by all custom node callbacks in one GUI compilation task. */
public final class GuiBuildContext {
    private final Project project;
    private final IResource resource;
    private final Map<String, IResource> resources;

    GuiBuildContext(Project project, IResource resource, Map<String, IResource> resources) {
        this.project = project;
        this.resource = resource;
        this.resources = resources;
    }

    public Project getProject() {
        return project;
    }

    /** The source GUI being compiled. */
    public IResource getResource() {
        return resource;
    }

    /** Looks up a resolved custom property by its registered name, or returns null. */
    public static Property getProperty(NodeDescOrBuilder node, String name) {
        long hash = MurmurHash.hash64(name);
        for (Property property : node.getCustomPropertiesList()) {
            if (property.hasIdHash() && property.getIdHash() == hash) {
                return property;
            }
        }
        return null;
    }

    /**
     * Named generic GUI resources, including those inherited from templates.
     * Paths refer to source files, before extension replacement or minification.
     * The map is read-only. Looking up a resource does not declare a task input.
     */
    public Map<String, IResource> getResources() {
        return resources;
    }

    /** Formats the node and optional layout prefix used in GUI diagnostics. */
    static String formatNodeLocation(NodeDescOrBuilder node, String layoutName) {
        String location = "Node '" + node.getId() + "'";
        if (!layoutName.isEmpty()) {
            location += ", layout '" + layoutName + "'";
        }
        return location + ": ";
    }

    /** Creates a build error identifying the source GUI, resolved node and layout. */
    public CompileExceptionError error(NodeDescOrBuilder node, String layoutName, String message) {
        return new CompileExceptionError(resource, 0, formatNodeLocation(node, layoutName) + message);
    }
}
