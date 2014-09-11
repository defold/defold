package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.graphics.proto.Graphics.Cubemap;
import com.google.protobuf.Message.Builder;

public class CubemapRefactorParticipant extends GenericRefactorParticipant {

    public CubemapRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return Cubemap.newBuilder();
    }

}
