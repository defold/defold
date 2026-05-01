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

(ns util.net
  (:require [clojure.java.io :as io])
  (:import [java.net HttpURLConnection]))

(set! *warn-on-reflection* true)

(def ^:const ^:private default-read-timeout 2000)

(def ^:const ^:private default-connect-timeout 2000)

(def ^:private default-cancelled-derefable (delay false))

(defn- default-progress-callback [current total])

(defn download! [url out & {:keys [read-timeout connect-timeout chunk-size progress-callback cancelled-derefable]
                            :or {read-timeout default-read-timeout
                                 connect-timeout default-connect-timeout
                                 chunk-size 0
                                 cancelled-derefable default-cancelled-derefable
                                 progress-callback default-progress-callback}
                            :as args}]
  (let [^HttpURLConnection conn (doto (.openConnection (io/as-url url))
                                  (.setRequestProperty "Connection" "close")
                                  (.setConnectTimeout connect-timeout)
                                  (.setReadTimeout read-timeout))
        length (.getContentLengthLong conn)
        response-code (.getResponseCode conn)]
    (case response-code
      301 (let [location (.getHeaderField conn "Location")]
            (apply download! location out (mapcat identity args)))
      200 (when-not @cancelled-derefable
            (with-open [input (.getInputStream conn)
                        output (io/output-stream out)]
              (if (pos? chunk-size)
                (let [buf (byte-array chunk-size)]
                  (loop [count (.read input buf)
                         previous 0]
                    (when (and (<= 0 count) (not @cancelled-derefable))
                      (.write output buf 0 count)
                      (progress-callback (+ previous count) length)
                      (recur (.read input buf) (+ previous count)))))
                (do
                  ;; io/copy will use an internal buffer of 1024 bytes for transfer
                  (io/copy input output)
                  (when (not @cancelled-derefable)
                    (progress-callback length length)))))))))
