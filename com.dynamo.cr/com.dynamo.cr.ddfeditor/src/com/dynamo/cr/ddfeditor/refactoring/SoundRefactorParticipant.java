package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.google.protobuf.Message.Builder;

public class SoundRefactorParticipant extends GenericRefactorParticipant {

    public SoundRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SoundDesc.newBuilder();
    }

}
