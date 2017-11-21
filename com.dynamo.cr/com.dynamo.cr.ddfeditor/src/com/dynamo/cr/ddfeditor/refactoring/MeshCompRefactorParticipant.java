package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.mesh.proto.MeshProto.MeshDesc;
import com.google.protobuf.Message.Builder;

public class MeshCompRefactorParticipant extends GenericRefactorParticipant {

    public MeshCompRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return MeshDesc.newBuilder();
    }

}
