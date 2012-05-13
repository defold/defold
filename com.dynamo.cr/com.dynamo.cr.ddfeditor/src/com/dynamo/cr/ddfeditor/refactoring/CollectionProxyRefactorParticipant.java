package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.google.protobuf.Message.Builder;

public class CollectionProxyRefactorParticipant extends GenericRefactorParticipant {

    public CollectionProxyRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return CollectionProxyDesc.newBuilder();
    }

}
