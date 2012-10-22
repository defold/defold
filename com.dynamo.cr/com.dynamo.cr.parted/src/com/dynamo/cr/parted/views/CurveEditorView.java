package com.dynamo.cr.parted.views;

import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.part.ViewPart;

import com.dynamo.cr.sceneed.Activator;

// NOTE: This could be moved to another plugin if general enough
public class CurveEditorView extends ViewPart implements IPartListener {

    private Composite parent;
    private ICurveEditorPage page;
    private IWorkbenchPart trackingPart;

    public CurveEditorView() {
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removePartListener(this);
    }

    @Override
    public void createPartControl(Composite parent) {
        this.parent = parent;
        getSite().getPage().addPartListener(this);

        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.SCENEED_CONTEXT_ID);

        IEditorPart activeEditor = getSite().getPage().getActiveEditor();
        if (activeEditor instanceof IEditorPart) {
            openPage(activeEditor);
        }
    }

    @Override
    public void setFocus() {
        if (page != null) {
            page.setFocus();
        }
    }

    @Override
    public void partActivated(IWorkbenchPart part) {
        partOpened(part);
    }

    @Override
    public void partBroughtToTop(IWorkbenchPart part) {
    }

    private void openPage(IWorkbenchPart part) {
        ICurveEditorPage newPage = (ICurveEditorPage) part.getAdapter(ICurveEditorPage.class);
        if (newPage == page) {
            return;
        }

        closePage();
        page = newPage;
        trackingPart = null;
        if (page != null) {
            page.init((IViewSite) getSite());
            page.createControl(parent);
            parent.layout();
            trackingPart = part;
        }
    }

    private void closePage() {
        if (page != null) {
            page.dispose();
            Control control = page.getControl();

            if (!control.isDisposed()) {
                control.dispose();
            }
            page = null;
            trackingPart = null;
        }
    }

    @Override
    public void partClosed(IWorkbenchPart part) {
        if (part == trackingPart) {
            closePage();
        }
    }

    @Override
    public void partDeactivated(IWorkbenchPart part) {
    }

    @Override
    public void partOpened(IWorkbenchPart part) {
        if (part instanceof IEditorPart) {
            openPage(part);
        }
    }

    public void frame() {
        if (this.page != null) {
            page.frame();
        }
    }

}
