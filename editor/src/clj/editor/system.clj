(ns editor.system
  (:import [com.defold.libs ResourceUnpacker]))

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

(defn defold-launcherpath
  ^String []
  (System/getProperty "defold.launcherpath"))

(defn defold-editor-sha1
  ^String []
  (System/getProperty "defold.editor.sha1"))

(defn set-defold-editor-sha1! [^String sha1]
  (assert (not-empty sha1))
  (System/setProperty "defold.editor.sha1" sha1))

(defn defold-engine-sha1
  ^String []
  (System/getProperty "defold.engine.sha1"))

(defn set-defold-engine-sha1! [^String sha1]
  (assert (not-empty sha1))
  (System/setProperty "defold.engine.sha1" sha1))

(defn defold-build-time
  ^String []
  (System/getProperty "defold.buildtime"))

(defn defold-dev? []
  (or (some? (System/getProperty "defold.dev"))
      (not (defold-version))))

(defn defold-unpack-path
  ^String []
  ;; This call ensures we have unpacked all the required libraries and binaries
  ;; so that they are ready to use. It contains a check so that it will only do
  ;; this the first time the method is called. It is safe to call from any
  ;; thead, but will block until the unpacking thread has completed.
  ;;
  ;; Having this call here mainly benefits the tests and repl-interactions, as
  ;; the editor will also explicitly call unpackResources at startup.
  (ResourceUnpacker/unpackResources)
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
