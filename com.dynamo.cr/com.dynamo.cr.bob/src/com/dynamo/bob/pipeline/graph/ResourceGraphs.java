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


public class ResourceGraphs {

    private ResourceGraph liveUpdateResourceGraph;
    private ResourceGraph fullResourceGraph;

    public ResourceGraphs(Project project) {
        liveUpdateResourceGraph = new ResourceGraph(ResourceGraph.GraphType.LIVE_UPDATE, project);
        fullResourceGraph =  new ResourceGraph(ResourceGraph.GraphType.FULL, project);
    }

    /**
     * Add a resource to the graphs.
     * @param resource The resource to add to the graphs
     */
    public void add(IResource resource) throws CompileExceptionError {
        liveUpdateResourceGraph.add(resource);
        fullResourceGraph.add(resource);
    }

    /**
     * Get the full resource graph.
     * @return The resource graph
     */
    public ResourceGraph getFullGraph() {
        return fullResourceGraph;
    }

    /**
     * Get a resource graph for use with live update, where each resource is
     * represented only once per collection proxy
     * @return The resource graph
     */
    public ResourceGraph getLiveUpdateGraph() {
        return liveUpdateResourceGraph;
    }

}