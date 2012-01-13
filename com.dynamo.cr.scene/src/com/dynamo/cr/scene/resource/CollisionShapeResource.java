package com.dynamo.cr.scene.resource;

import com.dynamo.physics.proto.Physics.CollisionShape;

public class CollisionShapeResource extends Resource {

    private CollisionShape collisionShape;

    public CollisionShapeResource(String path, CollisionShape collisionShape) {
        super(path);
        this.collisionShape = collisionShape;
    }

    public CollisionShape getConvexShape() {
        return collisionShape;
    }

    public void setCollisionShape(CollisionShape collisionShape) {
        this.collisionShape = collisionShape;
    }

}
