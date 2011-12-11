package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;

public class SphereCollisionShapeNode extends CollisionShapeNode {

    @Property
    @GreaterThanZero
    private double radius;

    public SphereCollisionShapeNode(Vector4d position, Quat4d rotation, float[] data, int index) {
        super(position, rotation);
        if (data.length - index < 1) {
            createBoundsStatusError();
        } else {
            setRadius(data[0]);
        }
    }

    private final void updateAABB() {
        AABB aabb = new AABB();
        aabb.union(-radius, -radius, -radius);
        aabb.union(radius, radius, radius);
        setAABB(aabb);
    }

    public double getRadius() {
        return this.radius;
    }

    public void setRadius(double radius) {
        clearBoundsStatusError();
        this.radius = radius;
        updateAABB();
    }

    @Override
    public String toString() {
        return "Sphere Shape";
    }

}
