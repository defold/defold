package com.dynamo.cr.tileeditor;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

import com.dynamo.cr.editor.ui.AbstractDefoldEditor;



public class DefoldEditorActionBarContributor extends
EditorActionBarContributor {

    public DefoldEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        ((AbstractDefoldEditor)targetEditor).updateActions();
    }

}
