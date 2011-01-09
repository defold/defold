package com.dynamo.cr.ddfeditor;

import com.dynamo.gui.proto.Gui;
import com.google.protobuf.Message;

public class GuiEditor extends DdfEditor {

    public GuiEditor() {
        super(Gui.SceneDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return Gui.SceneDesc.newBuilder()
            .setScript("")
            .build();

    }
}
