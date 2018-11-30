package com.dynamo.bob.test;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.text.ParseException;
import java.util.HashSet;
import java.util.List;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertEquals;


import javax.imageio.ImageIO;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.eclipse.swt.widgets.Display;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.BobProjectProperties;

public class ProjectBuildTest {

    private String contentRoot;
    private String projectName = "Unnamed";

    @Before
    public void setUp() throws Exception {
        projectName = "Unnamed";
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        createFile(contentRoot, "game.project", "[display]\nwidth=640\nheight=480\n");

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
    }

    void build() throws IOException, CompileExceptionError, MultipleCompileException {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setPublisher(new NullPublisher(new PublisherSettings()));

        OsgiScanner scanner = new OsgiScanner(FrameworkUtil.getBundle(Project.class));
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        // project.setOption("platform", Platform.X86Win32.getPair());
        project.findSources(contentRoot, new HashSet<String>());
        List<TaskResult> result = project.build(new NullProgress(), "clean", "build");
        for (TaskResult taskResult : result) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }
    }

    // Returns the number of files that will be put into the DARC file
    // Note that the game.project isn't put in the archive either
    protected int createDefaultFiles() throws IOException {
        int count = 0;
        createFile(contentRoot, "logic/main.collection", "name: \"default\"\nscale_along_z: 0\n");
        count++;
        createFile(contentRoot, "builtins/render/default.render", "script: \"/builtins/render/default.render_script\"\n");
        count++;
        createFile(contentRoot, "builtins/render/default.render_script", "");
        count++;
        createFile(contentRoot, "builtins/render/default.display_profiles", "");
        count++;
        createFile(contentRoot, "builtins/input/default.gamepads", "");
        count++;
        createFile(contentRoot, "input/game.input_binding", "");
        count++;

        return count;
    }

    @Test
    public void testBuild() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        build();
    }

    static private void checkProjectSetting(BobProjectProperties properties, String category, String key, String expectedValue)
    {
        assertEquals(expectedValue, properties.getStringValue(category, key));
    }
    
    @Test
    public void testGamePropertiesBuildtimeTransform() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
    	projectName = "Game Project Properties Transform";
    	createDefaultFiles();
    	createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +"\ndependencies = http://test.com/test.zip\n\n[display]" + "\nvariable_dt = 1\n" + "vsync = 1\n" + "update_frequency = 30\n");
    	build();
    	BobProjectProperties outputProps = new BobProjectProperties();
    	outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));
    	
    	checkProjectSetting(outputProps, "display", "vsync", "0");
    	checkProjectSetting(outputProps, "display", "update_frequency", "0");
    }

    @Test
    public void testGameProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        projectName = "Game Project Properties";
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +"\ndependencies = http://test.com/test.zip\n\n[custom]\nlove = defold\n");
        build();
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        checkProjectSetting(outputProps, "project", "title", projectName);

        // Non existent property
        checkProjectSetting(outputProps, "project", "doesn't_exist", null);

        // Default boolean value
        checkProjectSetting(outputProps, "script", "shared_state", "0");
        checkProjectSetting(outputProps, "display", "vsync", "1");
        checkProjectSetting(outputProps, "display", "update_frequency", "0");

        // Default number value
        checkProjectSetting(outputProps, "display", "width", "960");

        // Custom property
        checkProjectSetting(outputProps, "custom", "love", "defold");

        // project.dependencies entry should be removed
        checkProjectSetting(outputProps, "project", "dependencies", null);

        // Compiled resource
        checkProjectSetting(outputProps, "display", "display_profiles", "/builtins/render/default.display_profilesc");

        // Copy-only resource
        checkProjectSetting(outputProps, "osx", "infoplist", "/builtins/manifests/osx/Info.plist");

        // Check so that empty defaults are not included
        checkProjectSetting(outputProps, "tracking", "app_id", null);

        // Check so empty custom properties are included as empty strings
        checkProjectSetting(outputProps, "custom", "should_be_empty", "");
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        file.deleteOnExit();
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

}
