package com.dynamo.cr.ddfeditor;

import com.dynamo.physics.proto.Physics.CollisionObjectDesc;

public class CollisionObjectEditor extends DdfEditor {

    public CollisionObjectEditor() {
        super(CollisionObjectDesc.newBuilder().buildPartial());
    }

    @Override
    public String getTitle() {
        return "Collision Object";
    }

}
