package com.dynamo.cr.tileeditor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.GridEditor;
import com.dynamo.cr.tileeditor.core.IGridView;

public class SelectEraserHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof GridEditor) {
            GridEditor gridEditor = (GridEditor) editorPart;
            IGridView.Presenter presenter = gridEditor.getPresenter();
            presenter.onSelectTile(-1, false, false);
        }
        return null;
    }

}
