(ns editor.system)

(set! *warn-on-reflection* true)

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

(defn defold-sha1
  ^String []
  (System/getProperty "defold.sha1"))

(defn defold-dev? []
  (not (defold-version)))

(defn java-home
  ^String []
  (System/getProperty "java.home"))

(defn java-runtime-version
  ^String []
  (System/getProperty "java.runtime.version"))

(defonce mac? (-> (os-name)
                  (.toLowerCase)
                  (.indexOf "mac")
                  (>= 0)))
