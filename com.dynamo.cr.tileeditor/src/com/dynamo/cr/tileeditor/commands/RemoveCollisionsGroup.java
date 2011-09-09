package com.dynamo.cr.tileeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;

public class RemoveCollisionsGroup extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof TileSetEditor) {
            TileSetEditor tileSetEditor = (TileSetEditor)editorPart;
            if (tileSetEditor.getSelectedCollisionGroups().length > 0) {
                tileSetEditor.getPresenter().removeSelectedCollisionGroups();
            }
        }
        return null;
    }

}
