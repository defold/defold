package com.dynamo.cr.editor.ui;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.dialogs.ResourceListSelectionDialog;

public class FilteredResourceListSelectionDialog extends ResourceListSelectionDialog {

    String[] extensions;

    public FilteredResourceListSelectionDialog(Shell parentShell, IContainer container, int typesMask, final String[] extensions) {
        super(parentShell, container, typesMask);
        this.extensions = extensions;
        setTitle("Select Resource");
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        // Find and set the initial value of the text
        // NOTE This is a bit of a hack but could not find a better way to do it with minimal effort
        Composite composite = (Composite)super.createDialogArea(parent);
        for (Control child : composite.getChildren()) {
            if (child instanceof Text) {
                Text text = (Text)child;
                text.setText("*");
                text.selectAll();
                break;
            }
        }
        refresh(true);
        return composite;
    }

    @Override
    protected boolean select(IResource resource) {
        String extension = resource.getFileExtension();
        if (extension != null && !extension.isEmpty()) {
            for (String ext : this.extensions) {
                if (extension.equals(ext)) {
                    return true;
                }
            }
        }
        return false;
    }

}
