package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.gamesystem.proto.GameSystem.FactoryDesc;
import com.google.protobuf.Message.Builder;

public class FactoryRefactorParticipant extends GenericRefactorParticipant {

    public FactoryRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return FactoryDesc.newBuilder();
    }

}
