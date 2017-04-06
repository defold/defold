package com.dynamo.bob.test.util;

import com.dynamo.bob.util.RigUtil.DrawOrderBuilder;
import com.dynamo.rig.proto.Rig.MeshAnimationTrack.Builder;

public class MockDrawOrderBuilder extends DrawOrderBuilder {

    public MockDrawOrderBuilder(Builder builder) {
        super(builder);
        // TODO Auto-generated constructor stub
    }

    int GetOrderOffset(int index) {
        return builder.getOrderOffset(index);
    }
    
    int GetOrderOffsetCount()
    {
        return builder.getOrderOffsetCount();
    }
}
