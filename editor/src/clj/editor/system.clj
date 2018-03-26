(ns editor.system
  (:require [clojure.java.io :as io]))

(set! *warn-on-reflection* true)

(def ^:private MB 1048576)

(defn rt-mem []
  (let [rt    (Runtime/getRuntime)
        total (.totalMemory rt)
        max   (.maxMemory rt)
        free  (.freeMemory rt)]
    {:total (int (/ total MB))
     :max   (int (/ max MB))
     :free  (int (/ free MB))
     :used  (int (/ (- total free) MB))}))

(defn mem-diff [before after]
  {:total (- (:total after) (:total before))
   :max   (- (:max after) (:max before))
   :free  (- (:free after) (:free before))
   :used  (- (:used after) (:used before))})

(defn os-name
  ^String []
  (System/getProperty "os.name"))

(defn os-arch
  ^String []
  (System/getProperty "os.arch"))

(defn os-version
  ^String []
  (System/getProperty "os.version"))

(defn defold-version
  ^String []
  (System/getProperty "defold.version"))

(defn defold-channel
  ^String []
  (System/getProperty "defold.channel"))

(defn defold-resourcespath
  ^String []
  (System/getProperty "defold.resourcespath"))

(defn defold-editor-sha1
  ^String []
  (System/getProperty "defold.editor.sha1"))

(defn defold-engine-sha1
  ^String []
  (System/getProperty "defold.engine.sha1"))

(defn defold-build-time
  ^String []
  (System/getProperty "defold.buildtime"))

(defn defold-dev? []
  (or (some? (System/getProperty "defold.dev"))
      (not (defold-version))))

(defn defold-unpack-path
  ^String []
  (System/getProperty "defold.unpack.path"))

(defn java-home
  ^String []
  (System/getProperty "java.home"))

(defn user-home
  ^String []
  (System/getProperty "user.home"))

(defn java-runtime-version
  ^String []
  (System/getProperty "java.runtime.version"))

(defonce mac? (-> (os-name)
                  (.toLowerCase)
                  (.indexOf "mac")
                  (>= 0)))
