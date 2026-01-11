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

import static org.junit.Assert.assertEquals;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.FileUtil;

import java.nio.file.Files;
import java.io.File;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.text.ParseException;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.io.FileUtils;

public class BobProjectPropertiesTest {

    private String contentRoot;

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

        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.loadProjectFile(true);
        BobProjectProperties properties = project.getProjectProperties();

        assertEquals(true, properties.isPrivate("project", "custom_property"));
    }

    @Test
    public void testOverrideExtensionMetaProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", "[project]\ntitle = random\ncustom_property = just content");
        createFile(contentRoot, "extension1/ext.manifest", "name: Extension1\n");
        createFile(contentRoot, "extension1/"+BobProjectProperties.PROPERTIES_FILE, "[project]\ncustom_property.private = 1");
        createFile(contentRoot, BobProjectProperties.PROPERTIES_PROJECT_FILE, "[project]\ncustom_property.private = 0");

        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.loadProjectFile(true);
        BobProjectProperties properties = project.getProjectProperties();

        assertEquals(false, properties.isPrivate("project", "custom_property"));
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        FileUtil.deleteOnExit(file);
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }
}
