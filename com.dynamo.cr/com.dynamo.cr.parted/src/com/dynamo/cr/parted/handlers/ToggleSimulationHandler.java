package com.dynamo.cr.parted.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.parted.ParticleFXEditor;

public class ToggleSimulationHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ParticleFXEditor) {
            ParticleFXEditor editor = (ParticleFXEditor)editorPart;
            editor.getScenePresenter().toogleSimulation();
        }
        return null;
    }

}
