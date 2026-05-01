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

(ns editor.ns-batch-builder
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.set :as set]
            [clojure.tools.namespace.dependency :as ns-deps]
            [clojure.tools.namespace.find :as ns-find]
            [clojure.tools.namespace.parse :as ns-parse])
  (:import [java.io File]))

(defn- add-deps [graph [sym deps]]
  (reduce (fn [g [sym dep]] (ns-deps/depend g sym dep))
          graph
          (for [dep deps] [sym dep])))

(defn- make-namespace-deps-graph []
  (let [namespaces (ns-find/find-ns-decls
                     (mapv io/file (.split (System/getProperty "java.class.path") File/pathSeparator)))
        own-nses (into #{} (map second) namespaces)]
    (reduce add-deps
            (ns-deps/graph)
            (for [ns namespaces]
              [(second ns) (set/intersection own-nses (ns-parse/deps-from-ns-decl ns))]))))

(defn- make-load-batches-for-ns [graph ns-sym available]
  (let [deps (set/difference
               (conj (ns-deps/transitive-dependencies graph ns-sym) ns-sym)
               available)
        nodes-with-deps (for [n deps]
                          [n (ns-deps/transitive-dependencies graph n)])]
    (loop [available available
           remaining nodes-with-deps
           batches []]
      (if (empty? remaining)
        [batches deps]
        (let [next-batch (into #{}
                               (keep (fn [[node deps]]
                                       (when (empty? (set/difference deps available))
                                         node)))
                               remaining)]
          (recur (set/union available next-batch)
                 (filter (fn [[node _deps]] (not (next-batch node))) remaining)
                 (conj batches next-batch)))))))

(defn spit-batches
  "Writes a vector of batches (vectors of symbols) of Clojure namespaces of the
  current classpath in edn format to a file, for later consumption by the
  bootloader.
  Also verifies that the file was correctly written.

  to - an edn file where the batches will be written."
  [to]
  (let [graph (make-namespace-deps-graph)
        ;; Make two sets of batches. One to load editor.boot as fast as possible
        ;; so we can show the progress dialog. And another batch to keep loading
        ;; the dependencies for editor.boot-open-project, excluding the
        ;; dependencies already loaded from loading editor.boot.
        [boot-batches available] (make-load-batches-for-ns graph 'editor.boot #{})
        [boot-open-project-batches _] (make-load-batches-for-ns graph 'editor.boot-open-project available)
        batches (into []
                      (map #(vec (sort %))) ; Sort entries inside batches so load order is consistent.
                      (concat boot-batches boot-open-project-batches))]
    (spit to (with-out-str (pprint/pprint batches)))
    (when (not= batches (edn/read-string (slurp to)))
      (throw (Exception. (format "Batch file %s was not correctly generated." to))))))

(def -main spit-batches)
