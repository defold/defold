package com.dynamo.cr.tileeditor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenter;
import com.dynamo.cr.tileeditor.AtlasEditor;
import com.dynamo.cr.tileeditor.AtlasEditorPresenter;
import com.dynamo.cr.tileeditor.scene.AtlasAnimationNode;

public class ToggleAtlasPlaybackHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof AtlasEditor) {
            AtlasEditor atlasEditor = (AtlasEditor)editorPart;
            AtlasEditorPresenter editorPresnter = (AtlasEditorPresenter) atlasEditor.getScenePresenter();
            IStructuredSelection selection = atlasEditor.getPresenterContext().getSelection();
            Object selected = selection.getFirstElement();
            AtlasAnimationNode animationNode = null;
            IPresenter scenePresenter = atlasEditor.getScenePresenter();
            if (selected instanceof AtlasAnimationNode && !scenePresenter.isSimulating()) {
                animationNode = (AtlasAnimationNode)selected;
            }
            editorPresnter.setPlayAnimationNode(animationNode);
            scenePresenter.toogleSimulation();
        }
        return null;
    }

}
