package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.model.proto.ModelProto.Model;
import com.dynamo.rig.proto.Rig.RigScene;
import com.google.protobuf.Message;

public class ModelBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testModel() throws Exception {
        addFile("/test_meshset.dae", "");
        addFile("/test_skeleton.dae", "");
        addFile("/test_animationset.dae", "");
        addFile("/test.material", "");

        StringBuilder src = new StringBuilder();
        src.append(" mesh: \"/test_meshset.dae\"");
        src.append(" material: \"/test.material\"");
        src.append(" skeleton: \"/test_skeleton.dae\"");
        src.append(" animations: \"/test_animationset.dae\"");
        src.append(" default_animation: \"test\"");
        List<Message> outputs = build("/test.model", src.toString());

        Model model = (Model)outputs.get(0);
        assertEquals("/test.rigscenec", model.getRigScene());
        assertEquals("/test.materialc", model.getMaterial());
        assertEquals("test", model.getDefaultAnimation());

        RigScene rigScene = (RigScene)outputs.get(1);
        assertEquals("/test_meshset.meshsetc", rigScene.getMeshSet());
        assertEquals("/test_skeleton.skeletonc", rigScene.getSkeleton());
        assertEquals("/test_animationset_generated_0.animationsetc", rigScene.getAnimationSet());
    }
}
