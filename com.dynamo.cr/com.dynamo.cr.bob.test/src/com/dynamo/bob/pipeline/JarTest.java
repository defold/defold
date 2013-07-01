package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;

import org.apache.commons.io.FilenameUtils;
import org.junit.Test;

import com.dynamo.bob.util.Exec;

public class JarTest {

    private int bob(String command) throws IOException {
        String jarPath = FilenameUtils.concat(System.getenv("DYNAMO_HOME"), "share/java/bob.jar");
        return Exec.exec("java", "-jar", jarPath, "-i", "test", command);
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
