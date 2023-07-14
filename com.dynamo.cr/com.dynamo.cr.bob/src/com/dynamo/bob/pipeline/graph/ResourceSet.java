// Copyright 2020-2023 The Defold Foundation
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

package com.dynamo.bob.pipeline.graph;

import java.util.HashSet;
import java.util.Set;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceWalker;
import com.dynamo.bob.pipeline.graph.ResourceWalker.IResourceVisitor;


public class ResourceSet {

    /**
     * Get a set of resources, starting at a specific root resource.
     * @param project The project the resources belong to.
     * @param rootResource The root resource to create set from.
     * @return resources The set of unique resources.
     */
    public static Set<String> get(Project project, IResource rootResource) throws CompileExceptionError {
        final Set<String> resources = new HashSet<>();
        ResourceWalker.walk(project, rootResource, new IResourceVisitor() {
            @Override
            public boolean shouldVisit(IResource resource) {
                return !resources.contains(resource.output().getAbsPath());
            }

            @Override
            public void visit(IResource resource) throws CompileExceptionError {
                resources.add(resource.output().getAbsPath());
            }

            @Override
            public void leave(IResource resource) throws CompileExceptionError {}
        });
        return resources;
    }
}