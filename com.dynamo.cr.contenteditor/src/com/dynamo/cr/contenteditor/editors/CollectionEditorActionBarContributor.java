package com.dynamo.cr.contenteditor.editors;

import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;

public class CollectionEditorActionBarContributor extends
        EditorActionBarContributor {

    public CollectionEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        CollectionEditor collectionEditor = (CollectionEditor) targetEditor;
        collectionEditor.updateActions();
    }

}
