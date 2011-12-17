package com.dynamo.cr.tileeditor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.tileeditor.TileSetEditor2;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;

public class TogglePlaybackHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof TileSetEditor2) {
            TileSetEditor2 tileSetEditor = (TileSetEditor2)editorPart;
            TileSetNodePresenter presenter = (TileSetNodePresenter)tileSetEditor.getNodePresenter(TileSetNode.class);
            IStructuredSelection selection = tileSetEditor.getPresenterContext().getSelection();
            Object selected = selection.getFirstElement();
            if (selected instanceof AnimationNode) {
                AnimationNode node = (AnimationNode)selected;
                // TODO do this inside the presenter
                if (!node.isPlaying()) {
                    presenter.onPlayAnimation(tileSetEditor.getPresenterContext());
                } else {
                    presenter.onStopAnimation(tileSetEditor.getPresenterContext());
                }
            }
        }
        return null;
    }

}
