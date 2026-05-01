;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

(ns editor.html-view
  (:require [dynamo.graph :as g]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.markdown :as markdown]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.control Tab]))

(g/defnk produce-desc [html project parent resource]
  {:fx/type fxui/ext-with-anchor-pane-props
   :desc {:fx/type fxui/ext-value :value parent}
   :props {:children [{:fx/type markdown/html-view
                       :root-props {:style-class "md-page-root"}
                       :anchor-pane/top 0
                       :anchor-pane/right 0
                       :anchor-pane/bottom 0
                       :anchor-pane/left 0
                       :html html
                       :project project
                       :base-resource resource}]}})

(g/defnode HtmlViewNode
  (inherits view/WorkbenchView)
  (property parent g/Any)
  (input resource resource/Resource)
  (input html g/Str)
  (input project g/NodeID)
  (output desc g/Any :cached produce-desc))

(defn- repaint! [view-node]
  (fxui/advance-graph-user-data-component! view-node :view (g/node-value view-node :desc)))

(defn- make-view [graph ^Parent parent html-node {:keys [project ^Tab tab]}]
  (let [view-node (first
                    (g/tx-nodes-added
                      (g/transact
                        (g/make-nodes graph [view [HtmlViewNode :parent parent]]
                          (g/connect html-node :html view :html)
                          (g/connect html-node :resource view :resource)
                          (g/connect project :_node-id view :project)))))
        repainter (doto (ui/->timer 30 "update-html-view" (fn [_ _ _]
                                                            (when (.isSelected tab)
                                                              (repaint! view-node))))
                    ui/timer-start!)]
    (repaint! view-node)
    (ui/on-closed! tab (fn [_]
                         (ui/timer-stop! repainter)
                         (fxui/advance-graph-user-data-component! view-node :view nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :html
                                :label (localization/message "resource.view.html")
                                :make-view-fn #'make-view))
