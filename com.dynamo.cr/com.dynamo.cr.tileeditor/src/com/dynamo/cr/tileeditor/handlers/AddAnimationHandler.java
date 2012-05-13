package com.dynamo.cr.tileeditor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor2;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;

public class AddAnimationHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof TileSetEditor2) {
            TileSetEditor2 tileSetEditor = (TileSetEditor2)editorPart;
            TileSetNodePresenter presenter = (TileSetNodePresenter)tileSetEditor.getNodePresenter(TileSetNode.class);
            presenter.onAddAnimation(tileSetEditor.getPresenterContext());
        }
        return null;
    }

}
