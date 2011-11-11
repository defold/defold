package com.dynamo.cr.sceneed.gameobject;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.sceneed.NodeEditor;

public class AddComponentHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof NodeEditor) {
            // TODO: Retrieve component type
            String componentType = "sprite";

            NodeEditor nodeEditor = (NodeEditor)editorPart;
            GameObjectPresenter presenter = (GameObjectPresenter)nodeEditor.getPresenter(GameObjectNode.class);
            presenter.onAddComponent(componentType);
        }
        return null;
    }

}
