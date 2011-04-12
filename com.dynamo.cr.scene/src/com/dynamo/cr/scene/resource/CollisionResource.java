package com.dynamo.cr.scene.resource;

import com.dynamo.physics.proto.Physics.CollisionObjectDesc;

public class CollisionResource extends Resource {

    private CollisionObjectDesc collisionObjectDesc;
    private ConvexShapeResource convexShapeResource;

    public CollisionResource(String path, CollisionObjectDesc collisionObjectDesc, ConvexShapeResource convexShapeResource) {
        super(path);
        this.collisionObjectDesc = collisionObjectDesc;
        this.convexShapeResource = convexShapeResource;
    }

    public CollisionObjectDesc getCollisionDesc() {
        return collisionObjectDesc;
    }

    public void setCollisionDesc(CollisionObjectDesc collisionObjectDesc) {
        this.collisionObjectDesc = collisionObjectDesc;
    }

    public ConvexShapeResource getConvexShapeResource() {
        return convexShapeResource;
    }

}
