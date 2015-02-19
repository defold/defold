(ns dynamo.outline
  (:require [clojure.zip :as zip]
            [dynamo.ui :as ui]
            [dynamo.ui.widgets :as widgets])
  (:import [org.eclipse.jface.viewers ITreeContentProvider ColumnLabelProvider]
           [org.eclipse.swt.widgets Control Shell]
           [org.eclipse.ui.forms.widgets FormToolkit]))

(defn outline-tree-zipper [outline-tree]
  (zip/zipper (constantly true) :children nil outline-tree))

(defn outline-tree-content-provider ^ITreeContentProvider []
  (reify ITreeContentProvider
    (dispose      [_] nil)
    (inputChanged [_ viewer old-input new-input] nil)
    (getElements  [_ element] (into-array [(outline-tree-zipper element)]))
    (getChildren  [_ element] (->> element zip/down (iterate zip/right) (take-while identity) into-array))
    (getParent    [_ element] (zip/up element))
    (hasChildren  [_ element] (boolean (seq (zip/children element))))))

(defn outline-tree-label-provider []
  (proxy [ColumnLabelProvider] []
    (getText [element] (->> element zip/node :label str))))

(defn outline-tree-gui []
  (ui/swt-await
    (let [shell       (ui/shell)
          toolkit     (FormToolkit. (.getDisplay shell))
          widget-def  [:form {:type   :form
                              :text   "Outline!"
                              :layout {:type :grid :columns [{:horizontal-alignment :fill :vertical-alignment :fill}]}
                              :children [[:tree {:type :tree-viewer
                                                 :content-provider (outline-tree-content-provider)
                                                 :label-provider (outline-tree-label-provider)}]]}]
          widget-tree (widgets/make-control toolkit shell widget-def)]
      (.pack shell)
      (.open shell)
      widget-tree)))

(defn set-outline-tree-gui-data [widget-tree outline-tree]
  (ui/swt-safe
    (widgets/update-ui! (:form widget-tree)
      {:children [[:tree {:input outline-tree :expand-levels :all}]]})
    (.pack (.getShell ^Control (widgets/widget widget-tree [:form])))))

(defn close-outline-tree-gui-data [widget-tree]
  (ui/swt-safe
    (let [widget ^Control (widgets/widget widget-tree [:form])]
      (when-not (.isDisposed widget)
        (.close (.getShell widget))))))
