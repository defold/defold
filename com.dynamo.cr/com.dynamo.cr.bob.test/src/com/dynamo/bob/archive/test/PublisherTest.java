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

package com.dynamo.bob.archive.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.nio.charset.StandardCharsets;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.publisher.FolderPublisher;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;

public class PublisherTest {
    private MockFileSystem fileSystem;
    private Project project;

    @Before
    public void setUp() {
        fileSystem = new MockFileSystem();
        project = new Project(fileSystem);
        project.setOption("liveupdate", "true");
    }

    @After
    public void tearDown() {
        project.dispose();
    }

    // Zip and Folder remain selectable after removing the Amazon publisher.
    @Test
    public void testSupportedPublishers() throws Exception {
        Map<String, Class<? extends Publisher>> publishers = Map.of(
                "Zip", ZipPublisher.class,
                "Folder", FolderPublisher.class);
        for (var entry : publishers.entrySet()) {
            fileSystem.addFile("liveupdate.settings", ("[liveupdate]\nmode = " + entry.getKey() + "\n").getBytes(StandardCharsets.UTF_8));
            project.createPublisher();
            assertEquals(entry.getValue(), project.getPublisher().getClass());
        }
    }

    // Omitting the mode uses the same Zip default as the Editor settings form.
    @Test
    public void testDefaultPublisher() throws Exception {
        fileSystem.addFile("liveupdate.settings", "[liveupdate]\n".getBytes(StandardCharsets.UTF_8));
        project.createPublisher();
        assertTrue(project.getPublisher() instanceof ZipPublisher);
    }

    // Legacy Amazon and invalid modes report a useful error on the settings resource.
    @Test
    public void testUnsupportedPublisher() throws Exception {
        for (String mode : new String[] {"Amazon", "Unknown", ""}) {
            IResource settings = fileSystem.addFile("liveupdate.settings", ("[liveupdate]\nmode = " + mode + "\n").getBytes(StandardCharsets.UTF_8));
            try {
                project.createPublisher();
                fail("Expected an error for publishing mode '" + mode + "'");
            } catch (CompileExceptionError e) {
                assertSame(settings, e.getResource());
                assertTrue(e.getMessage().contains("Unsupported Live Update publishing mode '" + mode + "'"));
                assertTrue(e.getMessage().contains("Select Zip or Folder"));
            }
        }
    }

    // Legacy Amazon settings do not prevent builds when publishing is disabled.
    @Test
    public void testDisabledAmazonPublisher() throws Exception {
        fileSystem.addFile("liveupdate.settings", "[liveupdate]\nmode = Amazon\namazon-bucket = bucket\n".getBytes(StandardCharsets.UTF_8));
        project.setOption("liveupdate", "false");
        project.createPublisher();
        assertTrue(project.getPublisher() instanceof NullPublisher);
    }

    // A missing settings file still fails when Live Update publishing is requested.
    @Test
    public void testMissingSettings() throws Exception {
        try {
            project.createPublisher();
            fail("Expected an error for the missing settings file");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage().contains("There is no liveupdate.settings file"));
        }
    }

    // Ordinary builds continue to work without a Live Update settings file.
    @Test
    public void testDisabledPublisherWithoutSettings() throws Exception {
        project.setOption("liveupdate", "false");
        project.createPublisher();
        assertTrue(project.getPublisher() instanceof NullPublisher);
    }
}
