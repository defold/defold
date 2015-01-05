package internal.ui;

import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.dialogs.FilteredItemsSelectionDialog;

public abstract class ShimItemsSelectionDialog extends FilteredItemsSelectionDialog {
    public ShimItemsSelectionDialog(Shell shell, boolean multi) {
        super(shell, multi);
    }

    /**
     * This function exists to allow Clojure code to call
     * AbstractContentProvider.add() on the protected abstract class. Clojure's
     * 'proxy' function doesn't allow access to protected members from the
     * proxy.
     */
    public void addToContentProvider(AbstractContentProvider contentProvider, Object item, ItemsFilter itemsFilter) {
        contentProvider.add(item, itemsFilter);
    }
}
