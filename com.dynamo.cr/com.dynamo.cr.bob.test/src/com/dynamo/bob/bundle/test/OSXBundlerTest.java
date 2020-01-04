package com.dynamo.bob.bundle.test;

import static org.apache.commons.io.FilenameUtils.concat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.HashSet;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.bundle.OSXBundler;
import com.dynamo.bob.fs.DefaultFileSystem;

public class OSXBundlerTest {

    private String contentRoot;
    private String outputDir;

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        outputDir = Files.createTempDirectory(null).toFile().getAbsolutePath();
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteDirectory(new File(outputDir));
    }

    private void assertPList() {
        assertTrue(new File(concat(outputDir, "unnamed.app/Contents/Info.plist")).isFile());
    }

    private void assertExe() throws IOException {
        assertTrue(new File(concat(outputDir, "unnamed.app/Contents/MacOS/unnamed")).isFile());
    }

    protected void createBuiltins() throws IOException {
        createFile(contentRoot, "logic/main.collection", "name: \"default\"\nscale_along_z: 0\n");
        createFile(contentRoot, "builtins/render/default.render", "script: \"/builtins/render/default.render_script\"\n");
        createFile(contentRoot, "builtins/render/default.render_script", "");
        createFile(contentRoot, "builtins/render/default.display_profiles", "");
        createFile(contentRoot, "builtins/graphics/default.texture_profiles", "");
        createFile(contentRoot, "builtins/input/default.gamepads", "");
        createFile(contentRoot, "builtins/manifests/osx/Info.plist", "");
        createFile(contentRoot, "builtins/manifests/ios/Info.plist", "");
        createFile(contentRoot, "builtins/manifests/android/AndroidManifest.xml", "");
        createFile(contentRoot, "builtins/manifests/web/engine_template.html", "");
        createFile(contentRoot, "input/game.input_binding", "");
    }

    void build() throws IOException, CompileExceptionError, MultipleCompileException {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setPublisher(new NullPublisher(new PublisherSettings()));

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        project.setOption("platform", Platform.X86_64Darwin.getPair());
        project.setOption("archive", "true");
        project.setOption("bundle-output", outputDir);
        project.findSources(contentRoot, new HashSet<String>());
        project.build(new NullProgress(), "clean", "build", "bundle");
    }

public void listDirectory(File dir, int level) {
    if (level == 0) {
        System.out.printf("PATH: %s\n", dir.getAbsolutePath());
    }
    File[] firstLevelFiles = dir.listFiles();
    if (firstLevelFiles != null && firstLevelFiles.length > 0) {
        for (File aFile : firstLevelFiles) {
            for (int i = 0; i < level; i++) {
                System.out.print("\t");
            }
            if (aFile.isDirectory()) {
                System.out.println("[" + aFile.getName() + "]");
                listDirectory(aFile, level + 1);
            } else {
                System.out.println(aFile.getName());
            }
        }
    }
}
    @Test
    public void testBundle() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createBuiltins();
        createFile(contentRoot, "test.icns", "test_icon");
        createFile(contentRoot, "game.project", "[osx]\napp_icon=test.icns\n");


    System.out.printf("MAWE contentRoot:\n");
        listDirectory(new File(contentRoot), 0);

        build();
        assertEquals("test_icon", readFile(concat(outputDir, "unnamed.app/Contents/Resources"), OSXBundler.ICON_NAME));
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

}
