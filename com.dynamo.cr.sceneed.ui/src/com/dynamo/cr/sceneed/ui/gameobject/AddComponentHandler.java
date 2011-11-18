package com.dynamo.cr.sceneed.ui.gameobject;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.ui.SceneEditor;

public class AddComponentHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof SceneEditor) {
            SceneEditor sceneEditor = (SceneEditor)editorPart;
            GameObjectPresenter presenter = (GameObjectPresenter)sceneEditor.getNodePresenter(GameObjectNode.class);
            presenter.onAddComponent(sceneEditor.getContext());
        }
        return null;
    }

}
