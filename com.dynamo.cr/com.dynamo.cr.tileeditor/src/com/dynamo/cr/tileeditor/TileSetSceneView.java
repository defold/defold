package com.dynamo.cr.tileeditor;

import javax.inject.Inject;
import javax.vecmath.Point3d;

import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IWorkbenchPart;

import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.AbstractSceneView;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.ISceneOutlinePage;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class TileSetSceneView extends AbstractSceneView {

    @Inject private ISceneOutlinePage outline;
    @Inject private IFormPropertySheetPage propertySheetPage;
    @Inject private TileSetRenderer2 renderer;
    @Inject private TileSetEditor2 editor;
    @Inject private ManipulatorController manipulatorController;

    @Override
    public void setRoot(Node root) {
        TileSetNode tileSet = (TileSetNode) root;
        this.outline.setInput(root);
        this.renderer.setInput(tileSet);
    }

    @Override
    public void refreshRenderView() {
        this.renderer.refresh();
    }

    @Override
    public void refresh(IStructuredSelection selection, boolean dirty) {
        this.outline.refresh();
        this.outline.setSelection(selection);
        this.propertySheetPage.setSelection(selection);
        this.propertySheetPage.refresh();
        this.manipulatorController.setSelection(selection);
        this.renderer.setSelection(selection);
        this.renderer.refresh();
        this.editor.setDirty(dirty);
    }

    @Override
    public void setSimulating(boolean simulating) {
        // Not supported
    }

    @Override
    public void asyncExec(Runnable runnable) {
        // TODO This method is only called by the Animator to animate tile source animations
        // The timer exec below ensures a bounded execution rate, since in this specific case,
        // this method is only called by the runnable passed in. Fragile but it works for now.
        // Should be removed and changed to how atlases are animated once the tile source editor is ported to sceneed
        // https://defold.fogbugz.com/default.asp?439
        Display.getCurrent().timerExec(15, runnable);
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
    public String selectFile(String title, String[] extensions) {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public String[] selectFiles(String title, String[] extensions) {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void getCameraFocusPoint(Point3d focusPoint) {
        throw new RuntimeException("Not implemented");
    }

    @Override
    public void frameSelection() {
        this.renderer.frameTileSet();
    }

    @Override
    protected IWorkbenchPart getPart() {
        return this.editor;
    }
}
