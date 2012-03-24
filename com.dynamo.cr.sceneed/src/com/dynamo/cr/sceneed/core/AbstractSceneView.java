package com.dynamo.cr.sceneed.core;

import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import javax.inject.Inject;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.part.IContributedContentsView;
import org.eclipse.ui.views.contentoutline.ContentOutline;

public abstract class AbstractSceneView implements ISceneView {

    @Inject private ISceneView.IPresenter presenter;

    private ISelection selection;
    private boolean ignoreOutlineSelection = false;

    public ISelection getSelection() {
        return this.selection;
    }

    public void setSelection(ISelection selection) {
        this.selection = selection;
    }

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
        if (currentSelection && !sameSelection(this.selection, selection)) {
            this.presenter.onSelect((IStructuredSelection)selection);
        }
    }

    protected abstract IWorkbenchPart getPart();

    private boolean sameSelection(ISelection selectionA, ISelection selectionB) {
        if (selectionA == selectionB) {
            return true;
        }
        if (selectionA instanceof IStructuredSelection && selectionB instanceof IStructuredSelection) {
            IStructuredSelection a = (IStructuredSelection)selectionA;
            IStructuredSelection b = (IStructuredSelection)selectionB;
            int sA = a.size();
            int sB = b.size();
            if (sA != sB) {
                return false;
            }
            if (a.isEmpty() && b.isEmpty()) {
                return true;
            }
            @SuppressWarnings("unchecked")
            List<Object> lA = a.toList();
            @SuppressWarnings("unchecked")
            List<Object> lB = b.toList();
            Comparator<Object> comp = new Comparator<Object>() {
                @Override
                public int compare(Object o1, Object o2) {
                    return o2.hashCode() - o1.hashCode();
                }
            };
            Collections.sort(lA, comp);
            Collections.sort(lB, comp);
            Iterator<Object> itA = lA.iterator();
            Iterator<Object> itB = lB.iterator();
            while (itA.hasNext()) {
                Object oA = itA.next();
                Object oB = itB.next();
                if (oA != oB) {
                    return false;
                }
            }
            // Same objects in both selections
            return true;
        }
        // No idea what type of selection, assume inequality
        return false;
    }

}
