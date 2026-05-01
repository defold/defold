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

(ns util.profiler-test
  (:require [clojure.test :refer :all]
            [util.profiler :as profiler]
            [clojure.data.json :as json])
  (:import [com.defold.util Profiler]))

(defn- clear! []
  (Profiler/reset))

(defn- dump []
  (->> (Profiler/dumpJson)
    json/read-str
    (mapv (fn [e] (into {} (map (fn [[k v]] [(keyword k) v]) e))))))

(defn- sleep []
  (try
    (Thread/sleep 1)
    (catch InterruptedException e)))

(deftest threads []
  (clear!)
  (profiler/profile "outer" -1
                    (sleep)
                    (doall
                      (pcalls
                        (fn []
                          (profiler/profile "inner" -1
                                            (sleep))))))
  (let [data (into {} (map (fn [e] [(:name e) e]) (dump)))
        outer (get data "outer")
        inner (get data "inner")]
    (is (not= (:thread outer) (:thread inner)))
    (is (< (:start outer) (:start inner)))
    (is (< (:end inner) (:end outer)))))

(threads)
