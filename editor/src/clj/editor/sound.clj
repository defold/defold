;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.sound
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.outline :as outline]
            [editor.pipeline :as pipeline]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Sound$SoundDesc]
           [java.io IOException]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))

(def sound-icon "icons/32/Icons_26-AT-Sound.png")

(def supported-audio-formats #{"wav" "ogg"})

(defn- resource->bytes [resource]
  (with-open [in (io/input-stream resource)]
    (IOUtils/toByteArray in)))

(defn- build-sound-source
  [resource dep-resources user-data]
  {:resource resource :content (resource->bytes (:resource resource))})

(g/defnk produce-source-build-targets [_node-id resource]
  (try
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-sound-source
        :user-data {:content-hash (resource/resource->sha1-hex resource)}})]
    (catch IOException e
      (g/->error _node-id :resource :fatal resource (format "Couldn't read audio file %s" (resource/resource->proj-path resource))))))

(g/defnode SoundSourceNode
  (inherits resource-node/ResourceNode)

  (output build-targets g/Any produce-source-build-targets))

;;--------------------------------------------------------------------

(g/defnk produce-outline-data
  [_node-id]
  {:node-id _node-id
   :node-outline-key "Sound"
   :label "Sound"
   :icon sound-icon})

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id sound looping group gain pan speed loopcount]
  {:navigation false
   :form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :sections [{:title "Sound"
               :fields [{:path [:sound]
                         :label "Sound"
                         :type :resource
                         :filter supported-audio-formats}
                        {:path [:looping]
                         :label "Loop"
                         :type :boolean}
                        {:path [:loopcount]
                         :label "Loopcount"
                         :type :integer}
                        {:path [:group]
                         :label "Group"
                         :type :string}
                        {:path [:gain]
                         :label "Gain"
                         :type :number}
                         {:path [:pan]
                         :label "Pan"
                         :type :number}
                         {:path [:speed]
                         :label "Speed"
                         :type :number}
                         ]}]
   :values {[:sound] sound
            [:looping] looping
            [:group] group
            [:gain] gain
            [:pan] pan
            [:speed] speed
            [:loopcount] loopcount}})

(g/defnk produce-save-value
  [_node-id sound-resource looping group gain pan speed loopcount]
  (protobuf/make-map-without-defaults Sound$SoundDesc
    :sound (resource/resource->proj-path sound-resource)
    :looping (protobuf/boolean->int looping)
    :group group
    :gain gain
    :pan pan
    :speed speed
    :loopcount loopcount))

(defn make-sound-desc-build-target [owner-resource-node-id sound-desc-resource sound-desc dep-build-targets]
  {:pre [(map? sound-desc)]} ; Sound$SoundDesc in map format.
  (pipeline/make-protobuf-build-target owner-resource-node-id sound-desc-resource Sound$SoundDesc sound-desc dep-build-targets))

(g/defnk produce-build-targets
  [_node-id resource sound dep-build-targets save-value]
  (or (validation/prop-error :fatal _node-id :sound validation/prop-nil? sound "Sound")
      (validation/prop-error :fatal _node-id :sound validation/prop-nil? (seq dep-build-targets) "Sound")
      [(make-sound-desc-build-target _node-id resource save-value dep-build-targets)]))

(defn load-sound [_project self resource sound-desc]
  {:pre [(map? sound-desc)]} ; Sound$SoundDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (gu/set-properties-from-pb-map self Sound$SoundDesc sound-desc
      sound (resolve-resource :sound)
      looping (protobuf/int->boolean :looping)
      group :group
      gain :gain
      pan :pan
      speed :speed
      loopcount :loopcount)))

(def prop-sound_speed? (partial validation/prop-outside-range? [0.1 5.0]))

(g/defnode SoundNode
  (inherits resource-node/ResourceNode)

  (input dep-build-targets g/Any)
  (input sound-resource resource/Resource)

  (property sound resource/Resource ; Required protobuf field.
            (value (gu/passthrough sound-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :sound-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id sound]
                                  (or (validation/prop-error :info _node-id :sound validation/prop-nil? sound "Sound")
                                      (validation/prop-error :fatal _node-id :sound validation/prop-resource-not-exists? sound "Sound"))))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext supported-audio-formats})))
  (property looping g/Bool (default (protobuf/int->boolean (protobuf/default Sound$SoundDesc :looping))))
  (property loopcount g/Int (default (protobuf/default Sound$SoundDesc :loopcount))
            (value (g/fnk [looping loopcount]
                     (if (not looping) 0 loopcount)))
            (dynamic error (g/fnk [_node-id loopcount]
                             (validation/prop-error :fatal _node-id :loopcount (partial validation/prop-outside-range? [0 127]) loopcount "Loopcount" )))
            (dynamic read-only? (g/fnk [looping]
                                  (not looping))))
  (property group g/Str (default (protobuf/default Sound$SoundDesc :group)))
  (property gain g/Num (default (protobuf/default Sound$SoundDesc :gain))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? gain)))
  (property pan g/Num (default (protobuf/default Sound$SoundDesc :pan))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-1-1? pan)))
  (property speed g/Num (default (protobuf/default Sound$SoundDesc :speed))
            (dynamic error (validation/prop-error-fnk :fatal prop-sound_speed? speed)))

  (output form-data g/Any :cached produce-form-data)
  (output node-outline outline/OutlineData :cached produce-outline-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :ext "sound"
      :node-type SoundNode
      :ddf-type Sound$SoundDesc
      :read-defaults false
      :load-fn load-sound
      :icon sound-icon
      :view-types [:cljfx-form-view :text]
      :view-opts {}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{}}}
      :label "Sound")
    (for [format supported-audio-formats]
      (workspace/register-resource-type workspace
        :ext format
        :node-type SoundSourceNode
        :icon sound-icon
        :view-types [:default]
        :tags #{:embeddable}))))
