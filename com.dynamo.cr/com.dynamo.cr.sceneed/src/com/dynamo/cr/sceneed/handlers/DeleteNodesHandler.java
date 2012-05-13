package com.dynamo.cr.sceneed.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.core.ISceneEditor;

public class DeleteNodesHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor)editorPart;
            sceneEditor.getScenePresenter().onDeleteSelection(sceneEditor.getPresenterContext());
        }
        return null;
    }

}
