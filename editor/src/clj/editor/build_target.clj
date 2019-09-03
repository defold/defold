(ns editor.build-target
  (:require [editor.resource :as resource]
            [editor.system :as system]
            [util.digestable :as digestable]))

(set! *warn-on-reflection* true)

(defn content-hash-components [build-target]
  [(system/defold-engine-sha1)
   (:resource (:resource build-target))
   (:build-fn build-target)
   (:user-data build-target)])

(defn content-hash
  ^String [build-target]
  (let [content-hash-components (content-hash-components build-target)]
    (try
      (digestable/sha1-hash content-hash-components)
      (catch Throwable error
        (throw (ex-info (str "Failed to digest content for resource: " (resource/proj-path (:resource build-target)))
                        {:build-target build-target
                         :content-hash-components content-hash-components}
                        error))))))

(def content-hash? digestable/sha1-hash?)

(defn with-content-hash [build-target]
  (assoc build-target :content-hash (content-hash build-target)))
