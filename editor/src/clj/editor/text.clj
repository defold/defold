(ns editor.text
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.layout Pane]
           [javafx.scene.control TextArea]))

(g/defnode TextView
  (inherits core/ResourceNode)

  (property text-area TextArea))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [text-area (TextArea.)]
    (.setText text-area (slurp (g/node-value resource-node :resource)))
    (when-let [cp (:initial-caret-position opts)]
      (.positionCaret text-area cp))
    (.add (.getChildren ^Pane parent) text-area)
    (ui/fill-control text-area)
    (g/make-node! graph TextView :text-area text-area)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :text
                                :make-view-fn make-view))
