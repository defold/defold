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

(defn defold-build-time
  ^String []
  (System/getProperty "defold.buildtime"))

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

(defn exec
  ([args] (exec args {}))
  ([args env] (exec args nil {}))
  ([args dir env] (let [pb (-> (ProcessBuilder. args) (.redirectErrorStream true))]
                    (.putAll (.environment pb) env)
                    (when dir
                      (.directory pb dir))
                    (let [p (.start pb)]
                      (with-open [out (.getInputStream p)]
                        (let [s (slurp out)]
                          [(.waitFor p) s]))))))
