;; Copyright 2020-2026 The Defold Foundation
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

(defn defold-archive-domain
  ^String []
  (or (System/getProperty "defold.archive.domain")
      "d.defold.com"))

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
  ;; Having this call here mainly benefits repl-interactions, as the editor and
  ;; tests will explicitly call unpackResources at startup.
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
