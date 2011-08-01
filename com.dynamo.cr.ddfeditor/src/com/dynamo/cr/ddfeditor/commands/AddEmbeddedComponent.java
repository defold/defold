package com.dynamo.cr.ddfeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.ddfeditor.GameObjectEditor;

public class AddEmbeddedComponent extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        GameObjectEditor editor = (GameObjectEditor) HandlerUtil.getActiveEditor(event);
        editor.onAddEmbeddedComponent();
        return null;
    }

}
