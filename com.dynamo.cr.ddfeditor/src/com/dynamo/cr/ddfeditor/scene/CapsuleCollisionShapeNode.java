package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;

public class CapsuleCollisionShapeNode extends CollisionShapeNode {

    @Property
    @GreaterThanZero
    private double radius;
    @Property
    @GreaterThanZero
    private double height;

    public CapsuleCollisionShapeNode(Vector4d position, Quat4d rotation, float[] data, int index) {
        super(position, rotation);
        if (data.length - index < 2) {
            createBoundsStatusError();
        } else {
            this.radius = data[index + 0];
            this.height = data[index + 1];
        }
    }

    public double getRadius() {
        return radius;
    }

    public void setRadius(double radius) {
        this.radius = radius;
    }

    public double getHeight() {
        return height;
    }

    public void setHeight(double height) {
        this.height = height;
    }

    @Override
    public String toString() {
        return "Capsule Shape";
    }

}
