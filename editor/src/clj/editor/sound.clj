(ns editor.sound
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [editor.defold-project :as project])
  (:import [java.io ByteArrayOutputStream]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def sound-defs [{:ext "wav"
                  :icon "icons/32/Icons_26-AT-Sound.png"}
                 {:ext "ogg"
                  :icon "icons/32/Icons_26-AT-Sound.png"}])

(defn- build-sound [self basis resource dep-resources user-data]
  (let [source (:resource resource)
        in (io/input-stream source)
        out (ByteArrayOutputStream.)
        _ (IOUtils/copy in out)
        content (.toByteArray out)]
    {:resource resource :content content}))

(g/defnk produce-build-targets [_node-id resource]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-sound}])

(g/defnode SoundSourceNode
  (inherits project/ResourceNode)

  (output build-targets g/Any produce-build-targets))

(defn- register [workspace def]
  (workspace/register-resource-type workspace
                                   :ext (:ext def)
                                   :node-type SoundSourceNode
                                   :icon (:icon def)))

(defn register-resource-types [workspace]
  (for [def sound-defs]
    (register workspace def)))
