(ns dynamo.system.test-support
  (:require [clojure.core.async :as a]
            [clojure.java.io :as io]
            [com.stuartsierra.component :as component]
            [dynamo.node :as n]
            [dynamo.system :as ds :refer [in]]
            [internal.system :as is]
            [internal.transaction :as it])
  (:import [org.eclipse.core.resources IProject IFile]
           [org.eclipse.core.runtime Path]))

(set! *warn-on-reflection* true)

(defn clean-world
  []
  (let [report-ch (a/chan (a/dropping-buffer 1))
        world     (is/world report-ch (ref #{}))]
    (component/start world)))

(defmacro with-clean-world
  [& forms]
  `(let [~'world     (clean-world)
         ~'world-ref (:state ~'world)
         ~'root      (ds/node ~'world-ref 1)]
     (binding [it/*transaction* (it/->TransactionSeed ~'world-ref)]
       (ds/in ~'root
           ~@forms))))

(defn tx-nodes [& resources]
  (ds/transactional
    (doseq [r resources]
      (ds/add r))
    resources))

(defn await-world-time
  [world-ref desired-time clock-time]
  (let [valch (a/chan 1)
        timer (a/timeout clock-time)]
    (add-watch world-ref :world-time
      (fn [_ _ o n]
        (if (>= (:world-time n) desired-time)
          (a/put! valch n))))
    (first (a/alts!! [valch timer]))))

(defn tempfile
  [prefix suffix auto-delete?]
  (let [f (java.io.File/createTempFile prefix suffix)]
    (when auto-delete?
      (.deleteOnExit f))
    f))

;; These stubs have just enough to make the current test suite pass.
;; You will almost certainly need to enhance them for your own tests.
(defrecord MockIFile [path canned-contents]
  IFile
  (getFullPath [this] (Path. path)))

(defn mock-iproject
  [canned-files]
  (reify IProject
    (^IFile getFile [this ^String path] (->MockIFile (str "IProject/" path) (get canned-files path)))))

#_(defn resource-from-bundle
  [b f]
  (o/with-bundle b
    (io/resource f)))

#_(defn fixture [bundle-name fixture-path]
    (resource-from-bundle (o/get-bundle bundle-name) fixture-path))
