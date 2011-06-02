package com.dynamo.cr.ddfeditor;

import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

public class GameObjectEditorActionBarContributor extends EditorActionBarContributor {

    public GameObjectEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        GameObjectEditor gameObjectEditor = (GameObjectEditor) targetEditor;
        gameObjectEditor.updateActions();
    }

}
