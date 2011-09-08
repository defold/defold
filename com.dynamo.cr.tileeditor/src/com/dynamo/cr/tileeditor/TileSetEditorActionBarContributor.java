package com.dynamo.cr.tileeditor;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;



public class TileSetEditorActionBarContributor extends
EditorActionBarContributor {

    public TileSetEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        TileSetEditor tileSetEditor = (TileSetEditor) targetEditor;
        IActionBars actionBars = tileSetEditor.getEditorSite().getActionBars();
        actionBars.updateActionBars();
    }

}
