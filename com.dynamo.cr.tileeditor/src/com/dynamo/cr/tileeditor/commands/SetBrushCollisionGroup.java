package com.dynamo.cr.tileeditor.commands;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor;


public class SetBrushCollisionGroup extends AbstractHandler {

    private static final String PARAM_INDEX = "index";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart activeEditor = HandlerUtil.getActiveEditor(event);
        if (activeEditor instanceof TileSetEditor) {
            TileSetEditor editor = (TileSetEditor)activeEditor;
            String indexParameter = event.getParameter(PARAM_INDEX);
            int index = Integer.parseInt(indexParameter);
            editor.setBrushCollisionGroup(index);
        }
        return null;
    }

}
