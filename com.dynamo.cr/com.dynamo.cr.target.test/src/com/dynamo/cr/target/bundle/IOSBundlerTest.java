package com.dynamo.cr.target.bundle;

import static org.apache.commons.io.FilenameUtils.concat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.ProjectProperties;

public class IOSBundlerTest {

    private String exe;
    private String contentRoot;
    private String outputDir;
    private ProjectProperties projectProperties;
    private String provisioningProfile;

    @Before
    public void setUp() throws Exception {
        exe = tempFile("test_exe");
        contentRoot = tempDir();
        outputDir = tempDir();
        projectProperties = new ProjectProperties();
        createFile(contentRoot, "game.projectc", "game.projectc data");
        createFile(contentRoot, "game.arc", "game.arc data");
        provisioningProfile = createFile(contentRoot, "test.mobileprovision", "test provision");
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteDirectory(new File(outputDir));
    }

    private void assertPList() {
        assertTrue(new File(concat(outputDir, "MyApp.app/Info.plist")).isFile());
    }

    private void assertExe() throws IOException {
        assertEquals("test_exe", readFile(concat(outputDir, "MyApp.app"), new File(exe).getName()));
    }

    @Test
    public void testBundle() throws IOException, ConfigurationException {
        createFile(contentRoot, "test.png", "test_icon");
        projectProperties.putStringValue("ios", "app_icon_57x57", "test.png");
        projectProperties.putStringValue("project", "title", "MyApp");
        IOSBundler bundler = new IOSBundler(null, provisioningProfile, projectProperties, exe, contentRoot, contentRoot, outputDir);
        bundler.bundleApplication();

        assertFalse(new File(concat(outputDir, "MyApp.app/test.png")).exists());
        assertEquals("game.projectc data", readFile(concat(outputDir, "MyApp.app"), "game.projectc"));
        assertEquals("game.arc data", readFile(concat(outputDir, "MyApp.app"), "game.arc"));
        assertExe();
        assertPList();
    }

    @Test
    public void testNoIconSpecified() throws IOException, ConfigurationException {
        projectProperties.putStringValue("project", "title", "MyApp");
        IOSBundler bundler = new IOSBundler(null, provisioningProfile, projectProperties, exe, contentRoot, contentRoot, outputDir);
        bundler.bundleApplication();

        assertExe();
        assertPList();
    }

    private String readFile(String dir, String name) throws IOException {
        return FileUtils.readFileToString(new File(dir, name));
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        file.deleteOnExit();
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

    private String tempFile(String content) throws IOException {
        File file = File.createTempFile("test", "");
        file.deleteOnExit();
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

    private String tempDir() throws IOException {
        File file = File.createTempFile("test", "");
        // Yes this is "racy"...
        file.delete();
        file.mkdir();
        return file.getAbsolutePath();
    }

}
