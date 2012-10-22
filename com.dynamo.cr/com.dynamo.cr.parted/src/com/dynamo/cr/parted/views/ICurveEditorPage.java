package com.dynamo.cr.parted.views;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.ui.part.IPageBookViewPage;

// NOTE: This class and related view could be move if general enough
public interface ICurveEditorPage extends IPageBookViewPage {

    public void frame();

    public void setSelection(ISelection selection);
    public void refresh();
}
