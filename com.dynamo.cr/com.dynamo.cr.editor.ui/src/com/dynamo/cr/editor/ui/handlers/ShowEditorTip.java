package com.dynamo.cr.editor.ui.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

import com.dynamo.cr.editor.ui.EditorUIPlugin;
import com.dynamo.cr.editor.ui.tip.ITipManager;

public class ShowEditorTip extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        ITipManager tipManager = EditorUIPlugin.getDefault().getInjector().getInstance(ITipManager.class);
        tipManager.showTip(true);
        return null;
    }

}
