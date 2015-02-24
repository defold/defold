(ns dynamo.outline
  (:require [clojure.zip :as zip]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.ui.widgets :as widgets]
            [internal.java :refer [bean-mapper]])
  (:import [org.eclipse.jface.viewers ITreeContentProvider ColumnLabelProvider]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.widgets Control Menu MenuItem Shell Tree]
           [org.eclipse.swt.events MenuEvent MenuListener SelectionEvent SelectionListener]
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

(def outline-tree-selection-listener
  (reify SelectionListener
    (widgetSelected [_ event]
      (when-let [command (.getData (.-widget event))]
        (t/apply-if-fn (:command-fn command) (:context command))))
    (widgetDefaultSelected [_ event] nil)))

(defn outline-tree-menu-listener [^Tree tree ^Menu menu]
  (let [listener (reify MenuListener
                   (menuShown  [_ event]
                     ;; based on http://stackoverflow.com/a/18403738
                     (doseq [i (.getItems menu)] (.dispose i))
                     (when-let [selection (some->> tree .getSelection first .getData zip/node)]
                       (doseq [command (:commands selection)]
                         (doto (MenuItem. menu SWT/NONE)
                           (.setText (:label command))
                           (.setEnabled (:enabled command true))
                           (.setData command)
                           (.addSelectionListener outline-tree-selection-listener)))))
                   (menuHidden [_ event] nil))]
    (.addMenuListener menu listener)
    listener))

(defn outline-tree-menu ^Menu [^Tree parent]
  (let [menu (Menu. parent)
        menu-listener (outline-tree-menu-listener parent menu)]
    (.setMenu parent menu)
    menu))

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
          widget-tree (widgets/make-control toolkit shell widget-def)
          menu        (outline-tree-menu (.getControl (widgets/widget widget-tree [:form :tree])))]
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
