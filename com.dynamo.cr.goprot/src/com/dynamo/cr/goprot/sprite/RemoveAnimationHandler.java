package com.dynamo.cr.goprot.sprite;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.goprot.NodeEditor;

public class RemoveAnimationHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof NodeEditor) {
            NodeEditor nodeEditor = (NodeEditor)editorPart;
            SpritePresenter presenter = (SpritePresenter)nodeEditor.getPresenter(SpriteNode.class);
            presenter.onRemoveAnimation();
        }
        return null;
    }

}
