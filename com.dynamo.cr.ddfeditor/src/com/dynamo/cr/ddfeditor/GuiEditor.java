package com.dynamo.cr.ddfeditor;

import com.dynamo.gui.proto.Gui;

public class GuiEditor extends DdfEditor {

    public GuiEditor() {
        super(Gui.SceneDesc.newBuilder().buildPartial());
    }

}
