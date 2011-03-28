package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.render.proto.Font.FontDesc;
import com.google.protobuf.Message.Builder;

public class FontRefactorParticipant extends GenericRefactorParticipant {

    public FontRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return FontDesc.newBuilder();
    }

}
