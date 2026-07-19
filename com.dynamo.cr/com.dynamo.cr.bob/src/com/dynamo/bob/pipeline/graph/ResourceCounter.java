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

package com.dynamo.bob.pipeline.graph;

import java.util.HashSet;
import java.util.Set;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceWalker.IResourceVisitor;
import com.dynamo.gamesys.proto.CollectionProxy.CollectionProxyDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;

import com.google.protobuf.Message;

public class ResourceCounter implements IResourceVisitor {
    private final Set<IResource> visitedResources = new HashSet<>();
    private final Set<IResource> resourcesWithExcludedReferences = new HashSet<>();
    private int resourceCount = 0;

    @Override
    public boolean shouldVisit(IResource resource, IResource parentResource) {
        if (resourcesWithExcludedReferences.contains(parentResource)) {
            return false;
        }
        return visitedResources.add(resource);
    }

    @Override
    public void visit(IResource resource, IResource parentResource) {
        ++resourceCount;
    }

    @Override
    public void visitMessage(Message message, IResource resource, IResource parentResource) {
        if (message instanceof CollectionProxyDesc
                || message instanceof FactoryDesc && ((FactoryDesc) message).getLoadDynamically()
                || message instanceof CollectionFactoryDesc && ((CollectionFactoryDesc) message).getLoadDynamically()) {
            resourcesWithExcludedReferences.add(resource);
        }
    }

    @Override
    public void leave(IResource resource, IResource parentResource) {
    }

    public int getReferencedResourceCount() {
        return Math.max(0, resourceCount - 1);
    }

    /**
     * Count the unique resources referenced by an already parsed resource message.
     * The root resource represented by the message is not included in the returned
     * count.
     */
    public static int countResources(Project project, IResource rootResource, Message message) throws CompileExceptionError {
        ResourceCounter resourceCounter = new ResourceCounter();
        ResourceWalker.walk(project, rootResource, message, resourceCounter);
        return resourceCounter.getReferencedResourceCount();
    }
}
