package com.dynamo.bob.test.util;

import com.dynamo.bob.util.RigUtil.VisibilityBuilder;

public class MockVisibilityBuilder extends VisibilityBuilder {

    public MockVisibilityBuilder(com.dynamo.rig.proto.Rig.MeshAnimationTrack.Builder builder, String meshName) {
        super(builder, meshName);
    }
    
    public boolean GetVisible(int index) {
        return builder.getVisible(index);
    }

    public int GetVisibleCount() {
        return builder.getVisibleCount();
    }
}
