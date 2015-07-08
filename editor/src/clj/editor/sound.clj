(ns editor.sound
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [editor.project :as project])
  (:import [java.io ByteArrayOutputStream]
           [org.apache.commons.io IOUtils]))

(def sound-defs [{:ext "wav"
                  :icon "icons/pictures.png"}
                 {:ext "ogg"
                  :icon "icons/pictures.png"}])

(defn- build-sound [self basis resource dep-resources user-data]
  (let [source (:resource resource)
        in (io/input-stream source)
        out (ByteArrayOutputStream.)
        _ (IOUtils/copy in out)
        content (.toByteArray out)]
    {:resource resource :content content}))

(g/defnk produce-build-targets [node-id resource]
  [{:node-id node-id
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
