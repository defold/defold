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

package com.dynamo.bob.pipeline.graph.test;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.util.List;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.pipeline.graph.ResourceGraph;

public class ResourceGraphTest {


    private ResourceGraph resourceGraph;


    private ResourceGraph createResourceGraph() {
        Project project = new Project(new DefaultFileSystem());
        ResourceGraph graph = new ResourceGraph(project);

        /*
        root
        |
        +--/main/main.collectionc
           +--/main/main.goc
           |  +--/main/main.scriptc
           |
           +--/main/shared_go.goc
           |
           +--/main/level1.collectionproxyc
           |  +--/main/level1.collectionc
           |     +--/main/level1.goc
           |        +--/main/level1.scriptc
           |        +--/main/shared_go.goc
           |
           +--/main/level2.collectionproxyc
              +--/main/level2.collectionc
                 +--/main/level2.goc
                    +--/main/level2.soundc
        */

        ResourceNode root = graph.getRootNode();
        ResourceNode main_collectionc = graph.add("/main/main.collectionc", root);
        ResourceNode main_goc = graph.add("/main/main.goc", main_collectionc);
        ResourceNode main_scriptc = graph.add("/main/main.scriptc", main_goc);
        ResourceNode shared_goc = graph.add("/main/shared_go.goc", main_collectionc);

        ResourceNode level1_collectionproxyc = graph.add("/main/level1.collectionproxyc", main_goc);
        level1_collectionproxyc.setType(ResourceNode.Type.ExcludedCollectionProxy);
        ResourceNode level1_collectionc = graph.add("/main/level1.collectionc", level1_collectionproxyc);
        ResourceNode level1_goc = graph.add("/main/level1.goc", level1_collectionc);
        ResourceNode level1_scriptc = graph.add("/main/level1.scriptc", level1_goc);

        ResourceNode level2_collectionproxyc = graph.add("/main/level2.collectionproxyc", main_goc);
        ResourceNode level2_collectionc = graph.add("/main/level2.collectionc", level2_collectionproxyc);
        ResourceNode level2_goc = graph.add("/main/level2.goc", level2_collectionc);
        ResourceNode level2_scriptc = graph.add("/main/level2.scriptc", level2_goc);

        graph.add(shared_goc, level1_collectionc);
        graph.add(shared_goc, level2_collectionc);
        return graph;
    }


    @Before
    public void setUp() throws Exception {
        resourceGraph = createResourceGraph();
    }

    @After
    public void tearDown() throws IOException {

    }


    @Test
    public void testUsageCounts() throws IOException {
        List<String> excludedResources = resourceGraph.createExcludedResourcesList();
        System.out.println("excludedResources " + excludedResources);
        assertEquals(3, excludedResources.size());
    }
}
