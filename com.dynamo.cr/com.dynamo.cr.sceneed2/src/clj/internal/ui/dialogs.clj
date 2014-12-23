(ns internal.ui.dialogs
  (:require [dynamo.types :as t]
            [dynamo.ui :as ui]
            [internal.query :as iq])
  (:import [org.eclipse.ui.dialogs FilteredItemsSelectionDialog FilteredItemsSelectionDialog$ItemsFilter]
           [org.eclipse.jface.window IShellProvider]
           [com.dynamo.cr.sceneed Activator]))

;(dialogs/resource-selection-dialog "Add Images" ["png" "jpg"])

(def ^:private resource-selection-dialog-settings "com.dynamo.cr.dialogs.ResourcesSelectionDialog")

(defn- dialog-settings
  [section]
  (if-let [settings (.. Activator getDefault getDialogSettings (getSection section))]
    settings
    (.. Activator getDefault getDialogSettings (addNewSection section))))


(defn- make-resource-selection-dialog
  [shell]
  (proxy [FilteredItemsSelectionDialog] [shell]
    (createFilter [] (proxy [FilteredItemsSelectionDialog$ItemsFilter] [this]
                       (matchItem [item] true)
                       (isConsistentItem [item] true)))

    (createExtendedContentArea [parent] nil)

    (getDialogSettings [] (dialog-settings resource-selection-dialog-settings))

;    	/**
;	 * Returns comparator to sort items inside content provider. Returned object
;	 * will be probably created as an anonymous class. Parameters passed to the
;	 * <code>compare(java.lang.Object, java.lang.Object)</code> are going to
;	 * be the same type as the one used in the content provider.
;	 *
;	 * @return comparator to sort items content provider
;	 */
;	protected abstract Comparator getItemsComparator();
;
;	/**
;	 * Fills the content provider with matching items.
;	 *
;	 * @param contentProvider
;	 *            collector to add items to.
;	 *            {@link FilteredItemsSelectionDialog.AbstractContentProvider#add(Object, FilteredItemsSelectionDialog.ItemsFilter)}
;	 *            only adds items that pass the given <code>itemsFilter</code>.
;	 * @param itemsFilter
;	 *            the items filter
;	 * @param progressMonitor
;	 *            must be used to report search progress. The state of this
;	 *            progress monitor reflects the state of the filtering process.
;	 * @throws CoreException
;	 */
;	protected abstract void fillContentProvider(
;			AbstractContentProvider contentProvider, ItemsFilter itemsFilter,
;			IProgressMonitor progressMonitor) throws CoreException;
;
;	/**
;	 * Returns name for then given object.
;	 *
;	 * @param item
;	 *            an object from the content provider. Subclasses should pay
;	 *            attention to the passed argument. They should either only pass
;	 *            objects of a known type (one used in content provider) or make
;	 *            sure that passed parameter is the expected one (by type
;	 *            checking like <code>instanceof</code> inside the method).
;	 * @return name of the given item
;	 */
;	public abstract String getElementName(Object item);
;
;	/**
;	 * An interface to content providers for
;	 * <code>FilterItemsSelectionDialog</code>.
;	 */
;	protected abstract class AbstractContentProvider {
;		/**
;		 * Adds the item to the content provider iff the filter matches the
;		 * item. Otherwise does nothing.
;		 *
;		 * @param item
;		 *            the item to add
;		 * @param itemsFilter
;		 *            the filter
;		 *
;		 * @see FilteredItemsSelectionDialog.ItemsFilter#matchItem(Object)
;		 */
;		public abstract void add(Object item, ItemsFilter itemsFilter);
;	}
;	/**
;	 * Validates the item. When items on the items list are selected or
;	 * deselected, it validates each item in the selection and the dialog status
;	 * depends on all validations.
;	 *
;	 * @param item
;	 *            an item to be checked
;	 * @return status of the dialog to be set
;	 */
;	protected abstract IStatus validateItem(Object item);
;	/**
;	 * Sets a new label provider for items in the list. If the label provider
;	 * also implements {@link
;	 * org.eclipse.jface.viewers.DelegatingStyledCellLabelProvider
;	 * .IStyledLabelProvider}, the style text labels provided by it will be used
;	 * provided that the corresponding preference is set.
;	 *
;	 * @see IWorkbenchPreferenceConstants#USE_COLORED_LABELS
;	 *
;	 * @param listLabelProvider
;	 * 		the label provider for items in the list
;	 */
;	public void setListLabelProvider(ILabelProvider listLabelProvider) {
;		getItemsListLabelProvider().setProvider(listLabelProvider);
;	}
    ))

(defn resource-selection-dialog
  [^IShellProvider shell-provider project-node title allowed-types]
  (let [dlg (make-resource-selection-dialog (.getShell shell-provider))]
    (.setTitle dlg title)
    (.open dlg)
    (.getResult dlg)))