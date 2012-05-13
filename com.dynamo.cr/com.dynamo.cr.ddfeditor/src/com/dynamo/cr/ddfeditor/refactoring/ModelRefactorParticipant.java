package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.model.proto.Model.ModelDesc;
import com.google.protobuf.Message.Builder;

public class ModelRefactorParticipant extends GenericRefactorParticipant {

    public ModelRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return ModelDesc.newBuilder();
    }

}
