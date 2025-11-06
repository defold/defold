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

(ns editor.game-properties
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.lang.ini :as ini]
            [editor.code.resource :as r]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [util.coll :as coll])
  (:import [java.io BufferedReader]))

(g/defnode GameProperties
  (inherits r/CodeEditorResourceNode)
  (output proj-path+meta-info g/Any :cached
          (g/fnk [_node-id resource lines]
            (try
              (with-open [rdr (BufferedReader. (data/lines-reader lines))]
                (coll/pair (resource/proj-path resource) (settings-core/load-meta-properties rdr)))
              (catch Exception e
                (g/->error _node-id :meta-info :fatal resource (.getMessage e) (ex-data e)))))))

(defn- additional-load-fn [project self _resource]
  (g/connect self :proj-path+meta-info project :proj-path+meta-info-pairs))

(defn register-resource-types [workspace]
  (r/register-code-resource-type
    workspace
    :ext "properties"
    :node-type GameProperties
    :additional-load-fn additional-load-fn
    :label (localization/message "resource.type.properties")
    :icon "icons/32/Icons_05-Project-info.png"
    :language "ini"
    :view-types [:code :default]
    :view-opts {:code {:grammar ini/grammar}}))
