package com.dynamo.cr.sceneed.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.sceneed.ui.SceneEditor;

public class RecordHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);

        if (editor instanceof SceneEditor) {
            ((SceneEditor) editor).toggleRecord();
        }
        return null;
    }

    @Override
    public boolean isEnabled() {
        return EditorUtil.isDev();
    }

}
