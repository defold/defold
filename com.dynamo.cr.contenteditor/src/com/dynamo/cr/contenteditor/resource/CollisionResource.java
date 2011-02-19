package com.dynamo.cr.contenteditor.resource;

import com.dynamo.physics.proto.Physics.CollisionObjectDesc;

public class CollisionResource {

    private CollisionObjectDesc collisionObjectDesc;

    public CollisionResource(CollisionObjectDesc collisionObjectDesc) {
        this.collisionObjectDesc = collisionObjectDesc;
    }

    public CollisionObjectDesc getCollisionDesc() {
        return collisionObjectDesc;
    }

}
