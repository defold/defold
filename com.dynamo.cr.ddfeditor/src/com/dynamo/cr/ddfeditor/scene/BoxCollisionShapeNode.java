package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.AABB;

public class BoxCollisionShapeNode extends CollisionShapeNode {

    @Property
    @GreaterThanZero
    private double width;
    @Property
    @GreaterThanZero
    private double height;
    @Property
    @GreaterThanZero
    private double depth;

    public BoxCollisionShapeNode(Vector4d position, Quat4d rotation, float[] data, int index) {
        super(position, rotation);
        if (data.length - index < 3) {
            createBoundsStatusError();
        } else {
            setWidth(data[index + 0]);
            setHeight(data[index + 1]);
            setDepth(data[index + 2]);
        }
    }

    public double getWidth() {
        return width;
    }

    private final void updateAABB() {
        AABB aabb = new AABB();
        aabb.union(-width, -height, -depth);
        aabb.union(width, height, depth);
        setAABB(aabb);
    }

    public void setWidth(double width) {
        clearBoundsStatusError();
        this.width = width;
        updateAABB();
    }

    public double getHeight() {
        return height;
    }

    public void setHeight(double height) {
        clearBoundsStatusError();
        this.height = height;
        updateAABB();
    }

    public double getDepth() {
        return depth;
    }

    public void setDepth(double depth) {
        clearBoundsStatusError();
        this.depth = depth;
        updateAABB();
    }

    @Override
    public String toString() {
        return "Box Shape";
    }


}
