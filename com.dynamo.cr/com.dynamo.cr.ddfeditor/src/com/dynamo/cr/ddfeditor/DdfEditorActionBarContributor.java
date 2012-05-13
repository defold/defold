package com.dynamo.cr.ddfeditor;

import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

public class DdfEditorActionBarContributor extends EditorActionBarContributor {

    public DdfEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        DdfEditor ddfEditor = (DdfEditor) targetEditor;
        ddfEditor.updateActions();
    }

}
