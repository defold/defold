package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IWorkbenchPart;

public class FormPropertySheetPage implements IFormPropertySheetPage {

    private IWorkbenchPart sourcePart;
    private FormPropertySheetViewer viewer;
    private final PartListener partListener = new PartListener();
    private IContainer contentRoot;

    public FormPropertySheetPage(IContainer contentRoot) {
        this.contentRoot = contentRoot;
    }

    private class PartListener implements IPartListener {
        @Override
        public void partActivated(IWorkbenchPart part) {}
        @Override
        public void partBroughtToTop(IWorkbenchPart part) {}

        @Override
        public void partClosed(IWorkbenchPart part) {
            if (sourcePart == part) {
                if (sourcePart != null)
                    sourcePart.getSite().getPage().removePartListener(partListener);
                sourcePart = null;
                if (viewer != null && !viewer.getControl().isDisposed()) {
                    viewer.setInput(new Object[0]);
                }
            }
        }

        @Override
        public void partDeactivated(IWorkbenchPart part) {}
        @Override
        public void partOpened(IWorkbenchPart part) {}
    }

    @Override
    public void createControl(Composite parent) {
        viewer = new FormPropertySheetViewer(parent, contentRoot);
    }

    @Override
    public void dispose() {
        if (viewer != null) {
            viewer.dispose();
        }
    }

    @Override
    public Control getControl() {
        return this.viewer.getControl();
    }

    public FormPropertySheetViewer getViewer() {
        return this.viewer;
    }

    @Override
    public void setActionBars(IActionBars actionBars) {
        // Not supported
    }

    @Override
    public void setFocus() {
        this.viewer.getControl().setFocus();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (sourcePart != null) {
            sourcePart.getSite().getPage().removePartListener(partListener);
            sourcePart = null;
        }

        if (selection instanceof IStructuredSelection) {
            this.sourcePart = part;
        }
        setSelection(selection);

        if (sourcePart != null) {
            sourcePart.getSite().getPage().addPartListener(partListener);
        }
    }

    @Override
    public void refresh() {
        if (viewer != null && !this.viewer.getControl().isDisposed()) {
            viewer.refresh();
        }
    }

    @Override
    public void setSelection(ISelection selection) {
        if (this.viewer != null && selection instanceof IStructuredSelection) {
            viewer.setInput(((IStructuredSelection) selection).toArray());
        }
    }

}
