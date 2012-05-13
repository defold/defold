package com.dynamo.cr.tileeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;

public class RemoveCollisionsGroup extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        ISelection selection = HandlerUtil.getCurrentSelection(event);
        if (editorPart instanceof TileSetEditor && !selection.isEmpty()) {
            TileSetEditor tileSetEditor = (TileSetEditor)editorPart;
            tileSetEditor.getPresenter().removeSelectedCollisionGroups();
        }
        return null;
    }

}
