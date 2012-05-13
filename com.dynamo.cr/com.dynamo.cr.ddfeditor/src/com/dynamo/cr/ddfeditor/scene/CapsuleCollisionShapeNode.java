package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;

@SuppressWarnings("serial")
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
            setRadius(data[index + 0]);
            setHeight(data[index + 1]);
        }
        updateAABB();
    }

    private final void updateAABB() {
        AABB aabb = new AABB();
        aabb.union(-radius, -height, -radius);
        aabb.union(radius, height, radius);
        setAABB(aabb);
    }

    public double getRadius() {
        return radius;
    }

    public void setRadius(double radius) {
        this.radius = radius;
        updateAABB();
    }

    public double getHeight() {
        return height;
    }

    public void setHeight(double height) {
        this.height = height;
        updateAABB();
    }

    @Override
    public String toString() {
        return "Capsule Shape";
    }

}
