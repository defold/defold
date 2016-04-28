package com.defold.extender.test;

import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;

import org.apache.commons.io.FileUtils;
import org.junit.Test;
import org.yaml.snakeyaml.Yaml;

import com.defold.extender.Configuration;
import com.defold.extender.Extender;

public class ExtenderTest {

    @Test
    public void testBuild() throws IOException, InterruptedException {

        Configuration config = new Yaml().loadAs(FileUtils.readFileToString(new File("test-data/config.yml")), Configuration.class);

        Extender ext = new Extender(config, "x86-osx", new File("test-data/ext"));
        File engine = ext.buildEngine();
        assertTrue(engine.isFile());
        System.out.println(engine);
        ext.dispose();
    }

}