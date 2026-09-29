// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0; https://www.defold.com/license
package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import java.io.ByteArrayOutputStream;
import org.junit.Test;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.TextureUtil;

public class Ktx2TextureBuilderTest extends AbstractProtoBuilderTest {
    @Test
    public void standaloneAndVirtualTexturesCompileToTexturec() throws Exception {
        byte[] source = Ktx2TextureGeneratorTest.fixture("uastc-zstd");
        ByteArrayOutputStream expected = new ByteArrayOutputStream();
        TextureUtil.writeGenerateResultToOutputStream(TextureGenerator.generate(source, null, false), expected);
        for (String path : new String[] {"/source.ktx2", "/models/robot.glb/images/0.ktx2"}) {
            addFile(path, source);
            Task task = getProject().createTask(getProject().getResource(path));
            assertNotNull(task);
            assertTrue(task.getBuilder() instanceof TextureBuilder);
            task.getBuilder().build(task);
            assertTrue(task.output(0).getPath().endsWith(".texturec"));
            assertArrayEquals(expected.toByteArray(), task.output(0).getContent());
            assertEquals(path.substring(0, path.length() - 5) + ".texturec", ProtoBuilders.replaceTextureName(path));
        }
    }

    @Test
    public void malformedImageErrorNamesTheResource() throws Exception {
        byte[] source = Ktx2TextureGeneratorTest.fixture("uastc");
        source[80] = 0; source[81] = 0;
        addFile("/broken.ktx2", source);
        Task task = getProject().createTask(getProject().getResource("/broken.ktx2"));
        try {
            task.getBuilder().build(task);
            fail("Malformed KTX2 should fail the resource build");
        } catch (CompileExceptionError expected) {
            assertEquals(task.input(0), expected.getResource());
            assertTrue(expected.getMessage().contains("KTX2"));
        }
    }
}
