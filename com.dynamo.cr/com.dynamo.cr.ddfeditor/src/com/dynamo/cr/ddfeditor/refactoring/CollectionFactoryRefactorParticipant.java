package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.gamesystem.proto.GameSystem.CollectionFactoryDesc;
import com.google.protobuf.Message.Builder;

public class CollectionFactoryRefactorParticipant extends GenericRefactorParticipant {

    public CollectionFactoryRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return CollectionFactoryDesc.newBuilder();
    }
}
