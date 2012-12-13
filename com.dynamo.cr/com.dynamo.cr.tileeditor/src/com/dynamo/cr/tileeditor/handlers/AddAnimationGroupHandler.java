package com.dynamo.cr.tileeditor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.AtlasEditor;
import com.dynamo.cr.tileeditor.scene.AtlasNode;
import com.dynamo.cr.tileeditor.scene.AtlasNodePresenter;

public class AddAnimationGroupHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof AtlasEditor) {
            AtlasEditor atlasEditor = (AtlasEditor)editorPart;
            AtlasNodePresenter presenter = (AtlasNodePresenter)atlasEditor.getNodePresenter(AtlasNode.class);
            presenter.onAddAnimationGroup(atlasEditor.getPresenterContext());
        }
        return null;
    }

}
