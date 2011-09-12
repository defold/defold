package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

public class DisabledNewEditorHandler extends AbstractHandler {

    @Override
    public boolean isEnabled() {
        return false;
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        return null;
    }

}
