package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.google.protobuf.Message.Builder;

public class AnimationSetRefactorParticipant extends GenericRefactorParticipant {

    public AnimationSetRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return AnimationSetDesc.newBuilder();
    }

}
