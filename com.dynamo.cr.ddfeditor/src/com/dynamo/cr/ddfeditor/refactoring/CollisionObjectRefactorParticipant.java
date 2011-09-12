package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.google.protobuf.Message.Builder;

public class CollisionObjectRefactorParticipant extends GenericRefactorParticipant {

    public CollisionObjectRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return CollisionObjectDesc.newBuilder();
    }

}
