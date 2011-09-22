package com.dynamo.cr.tileeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.GridEditor;

public class ShowPalette extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof GridEditor) {
            boolean oldValue = HandlerUtil.toggleCommandState(event.getCommand());
            GridEditor gridEditor = (GridEditor)editorPart;
            gridEditor.showPalette(!oldValue);
        }
        return null;
    }

}
