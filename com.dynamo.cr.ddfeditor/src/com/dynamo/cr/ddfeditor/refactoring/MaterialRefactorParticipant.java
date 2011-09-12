package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.Message.Builder;

public class MaterialRefactorParticipant extends GenericRefactorParticipant {

    public MaterialRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return MaterialDesc.newBuilder();
    }

}
