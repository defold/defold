package com.dynamo.cr.ddfeditor.refactoring;

import com.dynamo.gui.proto.Gui.SceneDesc;

public class GuiRefactorParticipant extends GenericRefactorParticipant {

    public GuiRefactorParticipant() {
    }

    @Override
    public com.google.protobuf.Message.Builder getBuilder() {
        return SceneDesc.newBuilder();
    }

}
