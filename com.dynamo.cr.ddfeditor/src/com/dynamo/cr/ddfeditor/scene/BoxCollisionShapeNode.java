package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;

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
            this.width = data[index + 0];
            this.height = data[index + 1];
            this.depth = data[index + 2];
        }
    }

    public double getWidth() {
        return width;
    }

    public void setWidth(double width) {
        clearBoundsStatusError();
        this.width = width;
    }

    public double getHeight() {
        return height;
    }

    public void setHeight(double height) {
        clearBoundsStatusError();
        this.height = height;
    }

    public double getDepth() {
        return depth;
    }

    public void setDepth(double depth) {
        clearBoundsStatusError();
        this.depth = depth;
    }

    @Override
    public String toString() {
        return "Box Shape";
    }


}
