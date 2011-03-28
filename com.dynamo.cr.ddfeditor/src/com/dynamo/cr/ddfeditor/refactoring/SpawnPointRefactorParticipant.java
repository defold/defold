package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.google.protobuf.Message.Builder;

public class SpawnPointRefactorParticipant extends GenericRefactorParticipant {

    public SpawnPointRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SpawnPointDesc.newBuilder();
    }

}
