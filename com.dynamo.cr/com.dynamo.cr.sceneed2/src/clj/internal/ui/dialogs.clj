(ns internal.ui.dialogs
  (:require [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [eclipse.markers :as markers]
            [internal.query :as iq])
  (:import [org.eclipse.ui.dialogs FilteredItemsSelectionDialog FilteredItemsSelectionDialog$ItemsFilter]
           [org.eclipse.jface.window IShellProvider]
           [com.ibm.icu.text Collator]
           [com.dynamo.cr.sceneed Activator]))

(def ^:private resource-selection-dialog-settings "com.dynamo.cr.dialogs.ResourcesSelectionDialog")

(defn- dialog-settings
  [section]
  (if-let [settings (.. Activator getDefault getDialogSettings (getSection section))]
    settings
    (.. Activator getDefault getDialogSettings (addNewSection section))))

(defn compare-names
  [s1 s2]
  )

(def item-comparator
  (reify java.util.Comparator
    (^int compare [this o1 o2]
      (.compare (Collator/getInstance)
        (.. o1 getFullPath toOSString)
        (.. o2 getFullPath toOSString)))))

(defn- make-resource-selection-dialog
  [shell project-node]
  (proxy [FilteredItemsSelectionDialog] [shell]
    (createFilter []
      (proxy [FilteredItemsSelectionDialog$ItemsFilter] [this]
        (matchItem [item] true)
        (isConsistentItem [item] true)))

    (createExtendedContentArea [parent] nil)

    (getDialogSettings []
      (dialog-settings resource-selection-dialog-settings))

    (getElementName [o]
      (.getName (:filename o)))

    (getItemsComparator [] item-comparator)

    (fillContentProvider [content-provider items-filter progress-monitor]
      (doseq-monitored progress-monitor "Searching"
        [item (foo )]
        (.add content-provider item items-filter)))

    (validateItem [item]
      markers/ok-status)


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
  (let [dlg (make-resource-selection-dialog (.getShell shell-provider) project-node)]
    (.setTitle dlg title)
    (.open dlg)
    (.getResult dlg)))

;(dialogs/resource-selection-dialog "Add Images" ["png" "jpg"])
