package com.dynamo.bob.test.util;

import javax.vecmath.Quat4d;

import org.openmali.vecmath2.Point3d;

import com.dynamo.bob.util.RigUtil.RotationBuilder;
import com.dynamo.rig.proto.Rig.AnimationTrack.Builder;

public class MockRotationBuilder extends RotationBuilder {

    public MockRotationBuilder(Builder builder) {
        super(builder);
    }

    public int GetRotationsCount() {
        return builder.getRotationsCount();
    }
    
    public Quat4d GetRotations(int index) {
        Quat4d rot = new Quat4d(
                builder.getRotations(index),
                builder.getRotations(index+1),
                builder.getRotations(index+2),
                builder.getRotations(index+3));
        return rot;
    }
}
