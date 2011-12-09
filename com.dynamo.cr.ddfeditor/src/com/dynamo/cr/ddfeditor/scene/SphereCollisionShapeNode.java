package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;

public class SphereCollisionShapeNode extends CollisionShapeNode {

    @Property
    @GreaterThanZero
    private double radius;

    public SphereCollisionShapeNode(Vector4d position, Quat4d rotation, float[] data, int index) {
        super(position, rotation);
        if (data.length - index < 1) {
            createBoundsStatusError();
        } else {
            this.radius = data[0];
        }
    }

    public double getRadius() {
        return this.radius;
    }

    public void setRadius(double radius) {
        clearBoundsStatusError();
        this.radius = radius;
    }

    @Override
    public String toString() {
        return "Sphere Shape";
    }

}
