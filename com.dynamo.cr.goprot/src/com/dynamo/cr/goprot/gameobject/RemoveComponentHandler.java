package com.dynamo.cr.goprot.gameobject;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goprot.NodeEditor;

public class RemoveComponentHandler extends AbstractHandler {

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
                ComponentNode component = null;
                for (Object node : nodes) {
                    if (node instanceof ComponentNode) {
                        component = (ComponentNode)node;
                        break;
                    }
                }

                NodeEditor nodeEditor = (NodeEditor)editorPart;
                GameObjectPresenter presenter = (GameObjectPresenter)nodeEditor.getPresenter(GameObjectNode.class);
                presenter.onRemoveComponent(component);
            }
        }
        return null;
    }

}
