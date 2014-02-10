package com.dynamo.cr.guied.refactoring;

import com.dynamo.cr.editor.core.GenericRefactorParticipant;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.Builder;

public class GuiSceneRefactorParticipant extends GenericRefactorParticipant {

    public GuiSceneRefactorParticipant() {
    }

    @Override
    public Builder getBuilder() {
        return SceneDesc.newBuilder();
    }

}
