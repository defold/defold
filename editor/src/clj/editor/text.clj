(ns editor.text
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.layout Pane]
           [javafx.scene.control TextArea]))

(set! *warn-on-reflection* true)

(g/defnode TextView
  (inherits core/ResourceNode)

  (property text-area TextArea))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [text-area (TextArea.)]
    (.setText text-area (slurp (g/node-value resource-node :resource)))
    (when-let [cp (:caret-position opts)]
      (.positionCaret text-area cp))
    (.add (.getChildren ^Pane parent) text-area)
    (ui/fill-control text-area)
    (g/make-node! graph TextView :text-area text-area)))

(defn update-caret-pos [text-node {:keys [caret-position]}]
  (when caret-position
    (when-let [^TextArea text-area (g/node-value text-node :text-area)]
      (.positionCaret text-area caret-position))))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :text
                                :focus-fn update-caret-pos
                                :make-view-fn make-view))
