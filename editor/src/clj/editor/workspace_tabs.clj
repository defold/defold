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
  (:import [javafx.scene.control SplitPane Tab TabPane]))

(defn- tab->resource [^Tab tab]
  (some-> tab
    (ui/user-data :editor.app-view/view)
    (g/node-value :view-data)
    second
    :resource))

(defn- serialize-open-tabs [app-view]
  (let [editor-tabs-split ^SplitPane (g/with-auto-evaluation-context ec
                                       (g/node-value app-view :editor-tabs-split ec))]
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

(defn- serialize-tab-selections [app-view]
  (g/with-auto-evaluation-context evaluation-context
    (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)
          active-tab-pane (g/node-value app-view :active-tab-pane evaluation-context)
          tab-panes (.getItems editor-tabs-split)]
      {:selected-pane (.indexOf tab-panes active-tab-pane)
       :selected-tabs-idx (mapv (fn [^TabPane pane]
                                  (-> pane .getSelectionModel .getSelectedIndex))
                                tab-panes)})))

(defn save-open-tabs [prefs app-view]
  (prefs/set! prefs [:workflow :open-tabs] (serialize-open-tabs app-view)))

(defn save-tab-selections [prefs app-view]
  (prefs/set! prefs [:workflow :last-selected-tabs] (serialize-tab-selections app-view)))

(comment
  (defn save-open-tabs [prefs app-view] nil)
  (defn save-tab-selections [prefs app-view] nil)
  (g/with-auto-evaluation-context ec
    (let [editor-tabs-split ^SplitPane (g/node-value (dev/app-view) :editor-tabs-split ec)
          active-tab-pane (g/node-value (dev/app-view) :active-tab-pane ec)
          tab-panes (.getItems editor-tabs-split)]
      (-> (second tab-panes) .getSelectionModel .getSelectedIndex)))
  ,)
