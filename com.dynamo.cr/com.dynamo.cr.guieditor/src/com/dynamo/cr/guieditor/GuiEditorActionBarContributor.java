package com.dynamo.cr.guieditor;

import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

public class GuiEditorActionBarContributor extends EditorActionBarContributor {

    public GuiEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        GuiEditor guiEditor = (GuiEditor) targetEditor;
        guiEditor.updateActions();
    }

}
