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


public class ResourceGraph {

    // helper class to keep track of current state while traversing
    // the graph
    private static class GraphTraversalState {
        public Set<String> visitedNodes;
        public ResourceNode node;

        public GraphTraversalState(ResourceNode node) {
            this.visitedNodes = new HashSet<String>();
            this.node = node;
        }
        public GraphTraversalState(ResourceNode node, Set<String> visitedNodes) {
            this.visitedNodes = visitedNodes;
            this.node = node;
        }

        public String toString() {
            return "[GraphTraversalState node: " + node + ", visited nodes: " + visitedNodes + "]";
        }
    }

    public enum GraphType {
        // Graph with resources added only once per collection proxy
        LIVE_UPDATE,
        // Full graph with all resources
        FULL
    }


    private Project project;
    private GraphType type;
    private Set<IResource> resources = new HashSet<>();
    private ResourceNode root = new ResourceNode("<AnonymousRoot>", "<AnonymousRoot>");


    public ResourceGraph(GraphType type, Project project) {
        this.type = type;
        this.project = project;
    }

    /**
     * Add a resource to the graph. This will add the resource and all sub-resources
     * to the graph, optionally filtering which resources to add based on the
     * graph type.
     * @param rootResource The resource to create graph from.
     */
    public void add(IResource rootResource) throws CompileExceptionError {
        final Stack<GraphTraversalState> stack = new Stack<>();
        
        ResourceWalker.walk(project, rootResource, new IResourceVisitor() {
            @Override
            public boolean shouldVisit(IResource resource) {
                if (type == GraphType.FULL) {
                    return true;
                }
                if (stack.empty()) {
                    return true;
                }
                GraphTraversalState state = stack.peek();
                return !state.visitedNodes.contains(resource.output().getAbsPath());
            }

            @Override
            public void visit(IResource resource) throws CompileExceptionError {
                resources.add(resource.output());

                ResourceNode currentNode = new ResourceNode(resource);
                if (stack.empty()) {
                    GraphTraversalState state = new GraphTraversalState(currentNode, new HashSet<String>());
                    state.visitedNodes.add(resource.output().getAbsPath());
                    // push the first stack item twice so that we have one left
                    // when all resources have been visited (we pop in leave())
                    stack.push(state);
                    stack.push(state);
                }
                else {
                    GraphTraversalState state = stack.peek();
                    ResourceNode parentNode = state.node;
                    parentNode.addChild(currentNode);
                    if (resource.output().getPath().endsWith(".collectionproxyc")) {
                        state = new GraphTraversalState(currentNode, new HashSet<String>());
                    }
                    else {
                        state = new GraphTraversalState(currentNode, state.visitedNodes);
                    }
                    state.visitedNodes.add(resource.output().getAbsPath());
                    stack.push(state);
                }
            }

            @Override
            public void leave(IResource resource) throws CompileExceptionError {
                stack.pop();
            }
        });
        GraphTraversalState state = stack.pop();
        root.addChild(state.node);
    }

    /**
     * Get the root resource node of the graph. All resources added to the graph
     * will exist as children of the root resource.
     * @return The root node
     */
    public ResourceNode getRootNode() {
        return root;
    }

    /**
     * Get a set of all resources added to the graph.
     * @return A set of resources
     */
    public Set<IResource> getResources() {
        return resources;
    }
}