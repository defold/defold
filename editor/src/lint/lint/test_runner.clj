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

(ns lint.test-runner
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :as test])
  (:import [java.io File]))

(set! *warn-on-reflection* true)

(defn- test-namespaces
  "Find test namespaces recursively under directory in name order."
  [directory]
  (let [root (.getCanonicalFile (io/file directory))]
    (loop [files [root]
           namespaces []]
      (if (zero? (count files))
        (vec (sort namespaces))
        (let [^File file (peek files)]
          (if (.isDirectory file)
            (recur (into (pop files) (.listFiles file)) namespaces)
            (recur (pop files)
                   (cond-> namespaces
                     (string/ends-with? (.getName file) "_test.clj")
                     (conj (-> (str (.relativize (.toPath root) (.toPath file)))
                               (string/replace File/separator ".")
                               (string/replace "_" "-")
                               (string/replace #"\.clj$" "")
                               symbol))))))))))

(defn run-tests
  "Load and run tests from directory, returning zero only when they pass."
  [directory]
  (let [namespaces (test-namespaces directory)]
    (when (zero? (count namespaces))
      (throw (ex-info (str "No lint tests found in " directory) {})))
    (doseq [namespace namespaces]
      (require namespace))
    (if (test/successful? (apply test/run-tests namespaces)) 0 1)))
