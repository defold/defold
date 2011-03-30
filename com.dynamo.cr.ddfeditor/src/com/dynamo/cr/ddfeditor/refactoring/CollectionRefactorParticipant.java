package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;

public class CollectionRefactorParticipant extends GenericRefactorParticipant {

    public CollectionRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return CollectionDesc.newBuilder();
    }

}
