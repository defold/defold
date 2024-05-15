// Copyright 2020-2024 The Defold Foundation
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

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ConsoleProgress;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IFileSystem;

public class JarTest {

    private int bob(String command) throws IOException, InterruptedException, CompileExceptionError, URISyntaxException {
        String jarPath = "../com.dynamo.cr.bob/dist/bob.jar";
        Process p = Runtime.getRuntime().exec(new String[] { "java", "-jar", jarPath, "-v", "-r", "test", "-i", ".", command });
        BufferedReader in = new BufferedReader(new InputStreamReader(p.getInputStream()));
        BufferedReader ein = new BufferedReader(new InputStreamReader(p.getErrorStream()));
        String line;
        while ((line = in.readLine()) != null) {
            System.out.println(line);
        }
        while ((line = ein.readLine()) != null) {
            System.out.println(line);
        }
        return p.waitFor();
    }

    private int bob(String[] commands, String outputMatch) throws IOException, InterruptedException, CompileExceptionError, URISyntaxException {
        String jarPath = "../com.dynamo.cr.bob/dist/bob.jar";
        String[] bobArgs = new String[] { "java", "-jar", jarPath, "-v", "-r", "test", "-i", "."};
        String[] allArgs = new String[bobArgs.length + commands.length];
        System.arraycopy(bobArgs, 0, allArgs, 0, bobArgs.length);
        System.arraycopy(commands, 0, allArgs, bobArgs.length, commands.length);
        Process p = Runtime.getRuntime().exec(allArgs);
        BufferedReader in = new BufferedReader(new InputStreamReader(p.getInputStream()));
        BufferedReader ein = new BufferedReader(new InputStreamReader(p.getErrorStream()));
        String line;
        while ((line = in.readLine()) != null) {
            System.out.println(line);
            if (!outputMatch.isEmpty() && line.equals(outputMatch)) {
                return 1337;
            }
        }
        while ((line = ein.readLine()) != null) {
            System.out.println(line);
        }
        return p.waitFor();
    }

    @Test
    public void testBuild() throws Exception {
        String[] outputs = new String[] {"atlas.texturec", "atlas.a.texturesetc", "simple_box_2bones_generated_0.animationsetc"};
        int result = bob("distclean");
        assertEquals(0, result);
        for (String output : outputs) {
            assertFalse(new File("test/build/default/" + output).exists());
        }
        result = bob("build");
        assertEquals(0, result);
        for (String output : outputs) {
            assertTrue(new File("test/build/default/" + output).exists());
        }
    }

    @Test
    public void testMapTextureProfilesToCompression() throws Exception {
        String[] args = new String[] {"--texture-profiles", "true"};
        int result = bob(args, "WARNING option 'texture-profiles' is deprecated, use option 'texture-compression' instead.");
        assertEquals(1337, result);
    }

    /**
     * The purpose with this test is to be as close as possible to testBuild, but make debugging easier by running the same JVM.
     * @throws Exception
     */
    @Test
    public void testNonJarBuild() throws Exception {
        IFileSystem fs = new DefaultFileSystem();
        String cwd = new File("test").getAbsolutePath();
        Project p = new Project(fs, cwd, "build/default");
        p.setPublisher(new NullPublisher(new PublisherSettings()));

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        p.scan(scanner, "com.dynamo.bob");
        p.scan(scanner, "com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", "build/default"));

        p.findSources("", skipDirs);
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
