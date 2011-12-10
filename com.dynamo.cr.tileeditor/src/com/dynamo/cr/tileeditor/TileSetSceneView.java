package com.dynamo.cr.tileeditor;

import javax.inject.Inject;

import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.IImageProvider;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.ISceneOutlinePage;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class TileSetSceneView implements ISceneView {

    @Inject private ISceneOutlinePage outline;
    @Inject private IFormPropertySheetPage propertySheetPage;
    @Inject
    private TileSetRenderer2 renderer;
    @Inject
    private TileSetEditor2 editor;
    @Inject
    IImageProvider imageProvider;

    @Override
    public void setRoot(Node root) {
        TileSetNode tileSet = (TileSetNode) root;
        this.outline.setInput(root);
        this.renderer.setInput(tileSet);
    }

    @Override
    public void refresh(IStructuredSelection selection, boolean dirty) {
        this.outline.refresh();
        this.propertySheetPage.refresh();
        this.renderer.refresh(selection);
        this.outline.setSelection(selection);
        this.editor.setDirty(dirty);
    }

    @Override
    public String selectFromList(String title, String message, String... lst) {
        throw new RuntimeException("Not supported");
    }

    @Override
    public Object selectFromArray(String title, String message, Object[] input, ILabelProvider labelProvider) {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public String selectFile(String title) {
        // TODO Auto-generated method stub
        return null;
    }
}
