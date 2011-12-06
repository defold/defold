package com.dynamo.cr.go.ui.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class AddComponentFromFileHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor)editorPart;
            GameObjectPresenter presenter = (GameObjectPresenter)sceneEditor.getNodePresenter(GameObjectNode.class);
            presenter.onAddComponentFromFile(sceneEditor.getPresenterContext(), sceneEditor.getLoaderContext());
        }
        return null;
    }

}
