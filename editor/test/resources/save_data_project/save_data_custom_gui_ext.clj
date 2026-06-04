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
            [editor.resource :as-alias resource]
            [editor.types]))

(set! *warn-on-reflection* true)

(g/defnode SaveDataCustomGuiNode
  (inherits gui/BoxNode)

  (property test-boolean g/Bool (default false)
            (static custom-property {:id "test_boolean"
                                     :protobuf-type :type-boolean})
            (dynamic edit-type (gui/layout-property-edit-type test-boolean {:type g/Bool}))
            (value (gui/layout-property-getter test-boolean))
            (set (gui/layout-property-setter test-boolean)))
  (property test-hash g/Str (default "")
            (static custom-property {:id "test_hash"
                                     :protobuf-type :type-hash})
            (dynamic edit-type (gui/layout-property-edit-type test-hash {:type g/Str}))
            (value (gui/layout-property-getter test-hash))
            (set (gui/layout-property-setter test-hash)))
  (property test-number g/Num (default 0.0)
            (static custom-property {:id "test_number"
                                     :protobuf-type :type-number})
            (dynamic edit-type (gui/layout-property-edit-type test-number {:type g/Num}))
            (value (gui/layout-property-getter test-number))
            (set (gui/layout-property-setter test-number)))
  (property test-quat editor.types/Vec4 (default [0.0 0.0 0.0 1.0])
            (static custom-property {:id "test_quat"
                                     :protobuf-type :type-quat})
            (dynamic edit-type (gui/layout-property-edit-type test-quat {:type editor.types/Vec4}))
            (value (gui/layout-property-getter test-quat))
            (set (gui/layout-property-setter test-quat)))
  (property test-string g/Str (default "")
            (static custom-property {:id "test_string"
                                     :protobuf-type :type-string})
            (dynamic edit-type (gui/layout-property-edit-type test-string {:type g/Str}))
            (value (gui/layout-property-getter test-string))
            (set (gui/layout-property-setter test-string)))
  (property test-vector3 editor.types/Vec3 (default [0.0 0.0 0.0])
            (static custom-property {:id "test_vector3"
                                     :protobuf-type :type-vector3})
            (dynamic edit-type (gui/layout-property-edit-type test-vector3 {:type editor.types/Vec3}))
            (value (gui/layout-property-getter test-vector3))
            (set (gui/layout-property-setter test-vector3)))
  (property test-vector4 editor.types/Vec4 (default [0.0 0.0 0.0 0.0])
            (static custom-property {:id "test_vector4"
                                     :protobuf-type :type-vector4})
            (dynamic edit-type (gui/layout-property-edit-type test-vector4 {:type editor.types/Vec4}))
            (value (gui/layout-property-getter test-vector4))
            (set (gui/layout-property-setter test-vector4))))

#_{:clj-kondo/ignore [:unused-private-var]}
(defn- register-gui-resource-types! [workspace]
  (g/transact
    (concat
      (gui/register-custom-node-type-info
        workspace
        {:node-type SaveDataCustomGuiNode
         :display-name "Save Data Custom"
         :custom-type-name "SaveDataCustom"
         :icon "icons/32/Icons_40-GUI-Box-node.png"
         :defaults gui/shape-base-node-defaults})
      (gui/register-node-tree-attachment-node-type workspace SaveDataCustomGuiNode))))
