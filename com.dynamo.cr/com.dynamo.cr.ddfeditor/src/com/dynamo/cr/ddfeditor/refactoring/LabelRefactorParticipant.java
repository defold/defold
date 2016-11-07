package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.label.proto.Label.LabelDesc;
import com.google.protobuf.Message.Builder;

public class LabelRefactorParticipant extends GenericRefactorParticipant {

    public LabelRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return LabelDesc.newBuilder();
    }

}
