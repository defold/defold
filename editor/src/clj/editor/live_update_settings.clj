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

(ns editor.live-update-settings
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]))

(def live-update-icon "icons/32/Icons_04-Project-file.png")

(g/defnode LiveUpdateSettingsNode
  (inherits resource-node/ResourceNode)

  (input settings-map g/Any)
  (output settings-map g/Any (gu/passthrough settings-map))

  (input form-data g/Any)
  (output form-data g/Any :cached
          (g/fnk [form-data]
            (assoc form-data :navigation false)))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))
  (output build-targets g/Any :cached (g/constantly [])))

(def ^:private basic-meta-info
  (with-open [rdr (io/reader (io/resource "liveupdate-meta.properties"))]
    (settings-core/load-meta-properties rdr)))

(defn- load-live-update-settings [load-opts {:keys [owner-resource source-value] self :node-id}]
  (g/make-nodes [settings-node settings/SettingsNode]
    (g/connect settings-node :_node-id self :nodes)
    (g/connect settings-node :save-value self :save-value)
    (g/connect settings-node :form-data self :form-data)
    (g/connect settings-node :settings-map self :settings-map)
    (settings/load-settings-node load-opts self settings-node owner-resource source-value basic-meta-info nil)))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "settings"
    :label (localization/message "resource.type.settings")
    :node-type LiveUpdateSettingsNode
    :load-fn load-live-update-settings
    :meta-settings (:settings basic-meta-info)
    :icon live-update-icon
    :view-types [:form :text]))

(defn get-live-update-settings-path [project]
  (resource/resource->proj-path (get (project/settings project) ["liveupdate" "settings"])))
