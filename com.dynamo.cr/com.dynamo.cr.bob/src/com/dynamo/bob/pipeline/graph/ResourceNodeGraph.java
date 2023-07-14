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

import java.util.Stack;
import java.util.Set;
import java.util.HashSet;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceWalker;
import com.dynamo.bob.pipeline.graph.ResourceWalker.IResourceVisitor;


public class ResourceNodeGraph {

    // helper class to keep track of current state while traversing
    // the graph
    private static class GraphState {
        public Set<String> visitedNodes;
        public ResourceNode node;

        public GraphState(ResourceNode node) {
            this.visitedNodes = new HashSet<String>();
            this.node = node;
        }
        public GraphState(ResourceNode node, Set<String> visitedNodes) {
            this.visitedNodes = visitedNodes;
            this.node = node;
        }
    }

    /**
     * Get a resource node graph of all project resources. Each resource in the
     * graph will only exist once per encountered collection proxy, even if it
     * might be used multiple times.
     * @param project The project the resources belong to.
     * @param rootResource The root resource to create graph from.
     * @return rootNode The root node
     */
    public static ResourceNode get(Project project, IResource rootResource) throws CompileExceptionError {
        final Stack<GraphState> stack = new Stack<>();
        
        ResourceWalker.walk(project, rootResource, new IResourceVisitor() {
            @Override
            public boolean shouldVisit(IResource resource) {
                if (stack.empty()) {
                    return true;
                }
                GraphState state = stack.peek();
                return !state.visitedNodes.contains(resource.output().getAbsPath());
            }

            @Override
            public void visit(IResource resource) throws CompileExceptionError {
                ResourceNode currentNode = new ResourceNode(resource);
                if (stack.empty()) {
                    GraphState state = new GraphState(currentNode, new HashSet<String>());
                    state.visitedNodes.add(resource.output().getAbsPath());
                    // push the first stack item twice so that we have one left
                    // when all resources have been visited (we pop in leave())
                    stack.push(state);
                    stack.push(state);
                }
                else {
                    GraphState state = stack.peek();
                    ResourceNode parentNode = state.node;
                    parentNode.addChild(currentNode);
                    if (resource.output().getPath().endsWith(".collectionproxyc")) {
                        state = new GraphState(currentNode, new HashSet<String>());
                    }
                    else {
                        state = new GraphState(currentNode, state.visitedNodes);
                    }
                    state.visitedNodes.add(resource.output().getAbsPath());
                    stack.push(state);
                }
            }

            @Override
            public void leave(IResource resource) throws CompileExceptionError {
                graphStateStack.pop();
            }
        });
        GraphState state = graphStateStack.pop();
        return state.node;
    }

}