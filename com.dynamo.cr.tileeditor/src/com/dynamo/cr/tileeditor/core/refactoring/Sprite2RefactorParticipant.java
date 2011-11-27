package com.dynamo.cr.tileeditor.core.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;
import com.google.protobuf.Message.Builder;

public class Sprite2RefactorParticipant extends GenericRefactorParticipant {

    public Sprite2RefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return Sprite2Desc.newBuilder();
    }
}
