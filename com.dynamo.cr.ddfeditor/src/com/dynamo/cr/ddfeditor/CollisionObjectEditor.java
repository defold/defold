package com.dynamo.cr.ddfeditor;

import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectType;
import com.google.protobuf.Message;

public class CollisionObjectEditor extends DdfEditor {

    public CollisionObjectEditor() {
        super(CollisionObjectDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return CollisionObjectDesc.newBuilder()
            .setCollisionShape("")
            .setType(CollisionObjectType.COLLISION_OBJECT_TYPE_DYNAMIC)
            .setMass(1)
            .setFriction(0.1f)
            .setRestitution(0.5f)
            .setGroup(1)
            .build();
    }
}
