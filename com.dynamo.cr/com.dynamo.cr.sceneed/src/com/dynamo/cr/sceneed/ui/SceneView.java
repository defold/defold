package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.dialogs.ListDialog;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.FilteredResourceListSelectionDialog;
import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.AbstractSceneView;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.Node;

public class SceneView extends AbstractSceneView {

    @Inject private ISceneOutlinePage outline;
    @Inject private IFormPropertySheetPage propertySheetPage;
    @Inject private IRenderView renderView;
    @Inject private SceneEditor editor;
    @Inject private IContainer contentRoot;

    @Override
    public void setRoot(Node root) {
        this.outline.setInput(root);
        this.renderView.setInput(root);
    }

    @Override
    public void refreshRenderView() {
        this.renderView.refresh();
    }

    @Override
    public void refresh(IStructuredSelection selection, boolean dirty) {
        this.renderView.setSelection(selection);
        this.renderView.refresh();
        editor.setDirty(dirty);
        setIgnoreOutlineSelection(true);
        outline.refresh();
        outline.setSelection(selection);
        setIgnoreOutlineSelection(false);
        propertySheetPage.setSelection(selection);
        propertySheetPage.refresh();
    }

    @Override
    public void setSimulating(boolean simulating) {
        this.renderView.setSimulating(simulating);
    }

    @Override
    public void asyncExec(Runnable runnable) {
        Display.getCurrent().asyncExec(runnable);
    }

    @Override
    public String selectFromList(String title, String message, String... lst) {
        ListDialog dialog = new ListDialog(this.editor.getSite().getShell());
        dialog.setTitle(title);
        dialog.setMessage(message);
        dialog.setContentProvider(new ArrayContentProvider());
        dialog.setInput(lst);
        dialog.setLabelProvider(new LabelProvider());

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            Object[] result = dialog.getResult();
            return (String) result[0];
        }
        return null;
    }

    @Override
    public Object selectFromArray(String title, String message, Object[] input, ILabelProvider labelProvider) {
        ListDialog dialog = new ListDialog(this.editor.getSite().getShell());
        dialog.setTitle(title);
        dialog.setMessage(message);
        dialog.setContentProvider(new ArrayContentProvider());
        dialog.setInput(input);
        dialog.setLabelProvider(labelProvider);

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            return dialog.getResult()[0];
        }
        return null;
    }

    @Override
    public String selectFile(String title, String[] extensions) {
        ResourceListSelectionDialog dialog = new FilteredResourceListSelectionDialog(this.editor.getSite().getShell(), this.contentRoot, IResource.FILE, extensions);
        dialog.setTitle(title);

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            IResource r = (IResource) dialog.getResult()[0];
            return EditorUtil.makeResourcePath(r);
        }
        return null;
    }

    @Override
    public String[] selectFiles(String title, String[] extensions) {
        ResourceListSelectionDialog dialog = new FilteredResourceListSelectionDialog(this.editor.getSite().getShell(), this.contentRoot, IResource.FILE, extensions);
        dialog.setTitle(title);

        int ret = dialog.open();
        if (ret == Dialog.OK) {
            List<String> lst = new ArrayList<String>();
            for (Object x : dialog.getResult()) {
                lst.add(EditorUtil.makeResourcePath((IResource) x));
            }
            return lst.toArray(new String[lst.size()]);
        }
        return null;
    }

    @Override
    public void getCameraFocusPoint(Point3d focusPoint) {
        Vector4d p = this.renderView.getCameraFocusPoint();
        focusPoint.set(p.x, p.y, p.z);
    }

    @Override
    public void frameSelection() {
        this.renderView.frameSelection();
    }

    @Override
    protected IWorkbenchPart getPart() {
        return this.editor;
    }


}
