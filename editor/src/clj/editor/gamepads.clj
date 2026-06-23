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

(ns editor.gamepads
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as data]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.bob Platform]
           [com.dynamo.bob.pipeline GamepadBuilder]
           [com.dynamo.input.proto Input$GamepadMaps Input$GamepadMapsRuntime]
           [java.nio.charset StandardCharsets]
           [org.apache.commons.io IOUtils]
           [org.apache.commons.io.input ReaderInputStream]))

(set! *warn-on-reflection* true)

(def ^:private gamepads-def
  {:label (localization/message "resource.type.gamepads")
   :icon "icons/32/Icons_34-Gamepad.png"
   :icon-class :property
   :category (localization/message "resource.category.project_settings")
   :pb-class Input$GamepadMaps
   :view-types [:cljfx-form-view :text]})

(defn- gamepads->bytes
  ^bytes [gamepads]
  (let [gamepads-str (protobuf/map->str Input$GamepadMaps gamepads)]
    (.getBytes ^String gamepads-str StandardCharsets/UTF_8)))

(defn- gamepad-database-user-data [gamepad-database-resource gamepad-database-lines]
  (when-let [gamepad-database-resource (and (some? gamepad-database-lines)
                                            (some? gamepad-database-resource)
                                            (resource/exists? gamepad-database-resource)
                                            gamepad-database-resource)]
    (with-open [input-stream (ReaderInputStream. (data/lines-reader gamepad-database-lines) StandardCharsets/UTF_8)]
      {:gamepad-database-path (resource/proj-path gamepad-database-resource)
       :gamepad-database-bytes (IOUtils/toByteArray input-stream)})))

(defn- build-gamepads [build-resource _dep-resources user-data]
  (let [gamepads-resource (:resource build-resource)]
    {:resource build-resource
     :content (GamepadBuilder/compile (resource/proj-path gamepads-resource)
                                      (gamepads->bytes (:pb user-data))
                                      (:gamepad-database-path user-data)
                                      (:gamepad-database-bytes user-data)
                                      (:platform user-data))}))

(defn make-build-target [node-id resource pb gamepad-database-resource gamepad-database-lines]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-build-resource resource)
     :build-fn build-gamepads
     :user-data (merge {:pb pb
                        :platform (.getPair (Platform/getHostPlatform))}
                       (gamepad-database-user-data gamepad-database-resource gamepad-database-lines))}))

(g/defnk produce-build-targets [_node-id resource pb]
  [(make-build-target _node-id resource pb nil nil)])

(g/defnk produce-form-data [_node-id pb]
  (protobuf-forms/produce-form-data _node-id pb gamepads-def))

(g/defnk produce-save-value [pb]
  (protobuf/clear-defaults Input$GamepadMaps pb))

(g/defnode GamepadsNode
  (inherits resource-node/ResourceNode)

  (property pb g/Any ; Always assigned in load-fn.
            (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-gamepads [_project self _resource pb-map-without-defaults]
  (g/set-properties self
    :pb (protobuf/inject-defaults Input$GamepadMaps pb-map-without-defaults)))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "gamepads"
    :label (:label gamepads-def)
    :node-type GamepadsNode
    :ddf-type Input$GamepadMaps
    :built-pb-class Input$GamepadMapsRuntime
    :load-fn load-gamepads
    :icon (:icon gamepads-def)
    :icon-class (:icon-class gamepads-def)
    :category (:category gamepads-def)
    :view-types (:view-types gamepads-def)))
