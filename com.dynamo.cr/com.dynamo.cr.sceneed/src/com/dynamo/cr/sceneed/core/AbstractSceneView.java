package com.dynamo.cr.sceneed.core;

import javax.inject.Inject;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.part.IContributedContentsView;
import org.eclipse.ui.views.contentoutline.ContentOutline;

public abstract class AbstractSceneView implements ISceneView {

    @Inject
    private ISceneView.IPresenter presenter;
    @Inject
    private ISceneView.IPresenterContext presenterContext;

    private boolean ignoreOutlineSelection = false;

    public void setIgnoreOutlineSelection(boolean ignoreOutlineSelection) {
        this.ignoreOutlineSelection = ignoreOutlineSelection;
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        boolean currentSelection = false;
        IWorkbenchPart thisPart = getPart();
        if (part == thisPart) {
            currentSelection = true;
        } else if (part instanceof ContentOutline && !this.ignoreOutlineSelection) {
            IContributedContentsView view = (IContributedContentsView)((ContentOutline)part).getAdapter(IContributedContentsView.class);
            currentSelection = view.getContributingPart() == thisPart;
        }
        if (currentSelection) {
            this.presenter.onSelect(presenterContext, (IStructuredSelection) selection);
        }
    }

    protected abstract IWorkbenchPart getPart();

}
