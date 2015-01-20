(ns internal.ui.dialogs
  (:require [dynamo.file :as file]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [eclipse.markers :as markers]
            [internal.query :as iq])
  (:import [org.eclipse.ui PlatformUI]
           [org.eclipse.ui.dialogs FilteredItemsSelectionDialog FilteredItemsSelectionDialog$AbstractContentProvider FilteredItemsSelectionDialog$ItemsFilter]
           [org.eclipse.ui.model WorkbenchLabelProvider]
           [org.eclipse.core.runtime IProgressMonitor]
           [org.eclipse.core.resources IFile]
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
        (t/local-path (:filename o1))
        (t/local-path (:filename o2))))))

(defn resource-item-detail-provider
  [^FilteredItemsSelectionDialog dlg]
  (proxy [LabelProvider] []
    (getText [element]
      (let [^LabelProvider this this]
        (if-not (:filename element)
         (proxy-super getText element)
         (t/local-path (:filename element)))))))

(defn resource-item-label-provider
  [^FilteredItemsSelectionDialog dlg]
  (let [listeners          (ui/make-event-broadcaster)
        workbench-provider (WorkbenchLabelProvider.)
        item-provider      (proxy [LabelProvider ILabelProviderListener DelegatingStyledCellLabelProvider$IStyledLabelProvider] []
                             (getImage [element]
                               (let [^LabelProvider this this]
                                 (if-not (:filename element)
                                   (proxy-super getImage element)
                                   (.getImage workbench-provider (file/project-file (:filename element))))))

                             (getText [element]
                               (let [^LabelProvider this this]
                                 (if-not (:filename element)
                                  (proxy-super getText element)
                                  (let [efile ^IFile (file/project-file (:filename element))
                                        name  (.getName efile)]
                                    (if (.isDuplicateElement dlg element)
                                      (str name " - " (.. efile getParent getFullPath makeRelative))
                                      name)))))

                             (getStyledText [element]
                               (let [^LabelProvider this this]
                                 (if-not (:filename element)
                                  (StyledString. (proxy-super getText element))
                                  (let [efile ^IFile (file/project-file (:filename element))
                                        name  (.getName efile)]
                                    (if (.isDuplicateElement dlg element)
                                      (doto (StyledString. name)
                                        (.append " - " StyledString/QUALIFIER_STYLER)
                                        (.append (.. efile getParent getFullPath makeRelative toString) StyledString/QUALIFIER_STYLER))
                                      (StyledString. name))))))

                             (dispose []
                               (let [^LabelProvider this this]
                                 (.removeListener workbench-provider this)
                                 (.dispose workbench-provider)
                                 (proxy-super dispose)))

                             (addListener [listener]
                               (ui/add-listener listeners listener #(.labelProviderChanged ^ILabelProviderListener listener %)))

                             (removeListener [listener]
                               (ui/remove-listener listeners listener))

                             (labelProviderChanged [event]
                               (ui/send-event listeners event)))]
    (.addListener workbench-provider item-provider)
    item-provider))

(defn- make-resource-selection-dialog
  [shell multiselect? resource-seq]
  (proxy [ShimItemsSelectionDialog] [shell multiselect?]
    (createFilter []
      (let [dlg this]
        (proxy [FilteredItemsSelectionDialog$ItemsFilter] [this]
          (matchItem [item]
            (let [nm (:filename item)
                  filter ^FilteredItemsSelectionDialog$ItemsFilter this]
              (or
                (.matches filter ^String (t/local-name nm))
                (.matches filter ^String (t/local-path nm)))))

          (isConsistentItem [item] (not (nil? (:filename item)))))))

    (createExtendedContentArea [parent] nil)

    (getDialogSettings []
      (dialog-settings resource-selection-dialog-settings))

    (getElementName [o]
      (t/local-name (:filename o)))

    (getItemsComparator [] item-comparator)

    (fillContentProvider [^FilteredItemsSelectionDialog$AbstractContentProvider content-provider ^FilteredItemsSelectionDialog$ItemsFilter items-filter ^IProgressMonitor progress-monitor]
      (let [^ShimItemsSelectionDialog this this]
        (monitored-task progress-monitor "Searching" (count resource-seq)
          (doseq [node resource-seq]
            (monitored-work progress-monitor ""
              (.addToContentProvider this content-provider node items-filter))))))

    (validateItem [item]
      markers/ok-status)))

(defn- modal-dialog-shell []
  (.getShell (.getModalDialogShellProvider (PlatformUI/getWorkbench))))

(defn resource-selection-dialog
  [title multiselect? resource-seq]
  (let [dlg ^FilteredItemsSelectionDialog (make-resource-selection-dialog (modal-dialog-shell) multiselect? resource-seq)]
    (.setTitle dlg title)
    (.setListLabelProvider dlg (resource-item-label-provider dlg))
    (.setDetailsLabelProvider dlg (resource-item-detail-provider dlg))
    (.setInitialPattern dlg "**")
    (.open dlg)
    (seq (.getResult dlg))))

;(dialogs/resource-selection-dialog "Add Images" ["png" "jpg"])
