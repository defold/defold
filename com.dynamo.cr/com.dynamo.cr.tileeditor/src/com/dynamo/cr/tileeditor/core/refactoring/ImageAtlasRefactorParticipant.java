package com.dynamo.cr.tileeditor.core.refactoring;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.google.protobuf.Message.Builder;

public class ImageAtlasRefactorParticipant extends GenericRefactorParticipant {
    @Override
    public Builder getBuilder() {
        return Atlas.newBuilder();
    }
}
