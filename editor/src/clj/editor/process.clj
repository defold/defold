;; Copyright 2020 The Defold Foundation
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

(ns editor.process
  (:require [clojure.java.io :as io])
  (:import [java.lang Process ProcessBuilder]))

(set! *warn-on-reflection* true)

(defn start! ^Process [^String command args {:keys [directory env redirect-error-stream?]
                                             :or {redirect-error-stream? false}
                                             :as opts}]
  (let [pb (ProcessBuilder. ^"[Ljava.lang.String;" (into-array String (list* command args)))]
    (when directory
      (.directory pb directory))
    (when env
      (let [environment (.environment pb)]
        (doseq [[k v] env]
          (.put environment k v))))
    (when (some? redirect-error-stream?)
      (.redirectErrorStream pb (boolean redirect-error-stream?)))
    (.start pb)))

(defn watchdog! ^Thread [^Process proc on-exit]
  (doto (Thread. (fn []
                   (.waitFor proc)
                   (on-exit)))
    (.start)))
