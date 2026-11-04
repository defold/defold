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

package com.dynamo.bob.test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.BobProjectProperties;
import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.text.ParseException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

public class BobProjectPropertiesTest {

    private String contentRoot;

    static final String BASE_DEPENDENCIES = "[project]\ndependencies#0 = a\ndependencies#1 = b\ndependencies#2 = c\n";

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
    }

    BobProjectProperties createProperties() throws IOException, ParseException {
        BobProjectProperties properties = new BobProjectProperties();
        properties.loadDefaultMetaFile();
        return properties;
    }

    void load(BobProjectProperties properties, String load) throws IOException, ParseException {
        properties.load(new ByteArrayInputStream(load.getBytes()));
    }

    String[] loadDependencies(String... files) throws IOException, ParseException {
        BobProjectProperties properties = createProperties();
        for (String file : files) {
            load(properties, file);
        }
        return properties.getStringArrayValue("project", "dependencies");
    }

    void testValue(String p, String c, String k, Object expected) throws IOException, ParseException {
        BobProjectProperties properties = createProperties();
        load(properties, p);
        Map<String, Map<String, Object>> map = properties.createTypedMap(new BobProjectProperties.PropertyType[]{BobProjectProperties.PropertyType.BOOL});

        Object actual = map.get(c).get(k);
        assertEquals(expected, actual);
    }

    @Test
    public void testTypedProperties() throws IOException, ParseException {
        testValue("[project]\nwrite_log=0", "project", "write_log", false);
        testValue("[project]\nwrite_log=1", "project", "write_log", true);
        testValue("", "project", "write_log", false);
        testValue("", "project", "title", "unnamed");
    }

    @Test
    public void testProperties() throws IOException, ParseException {
        BobProjectProperties properties = createProperties();

        assertEquals(false, properties.getBooleanValue("html5", "doesn't_exist", false));
        assertEquals(Integer.valueOf(834), properties.getIntValue("html5", "doesn't_exist", 834));

        assertEquals(false, properties.getBooleanValue("display", "fullscreen"));
        assertEquals(true, properties.getBooleanValue("sound", "use_thread"));
        assertEquals(Integer.valueOf(960), properties.getIntValue("display", "width"));
    }

    @Test
    public void testExtensionMetaProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", "[project]\ntitle = random\ncustom_property = just content");
        createFile(contentRoot, "extension1/ext.manifest", "name: Extension1\n");
        createFile(contentRoot, "extension1/"+BobProjectProperties.PROPERTIES_FILE, "[project]\ncustom_property.private = 1");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.loadProjectFile(true);
            BobProjectProperties properties = project.getProjectProperties();

            assertEquals(true, properties.isPrivate("project", "custom_property"));
        }
    }

    @Test
    public void testOverrideExtensionMetaProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", "[project]\ntitle = random\ncustom_property = just content");
        createFile(contentRoot, "extension1/ext.manifest", "name: Extension1\n");
        createFile(contentRoot, "extension1/"+BobProjectProperties.PROPERTIES_FILE, "[project]\ncustom_property.private = 1");
        createFile(contentRoot, BobProjectProperties.PROPERTIES_PROJECT_FILE, "[project]\ncustom_property.private = 0");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.loadProjectFile(true);
            BobProjectProperties properties = project.getProjectProperties();

            assertEquals(false, properties.isPrivate("project", "custom_property"));
        }
    }

    @Test
    public void testMergedStringArrayValueIncludesExtensionDefaults() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", "[project]\ntitle = random\ncustom_resources = /project_resource");
        createFile(contentRoot, "extension1/ext.manifest", "name: Extension1\n");
        createFile(contentRoot, "extension1/"+BobProjectProperties.PROPERTIES_FILE, "[project]\ncustom_resources.default = /extension1_resource");
        createFile(contentRoot, "extension2/ext.manifest", "name: Extension2\n");
        createFile(contentRoot, "extension2/"+BobProjectProperties.PROPERTIES_FILE, "[project]\ncustom_resources.default = /extension2_resource");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.loadProjectFile(true);
            BobProjectProperties properties = project.getProjectProperties();

            String[] customResources = properties.getStringArrayValueMerged("project", "custom_resources", new String[0]);
            assertEquals(new HashSet<>(Arrays.asList("/extension1_resource", "/extension2_resource", "/project_resource")),
                         new HashSet<>(Arrays.asList(customResources)));
        }
    }

    // Verifies that "+=" and "-=" edit a list from earlier files by value and in order, and that indexed entries still replace the whole list
    @Test
    public void testArrayOperations() throws IOException, ParseException {
        assertArrayEquals(new String[] {"a", "c", "b2", "d"},
                          loadDependencies(BASE_DEPENDENCIES, "[project]\ndependencies -= b\ndependencies += b2\ndependencies += a", "[project]\ndependencies += d"));
        assertArrayEquals(new String[] {"x"}, loadDependencies(BASE_DEPENDENCIES, "[project]\ndependencies += d", "[project]\ndependencies#0 = x"));
        assertArrayEquals(new String[] {"a"}, loadDependencies("[project]\ndependencies += a"));
    }

    // Verifies that whitespace around "+=" and "-=" is optional, values may be comma separated, and values may contain "="
    @Test
    public void testArrayOperationSyntax() throws IOException, ParseException {
        assertArrayEquals(new String[] {"a", "c", "d", "e", "https://host/f.zip?token=1"},
                          loadDependencies(BASE_DEPENDENCIES, "[project]\ndependencies+=d\ndependencies -=b\n  dependencies   +=   e , https://host/f.zip?token=1  "));
    }

    // Verifies that malformed lines, mixing a full value with "+=" or "-=" in one file, removing a missing value, and editing a non-list setting are errors,
    // so a stale override (e.g. after a version bump in the base) fails instead of building with the wrong dependencies
    @Test
    public void testInvalidArrayOperations() throws IOException {
        for (String file : List.of("dependencies + = a", "dependencies ++= a", "dependencies#1 += a", "dependencies.value += a", "dependencies +=", "+= a",
                                   "dependencies#0 = a\ndependencies += b", "dependencies = a\ndependencies -= a", "dependencies -= x", "write_log += 1")) {
            try {
                loadDependencies(BASE_DEPENDENCIES, "[project]\n" + file);
                fail("Expected ParseException for: " + file);
            } catch (ParseException e) {
                // Expected
            }
        }
    }

    // Verifies that "+=" and "-=" in a --settings file are applied on top of game.project when Bob loads the project
    @Test
    public void testArrayOperationsInSettingsFile() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", BASE_DEPENDENCIES);
        String settings = createFile(contentRoot, "game.project.mobile", "[project]\ndependencies -= a\ndependencies += d");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.addPropertyFile(settings);
            project.loadProjectFile(false);
            assertArrayEquals(new String[] {"b", "c", "d"}, project.getProjectProperties().getStringArrayValue("project", "dependencies"));
        }
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }
}
