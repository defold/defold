package com.dynamo.cr.editor.ui;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.dialogs.FilteredResourcesSelectionDialog;

public class FilteredResourceListSelectionDialog extends FilteredResourcesSelectionDialog {

    String[] extensions;

    public FilteredResourceListSelectionDialog(Shell parentShell, IContainer container, int typesMask, final String[] extensions) {
        super(parentShell, false, container, typesMask);
        this.extensions = extensions;
        setInitialPattern("**", NONE);
    }

    public FilteredResourceListSelectionDialog(Shell parentShell, IContainer container, int typesMask, final String[] extensions, boolean multi) {
        super(parentShell, multi, container, typesMask);
        this.extensions = extensions;
        setInitialPattern("**", NONE);
    }

    @Override
    protected ItemsFilter createFilter() {
        return new Filter();
    }

    private class Filter extends ResourceFilter {

        public boolean matchItem(Object item) {
            if(extensions == null) {
                return true;
            }
            IResource resource = (IResource) item;
            return super.matchItem(item) && select(resource);
        }

        private boolean select(IResource resource) {
            String extension = resource.getFileExtension();
            if (extension != null && !extension.isEmpty()) {
                for (String ext : extensions) {
                    if (extension.equals(ext)) {
                        return true;
                    }
                }
            }
            return false;
        }

        public boolean equalsFilter(ItemsFilter filter) {
            if (!super.equalsFilter(filter)) {
                return false;
            }
            if (!(filter instanceof Filter)) {
                return false;
            }
            return true;
        }
    }
}
