package com.dynamo.cr.parted.views;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.part.IContributedContentsView;
import org.eclipse.ui.part.IPage;
import org.eclipse.ui.part.MessagePage;
import org.eclipse.ui.part.PageBook;
import org.eclipse.ui.part.PageBookView;

import com.dynamo.cr.sceneed.Activator;

// NOTE: This could be moved to another plugin if general enough
public class CurveEditorView extends PageBookView {

    /**
     * Message to show on the default page.
     */
    private String defaultText = Messages.CurveEditor_noCurves;

    /**
     * Creates a curve editor view with no curve editor pages.
     */
    public CurveEditorView() {
        super();
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removePartListener(this);
    }

    @Override
    public void createPartControl(Composite parent) {
        super.createPartControl(parent);
        getSite().getPage().addPartListener(this);

        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.SCENEED_CONTEXT_ID);
    }

    /* (non-Javadoc)
     * Method declared on IViewPart.
     * Treat this the same as part activation.
     */
    @Override
    public void partBroughtToTop(IWorkbenchPart part) {
        partActivated(part);
    }

    protected void partHidden(IWorkbenchPart part) {
        // Ignore if part is hidden, else let base class handle it
        if (part != getCurrentContributingPart()) {
            super.partHidden(part);
        }
    }

    public void frame() {
        IPage page = getCurrentPage();
        if (page != null) {
            ((ICurveEditorPage)page).frame();
        }
    }

    @Override
    protected IPage createDefaultPage(PageBook book) {
        MessagePage page = new MessagePage();
        initPage(page);
        page.createControl(book);
        page.setMessage(defaultText);
        return page;
    }

    @Override
    protected PageRec doCreatePage(IWorkbenchPart part) {
        // Try to get a curve page.
        Object obj = part.getAdapter(ICurveEditorPage.class);
        if (obj instanceof ICurveEditorPage) {
            ICurveEditorPage page = (ICurveEditorPage) obj;
            initPage(page);
            page.createControl(getPageBook());
            ISelection selection = part.getSite().getSelectionProvider().getSelection();
            page.setSelection(selection);
            return new PageRec(part, page);
        }
        // There is no content outline
        return null;
    }

    @Override
    protected void doDestroyPage(IWorkbenchPart part, PageRec pageRecord) {
        ICurveEditorPage page = (ICurveEditorPage) pageRecord.page;
        page.dispose();
        pageRecord.dispose();
    }

    @Override
    protected IWorkbenchPart getBootstrapPart() {
        IWorkbenchPage page = getSite().getPage();
        if (page != null) {
            return page.getActiveEditor();
        }

        return null;
    }

    @Override
    protected boolean isImportant(IWorkbenchPart part) {
        //We only care about editors
        return (part instanceof IEditorPart);
    }

    public Object getAdapter(@SuppressWarnings("rawtypes") Class key) {
        if (key == IContributedContentsView.class) {
            return new IContributedContentsView() {
                public IWorkbenchPart getContributingPart() {
                    return getContributingEditor();
                }
            };
        }
        return super.getAdapter(key);
    }

    private IWorkbenchPart getContributingEditor() {
        return getCurrentContributingPart();
    }

}
