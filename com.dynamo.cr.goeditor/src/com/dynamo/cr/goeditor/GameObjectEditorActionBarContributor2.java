package com.dynamo.cr.goeditor;

import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

public class GameObjectEditorActionBarContributor2 extends
        EditorActionBarContributor {

    public GameObjectEditorActionBarContributor2() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        GameObjectEditor2 gameObjectEditor = (GameObjectEditor2) targetEditor;
        gameObjectEditor.updateActions();
    }

}
