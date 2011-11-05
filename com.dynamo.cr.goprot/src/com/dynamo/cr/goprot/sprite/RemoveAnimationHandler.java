package com.dynamo.cr.goprot.sprite;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goprot.NodeEditor;

public class RemoveAnimationHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof NodeEditor) {
            ISelection selection = HandlerUtil.getCurrentSelection(event);
            if (selection instanceof IStructuredSelection) {
                // Find selected components
                // TODO: Support multi selection
                IStructuredSelection structuredSelection = (IStructuredSelection)selection;
                Object[] nodes = structuredSelection.toArray();
                AnimationNode animation = null;
                for (Object node : nodes) {
                    if (node instanceof AnimationNode) {
                        animation = (AnimationNode)node;
                        break;
                    }
                }

                NodeEditor nodeEditor = (NodeEditor)editorPart;
                SpritePresenter presenter = (SpritePresenter)nodeEditor.getPresenter(SpriteNode.class);
                presenter.onRemoveAnimation(animation);
            }
        }
        return null;
    }

}
