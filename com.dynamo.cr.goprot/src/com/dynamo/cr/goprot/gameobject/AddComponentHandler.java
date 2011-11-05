package com.dynamo.cr.goprot.gameobject;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goprot.NodeEditor;

public class AddComponentHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof NodeEditor) {
            ISelection selection = HandlerUtil.getCurrentSelection(event);
            if (selection instanceof IStructuredSelection) {
                // TODO: Retrieve component type
                String componentType = "sprite";

                // Find selected game objects
                // TODO: Support multi selection
                IStructuredSelection structuredSelection = (IStructuredSelection)selection;
                Object[] nodes = structuredSelection.toArray();
                GameObjectNode gameObject = null;
                for (Object node : nodes) {
                    if (node instanceof GameObjectNode) {
                        gameObject = (GameObjectNode)node;
                        break;
                    }
                }

                NodeEditor nodeEditor = (NodeEditor)editorPart;
                GameObjectPresenter presenter = (GameObjectPresenter)nodeEditor.getPresenter(GameObjectNode.class);
                presenter.onAddComponent(gameObject, componentType);
            }
        }
        return null;
    }

}
