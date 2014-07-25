package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.eclipse.swt.widgets.Display;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ConsoleProgress;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IFileSystem;

public class JarTest {

    private int bob(String command) throws IOException, InterruptedException, CompileExceptionError, URISyntaxException {
        String jarPath = "../com.dynamo.cr.bob/dist/bob-light.jar";
        Process p = Runtime.getRuntime().exec(new String[] { "java", "-jar", jarPath, "-v", "-i", "test", command });
        BufferedReader in = new BufferedReader(new InputStreamReader(p.getInputStream()));
        String line;
        while ((line = in.readLine()) != null) {
            System.out.println(line);
        }
        return p.waitFor();
    }

    @Test
    public void testBuild() throws Exception {
        String[] outputs = new String[] {"atlas.texturec", "atlas.texturesetc", "test_45.texturec"};
        int result = bob("distclean");
        assertEquals(0, result);
        for (String output : outputs) {
            assertFalse(new File("build/default/test/" + output).exists());
        }
        result = bob("build");
        assertEquals(0, result);
        for (String output : outputs) {
            assertTrue(new File("build/default/test/" + output).exists());
        }
    }

    /**
     * The purpose with this test is to be as close as possible to testBuild, but make debugging easier by running the same JVM.
     * @throws Exception
     */
    @Test
    public void testNonJarBuild() throws Exception {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        IFileSystem fs = new DefaultFileSystem();
        String cwd = new File(".").getAbsolutePath();
        Project p = new Project(fs, cwd, "build/default");

        IClassScanner scanner = new TestClassLoaderScanner();
        p.scan(scanner, "com.dynamo.bob");
        p.scan(scanner, "com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", "build/default"));

        p.findSources("test", skipDirs);
        List<TaskResult> result = p.build(new ConsoleProgress(), "distclean", "build");
        assertFalse(result.isEmpty());
        boolean res = true;
        for (TaskResult taskResult : result) {
            if (!taskResult.isOk()) {
                res = false;
                break;
            }
        }
        assertTrue(res);
    }
}
