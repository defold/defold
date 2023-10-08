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

package com.dynamo.bob.archive.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.util.ArrayList;
import java.util.List;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;
import org.apache.commons.io.FileUtils;

public class ResourceGraphTest {


    private ResourceGraph resourceGraph;
    private ResourceNode main_collectionc;
    private ResourceNode main_goc;
    private ResourceNode dynamic_collectionc;
    private ResourceNode level1_collectionproxyc;
    private ResourceNode level1_collectionc;
    private ResourceNode level1_goc;
    private ResourceNode level2_collectionproxyc;
    private ResourceNode level2_collectionc;
    private ResourceNode level2_goc;
    private ResourceNode level1_scriptc;
    private ResourceNode level2_soundc;
    private ResourceNode main_scriptc;
    private ResourceNode main_shared_goc;
    private ResourceNode main_dynamic_goc;


    private void createResourceGraph() {
        resourceGraph = new ResourceGraph(new Project(new DefaultFileSystem()));
        main_collectionc = new ResourceNode("/main/main.collectionc");
        main_goc = new ResourceNode("/main/main.goc");
        dynamic_collectionc = new ResourceNode("/main/dynamic.collectionc");
        level1_collectionproxyc = new ResourceNode("/main/level1.collectionproxyc", true);
        level1_collectionc = new ResourceNode("/main/level1.collectionc");
        level1_goc = new ResourceNode("/main/level1.goc");
        level2_collectionproxyc = new ResourceNode("/main/level2.collectionproxyc");
        level2_collectionc = new ResourceNode("/main/level2.collectionc");
        level2_goc = new ResourceNode("/main/level2.goc");
        level1_scriptc = new ResourceNode("/main/level1.scriptc");
        level2_soundc = new ResourceNode("/main/level2.soundc");
        main_scriptc = new ResourceNode("/main/main.scriptc");
        main_shared_goc = new ResourceNode("/main/shared_go.goc");
        main_dynamic_goc = new ResourceNode("/main/dynamic.goc");

        resourceGraph.getRootNode().addChild(main_collectionc);
        main_collectionc.addChild(main_goc);
            main_goc.addChild(main_scriptc);
            main_goc.addChild(level1_collectionproxyc);
                level1_collectionproxyc.addChild(level1_collectionc);
                    level1_collectionc.addChild(main_dynamic_goc);
                    level1_collectionc.addChild(level1_goc);
                        level1_goc.addChild(level1_scriptc);
                        level1_goc.addChild(main_shared_goc);
                        level1_goc.addChild(level2_collectionproxyc);
                            level2_collectionproxyc.addChild(level2_collectionc);
                                level2_collectionc.addChild(main_dynamic_goc);
                                level2_collectionc.addChild(level2_goc);
                                    level2_goc.addChild(level2_soundc);

        main_collectionc.addChild(main_shared_goc);

        main_collectionc.addChild(dynamic_collectionc);
            dynamic_collectionc.addChild(main_dynamic_goc);
    }


    @Before
    public void setUp() throws Exception {
        createResourceGraph();
    }

    @After
    public void tearDown() throws IOException {

    }


    @Test
    public void testUsageCounts() throws IOException {
        List<String> excludedResources = resourceGraph.createExcludedResourcesList();
        assertEquals(2, excludedResources.size());
    }
}
