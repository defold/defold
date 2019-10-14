(ns editor.ns-batch-builder
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :as pprint]
            [clojure.set :as set]
            [clojure.tools.namespace.dependency :as ns-deps]
            [clojure.tools.namespace.find :as ns-find]
            [clojure.tools.namespace.parse :as ns-parse]))

(defn- add-deps
  [graph [sym deps]]
  (reduce (fn [g [sym dep]] (ns-deps/depend g sym dep))
          graph
          (for [dep deps] [sym dep])))

(defn- make-namespace-deps-graph
  [srcdir]
  (let [namespaces (ns-find/find-ns-decls [(io/file srcdir)])
        own-nses (into #{} (map second) namespaces)]
    (reduce add-deps
            (ns-deps/graph)
            (for [ns namespaces]
              [(second ns) (set/intersection own-nses (ns-parse/deps-from-ns-decl ns))]))))

(defn- make-load-batches-for-ns
  [graph ns-sym available]
  (let [deps (conj (ns-deps/transitive-dependencies graph ns-sym) ns-sym)
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
  "Writes a vector of batches (sets of symbols) in edn format to a file,
  for later consumption by the bootloader.
  Also verifies that the file was correctly written.

  from - a directory containing the editor clojure source files.
  to - an edn file where the batches will be written."
  [from to]
  (let [graph (make-namespace-deps-graph from)
        ;; Make two sets of batches. One to load editor.boot as fast as possible
        ;; so we can show the progress dialog. And another batch to keep loading
        ;; the dependencies for editor.boot-open-project, excluding the
        ;; dependencies already loaded from loading editor.boot.
        [boot-batches available] (make-load-batches-for-ns graph 'editor.boot #{})
        [boot-open-project-batches _] (make-load-batches-for-ns graph 'editor.boot-open-project available)
        batches (into [] (concat boot-batches boot-open-project-batches))]
    (spit to (with-out-str (pprint/pprint batches)))
    (when (not= batches (edn/read-string (slurp to)))
      (throw (Exception. (format "Batch file %s was not correctly generated." to))))))

