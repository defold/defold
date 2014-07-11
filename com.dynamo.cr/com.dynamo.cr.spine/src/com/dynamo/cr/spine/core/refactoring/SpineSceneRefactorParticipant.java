package com.dynamo.cr.spine.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.google.protobuf.Message.Builder;

public class SpineSceneRefactorParticipant extends GenericRefactorParticipant {

    public SpineSceneRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SpineSceneDesc.newBuilder();
    }
}
