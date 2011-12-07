package com.dynamo.cr.ddfeditor.scene;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.physics.proto.Physics.CollisionShape;
import com.dynamo.physics.proto.Physics.CollisionShape.Builder;
import com.dynamo.physics.proto.Physics.CollisionShape.Shape;

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
    protected CollisionShape.Shape.Builder buildShape(
            Builder collisionShapeBuilder) {
        Shape.Builder b = Shape.newBuilder();
        b.setShapeType(CollisionShape.Type.TYPE_SPHERE);
        b.setCount(1);
        b.setIndex(collisionShapeBuilder.getDataCount());
        collisionShapeBuilder.addData((float) radius);
        return b;
    }

    @Override
    public String toString() {
        return "Sphere Shape";
    }

}
