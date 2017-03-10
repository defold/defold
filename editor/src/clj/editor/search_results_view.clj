(ns editor.search-results-view
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs])
  (:import [javafx.scene.control TabPane TreeView]
           [javafx.scene.layout AnchorPane]
           (javafx.event Event)))

(set! *warn-on-reflection* true)

(g/defnode SearchResultsView
  (inherits core/Scope)
  (property open-resource-fn g/Any)
  (property search-results-container g/Any))

(defn make-search-results-view [view-graph ^AnchorPane search-results-container open-resource-fn]
  (let [view-id (g/make-node! view-graph SearchResultsView
                              :open-resource-fn open-resource-fn
                              :search-results-container search-results-container)]
    view-id))

(defn update-search-results [search-results-view ^TreeView results-tree-view]
  (let [search-results-container ^AnchorPane (g/node-value search-results-view :search-results-container)
        open-resource-fn (g/node-value search-results-view :open-resource-fn)
        open-selected! (fn []
                         (doseq [[resource opts] (dialogs/resolve-search-in-files-tree-view-selection (ui/selection results-tree-view))]
                           (open-resource-fn resource opts)))]
    ;; Replace tree view.
    (doto (.getChildren search-results-container)
      (.clear)
      (.add (doto results-tree-view
              (AnchorPane/setTopAnchor 0.0)
              (AnchorPane/setRightAnchor 0.0)
              (AnchorPane/setBottomAnchor 0.0)
              (AnchorPane/setLeftAnchor 0.0)
              (ui/on-double! (fn on-double! [^Event event]
                               (.consume event)
                               (open-selected!))))))

    ;; Select tab-pane.
    (let [^TabPane tab-pane (.getParent (.getParent search-results-container))]
      (.select (.getSelectionModel tab-pane) 3))))

(defn open-resource-fn [search-results-view]
  (g/node-value search-results-view :open-resource-fn))
