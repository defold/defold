package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.Message;

public class ShaderProgramBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private final String vp =
            "attribute vec4 position; \n" +
            "varying vec4 fragColor; \n" +
            "uniform vec4 color; \n" +
            "void main(){ \n" +
            "fragColor = color;\n" +
            "gl_Position = position; \n" +
            "}\n";

    private final String fp =
            "varying vec4 fragColor; \n" +
            "void main(){ \n" +
            "gl_FragColor = fragColor; \n" +
            "}\n";

    @Test
    public void testShaderPrograms() throws Exception {
        // Test GL vp
        List<Message> outputs = build("/test_shader.vp", vp);
        ShaderDesc shader = (ShaderDesc)outputs.get(0);
        assertNotNull(shader.getShaders(0).getSource());
        assertEquals(ShaderDesc.Language.LANGUAGE_GLSL, shader.getShaders(0).getLanguage());
        switch(Platform.getHostPlatform())
        {
            case X86Darwin:
            case X86_64Darwin:
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getBinary());
                assertEquals(ShaderDesc.Language.LANGUAGE_MSL, shader.getShaders(1).getLanguage());
                break;
            default:
                assertEquals(1, shader.getShadersCount());
        }

        // Test GL fp
        outputs = build("/test_shader.fp", fp);
        shader = (ShaderDesc)outputs.get(0);
        assert(shader.getShaders(0).getLanguage() == ShaderDesc.Language.LANGUAGE_GLSL);
        assertNotNull(shader.getShaders(0).getSource());
        assertEquals(ShaderDesc.Language.LANGUAGE_GLSL, shader.getShaders(0).getLanguage());
        switch(Platform.getHostPlatform())
        {
            case X86Darwin:
            case X86_64Darwin:
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getBinary());
                assertEquals(ShaderDesc.Language.LANGUAGE_MSL, shader.getShaders(1).getLanguage());
                break;

            default:
                assertEquals(1, shader.getShadersCount());
        }

        // Test GLES vp
        outputs = build("/test_shader.vp", "#version 310 es\n" + vp);
        shader = (ShaderDesc)outputs.get(0);
        switch(Platform.getHostPlatform())
        {
            case X86Darwin:
            case X86_64Darwin:
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getBinary());
                assertEquals(ShaderDesc.Language.LANGUAGE_MSL, shader.getShaders(1).getLanguage());
                break;

            default:
                assertEquals(1, shader.getShadersCount());
        }

        // Test GLES fp
        outputs = build("/test_shader.fp", "#version 310 es\n" + fp);
        shader = (ShaderDesc)outputs.get(0);
        switch(Platform.getHostPlatform())
        {
            case X86Darwin:
            case X86_64Darwin:
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getBinary());
                assertEquals(ShaderDesc.Language.LANGUAGE_MSL, shader.getShaders(1).getLanguage());
                break;

            default:
                assertEquals(1, shader.getShadersCount());
        }
    }
}
