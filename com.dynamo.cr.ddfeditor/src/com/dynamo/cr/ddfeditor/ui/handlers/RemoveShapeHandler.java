package com.dynamo.cr.ddfeditor.ui.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectPresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class RemoveShapeHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor)editorPart;
            CollisionObjectPresenter presenter = (CollisionObjectPresenter)sceneEditor.getNodePresenter(CollisionObjectNode.class);
            presenter.onRemoveShape(sceneEditor.getPresenterContext(), sceneEditor.getLoaderContext());
        }
        return null;
    }

}
