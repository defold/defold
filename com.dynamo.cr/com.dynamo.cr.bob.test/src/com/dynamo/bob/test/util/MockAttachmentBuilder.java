package com.dynamo.bob.test.util;

import com.dynamo.bob.util.RigUtil.AttachmentBuilder;

public class MockAttachmentBuilder extends AttachmentBuilder {

    public MockAttachmentBuilder(com.dynamo.rig.proto.Rig.MeshAnimationTrack.Builder builder) {
        super(builder);
    }

    public int GetMeshAttachment(int index) {
        return builder.getMeshAttachment(index);
    }

    public int GetMeshAttachmentCount() {
        return builder.getMeshAttachmentCount();
    }
}
