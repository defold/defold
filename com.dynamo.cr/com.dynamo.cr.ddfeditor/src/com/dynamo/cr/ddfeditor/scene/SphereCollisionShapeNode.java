package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;

@SuppressWarnings("serial")
public class SphereCollisionShapeNode extends CollisionShapeNode {

    @Property
    @GreaterThanZero
    private double diameter;

    public SphereCollisionShapeNode(Vector4d position, Quat4d rotation, float[] data, int index) {
        super(position, rotation);
        if (data.length - index < 1) {
            createBoundsStatusError();
        } else {
            setDiameter(2.0f * data[index]);
        }
    }

    private final void updateAABB() {
        AABB aabb = new AABB();
        double radius = 0.5f * this.diameter;
        aabb.union(-radius, -radius, -radius);
        aabb.union(radius, radius, radius);
        setAABB(aabb);
    }

    public double getDiameter() {
        return this.diameter;
    }

    public void setDiameter(double diameter) {
        clearBoundsStatusError();
        this.diameter = diameter;
        updateAABB();
    }

    @Override
    public String toString() {
        return "Sphere Shape";
    }

}
