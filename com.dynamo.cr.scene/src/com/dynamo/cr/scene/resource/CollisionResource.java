package com.dynamo.cr.scene.resource;

import com.dynamo.physics.proto.Physics.CollisionObjectDesc;

public class CollisionResource extends Resource {

    private CollisionObjectDesc collisionObjectDesc;
    private Resource collisionShapeResource;

    public CollisionResource(String path, CollisionObjectDesc collisionObjectDesc, Resource collisionShapeResource) {
        super(path);
        this.collisionObjectDesc = collisionObjectDesc;
        this.collisionShapeResource = collisionShapeResource;
    }

    public CollisionObjectDesc getCollisionDesc() {
        return collisionObjectDesc;
    }

    public void setCollisionDesc(CollisionObjectDesc collisionObjectDesc) {
        this.collisionObjectDesc = collisionObjectDesc;
    }

    public Resource getCollisionShapeResource() {
        return collisionShapeResource;
    }

}
