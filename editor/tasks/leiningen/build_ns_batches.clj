(ns leiningen.build-ns-batches)

(load-file "src/clj/editor/ns_batch_builder.clj")
(require '[editor.ns-batch-builder :as bb])

(defn build-ns-batches
  [project & rest]
  (bb/spit-batches "src/clj" "resources/sorted_clojure_ns_list.edn"))
