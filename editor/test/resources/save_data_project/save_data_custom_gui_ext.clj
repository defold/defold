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

(ns save-data-project.save-data-custom-gui-ext
  (:require [dynamo.graph :as g]
            [editor.gui :as gui]
            [editor.resource :as-alias resource]))

(set! *warn-on-reflection* true)

(g/defnode SaveDataCustomGuiNode
  (inherits gui/BoxNode))

#_{:clj-kondo/ignore [:unused-private-var]}
(defn- register-gui-resource-types! [workspace]
  (g/transact
    (concat
      (gui/register-custom-node-type-info
        workspace
        {:node-cls SaveDataCustomGuiNode
         :display-name "Save Data Custom"
         :custom-type-name "SaveDataCustom"
         :icon "icons/32/Icons_40-GUI-Box-node.png"
         :defaults gui/shape-base-node-defaults
         :custom-properties [{:id "test_boolean"
                              :type g/Bool}
                             {:id "test_hash"
                              :type g/Str
                              :protobuf-type :type-hash}
                             {:id "test_number"
                              :type g/Num}
                             {:id "test_quat"
                              :type g/Any
                              :protobuf-type :type-quat}
                             {:id "test_string"
                              :type g/Str}
                             {:id "test_vector3"
                              :type g/Any
                              :protobuf-type :type-vector3}
                             {:id "test_vector4"
                              :type g/Any
                              :protobuf-type :type-vector4}]})
      (gui/register-node-tree-attachment-node-type workspace SaveDataCustomGuiNode))))
