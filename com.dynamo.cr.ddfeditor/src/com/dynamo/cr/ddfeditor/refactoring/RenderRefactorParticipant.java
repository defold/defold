package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.google.protobuf.Message.Builder;

public class RenderRefactorParticipant extends GenericRefactorParticipant {

    public RenderRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return RenderPrototypeDesc.newBuilder();
    }

}
