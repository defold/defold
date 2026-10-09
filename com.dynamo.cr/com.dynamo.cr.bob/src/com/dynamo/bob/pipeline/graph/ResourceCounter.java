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

import java.util.ArrayDeque;
import java.util.Deque;
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
    private static class ResourceContext {
        final Set<IResource> visitedChildren = new HashSet<>();
        boolean excludeReferences;
    }

    // Resources on the path from the root to the resource currently being visited.
    // This prevents dependency cycles without globally deduplicating occurrences of
    // the same resource below different parents.
    private final Set<IResource> resourcesInCurrentBranch = new HashSet<>();

    // Per-resource traversal state. Each context corresponds to one resource
    // occurrence in the current branch and tracks the children of that occurrence.
    private final Deque<ResourceContext> resourceContexts = new ArrayDeque<>();
    private int resourceCount = 0;

    @Override
    public boolean shouldVisit(IResource resource, IResource parentResource) {
        if (parentResource != null) {
            ResourceContext parentContext = resourceContexts.peek();

            // Dynamic factories and collection proxies do not preload their
            // referenced resources. Also count repeated children of one parent only
            // once, matching the runtime preloader's sibling deduplication.
            if (parentContext.excludeReferences || !parentContext.visitedChildren.add(resource)) {
                return false;
            }
        }

        // The same resource can be counted below another parent, but not if it is
        // already an ancestor of this occurrence, since that would form a cycle.
        return resourcesInCurrentBranch.add(resource);
    }

    @Override
    public void visit(IResource resource, IResource parentResource) {
        ++resourceCount;
        resourceContexts.push(new ResourceContext());
    }

    @Override
    public void visitMessage(Message message, IResource resource, IResource parentResource) {
        if (message instanceof CollectionProxyDesc
                || message instanceof FactoryDesc && ((FactoryDesc) message).getLoadDynamically()
                || message instanceof CollectionFactoryDesc && ((CollectionFactoryDesc) message).getLoadDynamically()) {
            resourceContexts.peek().excludeReferences = true;
        }
    }

    @Override
    public void leave(IResource resource, IResource parentResource) {
        resourceContexts.pop();
        resourcesInCurrentBranch.remove(resource);
    }

    public int getReferencedResourceCount() {
        return Math.max(0, resourceCount - 1);
    }

    /**
     * Count the resource request nodes referenced by an already parsed resource
     * message. Repeated children of the same parent are counted once, matching the
     * runtime preloader, while occurrences below different parents are counted
     * separately. The root resource represented by the message is not included in
     * the returned count.
     */
    public static int countResources(Project project, IResource rootResource, Message message) throws CompileExceptionError {
        ResourceCounter resourceCounter = new ResourceCounter();
        ResourceWalker.walk(project, rootResource, message, resourceCounter);
        return resourceCounter.getReferencedResourceCount();
    }
}
