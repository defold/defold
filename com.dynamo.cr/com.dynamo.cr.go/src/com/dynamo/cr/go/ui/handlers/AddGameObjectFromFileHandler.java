package com.dynamo.cr.go.ui.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.CollectionPresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class AddGameObjectFromFileHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor)editorPart;
            CollectionPresenter presenter = (CollectionPresenter)sceneEditor.getNodePresenter(CollectionNode.class);
            presenter.onAddGameObjectFromFile(sceneEditor.getPresenterContext(), sceneEditor.getLoaderContext());
        }
        return null;
    }

}
