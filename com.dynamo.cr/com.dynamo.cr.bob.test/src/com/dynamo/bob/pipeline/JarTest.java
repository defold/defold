package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URISyntaxException;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;

public class JarTest {

    private int bob(String command) throws IOException, InterruptedException, CompileExceptionError, URISyntaxException {
        String jarPath = "../com.dynamo.cr.bob/dist/bob.jar";
        Process p = Runtime.getRuntime().exec(new String[] { "java", "-jar", jarPath, "-i", "test", command });
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

}
