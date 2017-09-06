package com.dynamo.cr.guied.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.GuiScenePresenter;
import com.dynamo.cr.sceneed.core.ISceneEditor;

public class AddParticleFXHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof ISceneEditor) {
            ISceneEditor sceneEditor = (ISceneEditor) editorPart;
            GuiScenePresenter presenter = (GuiScenePresenter) sceneEditor.getNodePresenter(GuiSceneNode.class);
            presenter.onAddParticleFXNode(sceneEditor.getPresenterContext());
        }
        return null;
    }

}
