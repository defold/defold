(ns editor.sound
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [java.io ByteArrayOutputStream]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(defmacro spy
  [& body]
  `(let [ret# (try ~@body (catch Throwable t# (prn t#) (throw t#)))]
     (prn ret#)
     ret#))

(def sound-icon "icons/32/Icons_26-AT-Sound.png")

(def supported-audio-formats #{"wav" "ogg"})

(defn- build-sound-source
  [self basis resource dep-resources user-data]
  (let [source (:resource resource)
        in (io/input-stream source)
        out (ByteArrayOutputStream.)
        _ (IOUtils/copy in out)
        content (.toByteArray out)]
    {:resource resource :content content}))

(g/defnk produce-source-build-targets [_node-id resource]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-sound-source}])

(g/defnode SoundSourceNode
  (inherits project/ResourceNode)

  (output build-targets g/Any produce-source-build-targets))

;;--------------------------------------------------------------------

(g/defnk produce-outline-data
  [_node-id]
  {:node-id _node-id
   :label "Sound"
   :icon sound-icon})

(g/defnk produce-form-data
  [_node-id sound looping group gain]
  {:form-ops {:user-data {}
              :set (fn [v [property] val]
                     (g/set-property! _node-id property val))
              :clear (fn [property]
                       (g/clear-property! _node-id property))}
   :sections [{:title "Sound"
               :fields [{:path [:sound]
                         :label "Sound"
                         :type :resource
                         :filter supported-audio-formats}
                        {:path [:looping]
                         :label "Loop"
                         :type :boolean}
                        {:path [:group]
                         :label "Group"
                         :type :number}
                        {:path [:gain]
                         :label "Gain"
                         :type :number}]}]
   :values {[:sound] (resource/resource->proj-path sound)
            [:looping] looping
            [:group] group
            [:gain] gain}})

(g/defnk produce-pb-msg
  [_node-id sound-resource looping group gain]
  {:sound (resource/resource->proj-path sound-resource)
   :looping (if looping 1 0)
   :group group
   :gain gain})

(g/defnk produce-save-data
  [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Sound$SoundDesc pb-msg)})

(defn build-sound
  [self basis resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes Sound$SoundDesc pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource sound dep-build-targets pb-msg]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-resource (into {} (map (juxt (comp :resource :resource) :resource) dep-build-targets))
        dep-resources (map (fn [[label resource]]
                             [label (get deps-by-resource resource)])
                           [[:sound sound]])]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-sound
      :user-data {:pb-msg pb-msg
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(defn load-sound
  [project self resource]
  (let [sound (protobuf/read-text Sound$SoundDesc resource)]
    (g/set-property self
                    :sound (workspace/resolve-resource resource (:sound sound))
                    :looping (not (zero? (:looping sound)))
                    :group (:group sound)
                    :gain (:gain sound))))

(g/defnode SoundNode
  (inherits project/ResourceNode)

  (input dep-build-targets g/Any)
  (input sound-resource resource/Resource)

  (property sound resource/Resource
            (value (gu/passthrough sound-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :sound-resource]
                                            [:build-targets :dep-build-targets])))
            (validate (validation/validate-resource sound "Missing sound"))
            (dynamic edit-type (g/always {:type resource/Resource :ext supported-audio-formats})))

  (property looping g/Bool (default false))
  (property group g/Str (default "master"))
  (property gain g/Num (default 1.0)
            (validate (g/fnk [gain]
                        (when-not (<= 0.0 gain 1.0)
                          (g/error-warning "Gain must be between 0.0 and 1.0")))))

  (output form-data g/Any :cached produce-form-data)
  (output node-outline outline/OutlineData :cached produce-outline-data)
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (concat
   (workspace/register-resource-type workspace
                                     :ext "sound"
                                     :node-type SoundNode
                                     :load-fn load-sound
                                     :icon sound-icon
                                     :view-types [:form-view :text]
                                     :view-opts {}
                                     :tags #{:component}
                                     :label "Sound")
   (for [format supported-audio-formats]
     (workspace/register-resource-type workspace
                                       :ext format
                                       :node-type SoundSourceNode
                                       :icon sound-icon))))
