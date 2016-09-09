(ns editor.system)

(set! *warn-on-reflection* true)

(defn os-name
  ^String []
  (System/getProperty "os.name"))

(defn os-version
  ^String []
  (System/getProperty "os.version"))

(defn defold-version
  ^String []
  (System/getProperty "defold.version"))

(defn defold-sha1
  ^String []
  (System/getProperty "defold.sha1"))

(defonce mac? (-> (os-name)
                  (.toLowerCase)
                  (.indexOf "mac")
                  (>= 0)))
