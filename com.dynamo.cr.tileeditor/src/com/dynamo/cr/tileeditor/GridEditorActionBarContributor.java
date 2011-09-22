package com.dynamo.cr.tileeditor;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.part.EditorActionBarContributor;



public class GridEditorActionBarContributor extends
EditorActionBarContributor {

    public GridEditorActionBarContributor() {
    }

    @Override
    public void setActiveEditor(IEditorPart targetEditor) {
        super.setActiveEditor(targetEditor);
        GridEditor gridEditor = (GridEditor) targetEditor;
        IActionBars actionBars = gridEditor.getEditorSite().getActionBars();
        actionBars.updateActionBars();
    }

}
