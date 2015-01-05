(ns internal.ui.dialogs
  (:require [dynamo.file :as file]
            [dynamo.project :as p]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [eclipse.markers :as markers]
            [internal.query :as iq])
  (:import [org.eclipse.ui.dialogs FilteredItemsSelectionDialog FilteredItemsSelectionDialog$ItemsFilter]
           [org.eclipse.ui.model WorkbenchLabelProvider]
           [org.eclipse.jface.window IShellProvider]
           [org.eclipse.jface.viewers DelegatingStyledCellLabelProvider$IStyledLabelProvider ILabelProviderListener LabelProvider LabelProviderChangedEvent StyledString]
           [com.ibm.icu.text Collator]
           [com.dynamo.cr.sceneed Activator]))

(def ^:private resource-selection-dialog-settings "com.dynamo.cr.dialogs.ResourcesSelectionDialog")

(defn- dialog-settings
  [section]
  (if-let [settings (.. Activator getDefault getDialogSettings (getSection section))]
    settings
    (.. Activator getDefault getDialogSettings (addNewSection section))))

(def item-comparator
  (reify java.util.Comparator
    (^int compare [this o1 o2]
      (.compare (Collator/getInstance)
        (.. o1 getFullPath toOSString)
        (.. o2 getFullPath toOSString)))))

(def resource-item-label-provider
  (let [listeners          (ui/make-event-broadcaster)
        workbench-provider (WorkbenchLabelProvider.)
        item-provider      (proxy [LabelProvider ILabelProviderListener DelegatingStyledCellLabelProvider$IStyledLabelProvider] []
                             (getImage [element]
                               (if-not (:filename element)
                                 (proxy-super getImage element)
                                 (.getImage workbench-provider (file/eclipse-file (:filename element)))))

                             (getText [element]
                               (if-not (:filename element)
                                 (proxy-super getText element)
                                 (let [efile (file/eclipse-file (:filename element))
                                       name  (.getName efile)]
                                   (if (.isDuplicateElement this element)
                                     (str name " - " (.. efile getParent getFullPath makeRelative))
                                     name))))

                             (getStyledText [element]
                               (if-not (:filename element)
                                 (StyledString. (proxy-super getText element))
                                 (let [efile (file/eclipse-file (:filename element))
                                       name  (.getName efile)]
                                   (if (.isDuplicateElement this element)
                                     (doto (StyledString. name)
                                       (.append " - " StyledString/QUALIFIER_STYLER)
                                       (.append (.. efile getParent getFullPath makeRelative) StyledString/QUALIFIER_STYLER))
                                     (StyledString. name)))))

                             (dispose []
                               (.removeListener workbench-provider this)
                               (.dispose workbench-provider)
                               (proxy-super dispose))

                             (addListener [listener]
                               (ui/add-listener listeners listener #(.labelProviderChanged listener %)))

                             (removeListener [listener]
                               (ui/remove-listener listeners listener))

                             (labelProviderChanged [event]
                               (ui/send-event listeners event)))]
    (.addListener workbench-provider item-provider)
    item-provider))

(defn- make-resource-selection-dialog
  [shell resource-seq]
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
      (println :fillContentProvider :resource-seq resource-seq)
      (doseq-monitored progress-monitor "Searching"
        [node resource-seq]
        (println :fillContentProvider :node node)
        (.add content-provider node items-filter)))

    (validateItem [item]
      markers/ok-status)))

(defn resource-selection-dialog
  [^IShellProvider shell-provider title resource-seq]
  (let [dlg (make-resource-selection-dialog (.getShell shell-provider) resource-seq)]
    (.setTitle dlg title)
    (.setListLabelProvider dlg resource-item-label-provider)
    (.open dlg)
    (.getResult dlg)))

;(dialogs/resource-selection-dialog "Add Images" ["png" "jpg"])
