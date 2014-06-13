package com.dynamo.cr.spine.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.google.protobuf.Message.Builder;

public class SpineModelRefactorParticipant extends GenericRefactorParticipant {

    public SpineModelRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SpineModelDesc.newBuilder();
    }
}
