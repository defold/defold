;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.workspace-tabs
  (:require [dynamo.graph :as g]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.ui :as ui])
  (:import [javafx.scene.control Tab TabPane SplitPane]))

(defn- tab->resource [^Tab tab]
  (some-> tab
    (ui/user-data :editor.app-view/view)
    (g/node-value :view-data)
    second
    :resource))

(defn serialize-open-tabs
  [app-view evaluation-context]
  (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)]
    (mapv (fn [^TabPane tab-pane]
            (mapv (fn [tab]
                    [(resource/proj-path (tab->resource tab))
                     (-> tab
                         tab->resource
                         resource/resource-type
                         :view-types
                         first
                         :id)])
                  (.getTabs tab-pane)))
          (.getItems editor-tabs-split))))

(defn save-prefs  [prefs app-view evaluation-context]
  (prefs/set! prefs [:workflow :open-tabs] (serialize-open-tabs app-view evaluation-context)))
