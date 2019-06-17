package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipFile;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.codec.binary.Base64;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.eclipse.jetty.util.resource.Resource;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;

public class ExtenderUtilTest {

    private MockFileSystem fileSystem;
    private Project project;
    private File tmpDir;

    private void createDirs(MockFileSystem fileSystem, String path) {
        if (!path.isEmpty()) {
            String accumulate = "";
            String[] parts = path.split("/");
            for (int i = 0; i < parts.length - 1; ++i) {
                accumulate += parts[i] + "/";

                fileSystem.addDirectory(accumulate);
            }
        }
    }

    private void createFile(MockFileSystem fileSystem, String path, byte[] data) throws IOException {
        // For simplicity, we create a directory with the name of the file...
        createDirs(fileSystem, path);
        // ...but we immediately replace it with the intended mock file
        fileSystem.addFile(path,  data);
    }


    @Before
    public void setUp() throws Exception {

        tmpDir = Files.createTempDirectory("defold_").toFile();
        tmpDir.deleteOnExit();

        fileSystem = new MockFileSystem();
        createFile(fileSystem, "extension1/ext.manifest", "name: Extension1\n".getBytes());
        createFile(fileSystem, "extension1/src/ext1.cpp", "// ext1.cpp".getBytes());
        createFile(fileSystem, "extension1/res/android/res/values/values.xml", "<xml>/<xml>".getBytes());

        createFile(fileSystem, "extension2/ext.manifest", "name: Extension2\n".getBytes());
        createFile(fileSystem, "extension2/src/ext1.cpp", "// ext2.cpp".getBytes());
        createFile(fileSystem, "extension2/res/android/res/com.foo.org/values/values.xml", "<xml>/<xml>".getBytes());

        createFile(fileSystem, "extension3/ext.manifest", "name: Extension3\n".getBytes());
        createFile(fileSystem, "extension3/src/ext1.cpp", "// ext3.cpp".getBytes());
        createFile(fileSystem, "extension3/res/android/res/com.foo.org/values/values.xml", "<xml>/<xml>".getBytes());
        createFile(fileSystem, "extension3/res/android/res/com.bar.org/values/values.xml", "<xml>/<xml>".getBytes());

        createDirs(fileSystem, "notextension/res/android/res/bla");

        bundle = Platform.getBundle("com.dynamo.cr.bob");

        project = new Project(fileSystem, tmpDir.getAbsolutePath(), "build/default");
    }

    @After
    public void tearDown() throws Exception {
        project.dispose();
    }
    @Test
    public void testIsAndroidAssetDirectory() throws Exception {
        assertTrue(ExtenderUtil.isAndroidAssetDirectory(project, "extension1/res/android/res/"));
        assertFalse(ExtenderUtil.isAndroidAssetDirectory(project, "extension2/res/android/res/"));
    }

    @Test
    public void testGetAndroidResources() throws Exception {
        Map<String, IResource> resources = ExtenderUtil.getAndroidResources(project);
        assertFalse(resources.isEmpty());
    }
}

