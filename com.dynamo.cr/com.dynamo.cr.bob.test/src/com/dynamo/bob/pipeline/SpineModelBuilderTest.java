package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;

import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.google.protobuf.Message;

public class SpineModelBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testSpineModel() throws Exception {
        addFile("/test.spinescene", "");
        addFile("/test.material", "");
        StringBuilder src = new StringBuilder();
        src.append("spine_scene: \"/test.spinescene\"");
        src.append(" material: \"/test.material\"");
        src.append(" default_animation: \"test\"");
        src.append(" skin: \"test\"");
        src.append(" blend_mode: BLEND_MODE_ALPHA");
        List<Message> outputs = build("/test.spinemodel", src.toString());
        SpineModelDesc scene = (SpineModelDesc)outputs.get(0);

        assertEquals("/test.rigscenec", scene.getSpineScene());
        assertEquals("/test.materialc", scene.getMaterial());
        assertEquals("test", scene.getDefaultAnimation());
        assertEquals("test", scene.getSkin());
        assertEquals(SpineModelDesc.BlendMode.BLEND_MODE_ALPHA, scene.getBlendMode());
    }
}
