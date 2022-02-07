;; Copyright 2020 The Defold Foundation
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.text
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.layout Pane]
           [javafx.scene.control TextArea]))

(set! *warn-on-reflection* true)

(g/defnode TextView
  (inherits view/WorkbenchView)

  (property text-area TextArea))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [text-area (TextArea.)]
    (.setEditable text-area false)
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
                                :label "Text"
                                :focus-fn update-caret-pos
                                :make-view-fn make-view))
