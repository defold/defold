package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.Message.Builder;

public class SpriteRefactorParticipant extends GenericRefactorParticipant {

    public SpriteRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SpriteDesc.newBuilder();
    }
}
