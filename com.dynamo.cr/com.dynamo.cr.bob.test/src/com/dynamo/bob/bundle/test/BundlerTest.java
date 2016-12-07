package com.dynamo.bob.bundle.test;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;


import javax.imageio.ImageIO;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.eclipse.swt.widgets.Display;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.DefaultFileSystem;

@RunWith(Parameterized.class)
public class BundlerTest {

    private String contentRoot;
    private String outputDir;
    private String contentRootUnused;
    private Platform platform;

    @Parameters
    public static Collection<Platform[]> data() {
        Platform[][] data = new Platform[][] { { Platform.X86Darwin}, {Platform.X86Linux}, {Platform.X86Win32}, {Platform.X86_64Win32}, {Platform.Armv7Android}, {Platform.JsWeb}};
        return Arrays.asList(data);
    }

    public BundlerTest(Platform platform) {
        this.platform = platform;
    }

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        outputDir = Files.createTempDirectory(null).toFile().getAbsolutePath();
        createFile(contentRoot, "game.project", "[display]\nwidth=640\nheight=480\n");

        contentRootUnused = Files.createTempDirectory(null).toFile().getAbsolutePath();
        createFile(contentRootUnused, "game.project", "[display]\nwidth=640\nheight=480\n[bootstrap]\nmain_collection = /main.collectionc\n");

        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        BufferedImage image = new BufferedImage(16, 16, BufferedImage.TYPE_4BYTE_ABGR);
        ImageIO.write(image, "png", new File(contentRoot, "test.png"));
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteDirectory(new File(outputDir));
        FileUtils.deleteDirectory(new File(contentRootUnused));
    }

    void build() throws IOException, CompileExceptionError {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");

        OsgiScanner scanner = new OsgiScanner(FrameworkUtil.getBundle(Project.class));
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        project.setOption("platform", platform.getPair());
        project.setOption("archive", "true");
        project.setOption("bundle-output", outputDir);
        project.findSources(contentRoot, new HashSet<String>());
        List<TaskResult> result = project.build(new NullProgress(), "clean", "build", "bundle");
        for (TaskResult taskResult : result) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }
    }

    HashSet<String> readDarcEntries(String root) throws IOException
    {
        // Read the path entries in the resulting archive
        RandomAccessFile darcFile = new RandomAccessFile(root + "/build/game.darc", "r");
        darcFile.readInt();  // Version
        darcFile.readInt();  // Pad
        darcFile.readLong(); // Userdata
        int stringPoolOffset = darcFile.readInt();
        int stringPoolSize = darcFile.readInt();

        // Seek to path entries
        HashSet<String> entries = new HashSet<String>();
        darcFile.seek(stringPoolOffset);
        while (darcFile.getFilePointer() < stringPoolOffset + stringPoolSize) {
            String path = "";
            byte[] b = {0};
            while((b[0] = darcFile.readByte()) != 0) {
                path = path + new String(b);
            }
            entries.add(path);
        }
        darcFile.close();
        return entries;
    }

    void buildWithUnused() throws IOException, CompileExceptionError {

        Project project = new Project(new DefaultFileSystem(), contentRootUnused, "build");

        OsgiScanner scanner = new OsgiScanner(FrameworkUtil.getBundle(Project.class));
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        project.setOption("platform", platform.getPair());
        project.setOption("keep-unused", "true");
        project.setOption("archive", "true");
        project.findSources(contentRootUnused, new HashSet<String>());
        List<TaskResult> result = project.build(new NullProgress(), "clean", "build");
        for (TaskResult taskResult : result) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }

        HashSet<String> entries = readDarcEntries(contentRootUnused);

        assertTrue(entries.contains("/main.collectionc"));
        assertTrue(entries.contains("/unused.collectionc"));
    }



    @Test
    public void testBundle() throws IOException, ConfigurationException, CompileExceptionError {
        createFile(contentRoot, "test.icns", "test_icon");
        build();
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        file.deleteOnExit();
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

    @Test
    public void testUnusedCollections() throws IOException, ConfigurationException, CompileExceptionError {
        createFile(contentRootUnused, "main.collection", "name: \"default\"\nscale_along_z: 0\n");
        createFile(contentRootUnused, "unused.collection", "name: \"unused\"\nscale_along_z: 0\n");
        buildWithUnused();
    }

    @Test
    public void testCustomResourcesFile() throws IOException, ConfigurationException, CompileExceptionError {
        createFile(contentRoot, "game.project", "[project]\ncustom_resources=m.txt\n[display]\nwidth=640\nheight=480\n");
        createFile(contentRoot, "m.txt", "dummy");
        build();
        HashSet<String> entries = readDarcEntries(contentRoot);
        assertTrue(entries.contains("/m.txt"));
    }

    @Test
    public void testCustomResourcesDirs() throws IOException, ConfigurationException, CompileExceptionError {
        File cust = new File(contentRoot, "custom");
        cust.mkdir();
        File sub1 = new File(cust, "sub1");
        File sub2 = new File(cust, "sub2");
        sub1.mkdir();
        sub2.mkdir();
        createFile(contentRoot, "m.txt", "dummy");
        createFile(sub1.getAbsolutePath(), "s1-1.txt", "dummy");
        createFile(sub1.getAbsolutePath(), "s1-2.txt", "dummy");
        createFile(sub2.getAbsolutePath(), "s2-1.txt", "dummy");
        createFile(sub2.getAbsolutePath(), "s2-2.txt", "dummy");

        createFile(contentRoot, "game.project", "[project]\ncustom_resources=custom,m.txt\n[display]\nwidth=640\nheight=480\n");
        build();
        HashSet<String> entries = readDarcEntries(contentRoot);
        assertTrue(entries.contains("/m.txt"));
        assertTrue(entries.contains("/custom/sub1/s1-1.txt"));
        assertTrue(entries.contains("/custom/sub1/s1-2.txt"));
        assertTrue(entries.contains("/custom/sub2/s2-1.txt"));
        assertTrue(entries.contains("/custom/sub2/s2-2.txt"));

        createFile(contentRoot, "game.project", "[project]\ncustom_resources=custom/sub2\n[display]\nwidth=640\nheight=480\n");
        build();
        entries = readDarcEntries(contentRoot);
        assertFalse(entries.contains("/m.txt"));
        assertFalse(entries.contains("/custom/sub1/s1-1.txt"));
        assertFalse(entries.contains("/custom/sub1/s1-2.txt"));
        assertTrue(entries.contains("/custom/sub2/s2-1.txt"));
        assertTrue(entries.contains("/custom/sub2/s2-2.txt"));
    }
}
