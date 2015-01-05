(ns internal.ui.dialogs
  (:require [dynamo.file :as file]
            [dynamo.project :as p]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [eclipse.markers :as markers]
            [internal.query :as iq])
  (:import [org.eclipse.ui.dialogs FilteredItemsSelectionDialog FilteredItemsSelectionDialog$AbstractContentProvider FilteredItemsSelectionDialog$ItemsFilter]
           [org.eclipse.ui.model WorkbenchLabelProvider]
           [org.eclipse.jface.window IShellProvider]
           [org.eclipse.jface.viewers DelegatingStyledCellLabelProvider$IStyledLabelProvider ILabelProviderListener LabelProvider LabelProviderChangedEvent StyledString]
           [com.ibm.icu.text Collator]
           [com.dynamo.cr.sceneed Activator]
           [internal.ui ShimItemsSelectionDialog]))

(set! *warn-on-reflection* true)

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
        (file/local-path (:filename o1))
        (file/local-path (:filename o2))))))

(defn resource-item-label-provider
  [dlg]
  (let [listeners          (ui/make-event-broadcaster)
        workbench-provider (WorkbenchLabelProvider.)
        item-provider      (proxy [LabelProvider ILabelProviderListener DelegatingStyledCellLabelProvider$IStyledLabelProvider] []
                             (getImage [element]
                               (if-not (:filename element)
                                 (proxy-super getImage element)
                                 (.getImage workbench-provider (file/eclipse-file (:filename element)))))

                             (getText [element]
                               (prn "label provider getText " element)
                               (if-not (:filename element)
                                 (proxy-super getText element)
                                 (let [efile (file/eclipse-file (:filename element))
                                       name  (.getName efile)]
                                   (if (.isDuplicateElement dlg element)
                                     (str name " - " (.. efile getParent getFullPath makeRelative))
                                     name))))

                             (getStyledText [element]
                               (if-not (:filename element)
                                 (StyledString. (proxy-super getText element))
                                 (let [efile (file/eclipse-file (:filename element))
                                       name  (.getName efile)]
                                   (if (.isDuplicateElement dlg element)
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
  (proxy [ShimItemsSelectionDialog] [shell]
    (createFilter []
      (let [dlg this]
        (proxy [FilteredItemsSelectionDialog$ItemsFilter] [this]
          (matchItem [item]
            (let [nm (:filename item)]
              (or
                (.matches this (file/local-name nm))
                (.matches this (file/local-path nm)))))

          (isConsistentItem [item] (not (nil? (:filename item)))))))

    (createExtendedContentArea [parent] nil)

    (getDialogSettings []
      (dialog-settings resource-selection-dialog-settings))

    (getElementName [o]
      (file/local-name (:filename o)))

    (getItemsComparator [] item-comparator)

    (fillContentProvider [content-provider items-filter progress-monitor]
      (monitored-task progress-monitor "Searching" (count resource-seq)
        (doseq [node resource-seq]
          (monitored-work progress-monitor ""
            (.addToContentProvider ^ShimItemsSelectionDialog this content-provider node items-filter)))))

    (validateItem [item]
      markers/ok-status)))

(defn resource-selection-dialog
  [^IShellProvider shell-provider title resource-seq]
  (let [dlg (make-resource-selection-dialog (.getShell shell-provider) resource-seq)]
    (.setTitle dlg title)
    (.setListLabelProvider dlg (resource-item-label-provider dlg))
    (.setInitialPattern dlg "**")
    (.open dlg)
    (.getResult dlg)))

;(dialogs/resource-selection-dialog "Add Images" ["png" "jpg"])
