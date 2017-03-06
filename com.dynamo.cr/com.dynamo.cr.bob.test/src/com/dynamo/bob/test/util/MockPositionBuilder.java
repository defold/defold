package com.dynamo.bob.test.util;

import javax.vecmath.Point3d;

import com.dynamo.bob.util.RigUtil.PositionBuilder;
import com.dynamo.rig.proto.Rig.AnimationTrack.Builder;

public class MockPositionBuilder extends PositionBuilder {

    public MockPositionBuilder(Builder builder) {
        super(builder);
    }

    public int GetPositionsCount() {
        return builder.getPositionsCount();
    }
    
    public javax.vecmath.Point3d GetPositions(int index) {
        Point3d pos = new Point3d(
                builder.getPositions(index),
                builder.getPositions(index+1),
                builder.getPositions(index+2));
        return pos;
    }
}
