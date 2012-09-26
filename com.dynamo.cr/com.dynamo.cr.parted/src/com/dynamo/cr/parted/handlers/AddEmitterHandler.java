package com.dynamo.cr.parted.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.parted.ParticleFXNode;
import com.dynamo.cr.parted.ParticleFXPresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class AddEmitterHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor)editorPart;
            ParticleFXPresenter presenter = (ParticleFXPresenter)sceneEditor.getNodePresenter(ParticleFXNode.class);
            presenter.onAddEmitter(sceneEditor.getPresenterContext(), sceneEditor.getLoaderContext());
        }
        return null;
    }

}
