package com.dynamo.cr.properties;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.ui.views.properties.IPropertySheetPage;

public interface IFormPropertySheetPage extends IPropertySheetPage {
    void refresh();
    void setSelection(ISelection selection);
}
