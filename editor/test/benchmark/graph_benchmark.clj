(ns benchmark.graph-benchmark
  (:require [clojure.test :refer :all]
            [criterium.core :as cc]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.generator :as ggen]
            [internal.system :as isys]
            [dynamo.graph.test-support :refer :all]
            [dynamo.types :as t]))


;;;Want to use non-random graphs do that we have a non-moving target
;; for subsequent benchmarks

(def nodes-num 10)

(defmacro do-benchmark [benchmark-name f]
;;; For more rigorous use benchmark instead of quick-benchmark
  `(do
     (println "***************************************")
     (println)
     (println)
     (println ~benchmark-name)
     (println)
     (println)
     (println "***************************************")
     (cc/with-progress-reporting
       (let [bresults# (cc/quick-benchmark ~f {})]
         (cc/report-result bresults# :os)
         bresults#))))

(g/defnode AThing
  (property a-property t/Str))

(g/defnode Container
  (input nodes t/Any))

(defn build-network
  [world]
  (let [nodes-num          10
        tx-result          (g/transact (repeatedly nodes-num  #(g/make-node world AThing)))
        named-nodes        (g/tx-nodes-added tx-result)
        named-node-ids     (map g/node-id named-nodes)
        tx-result          (g/transact (repeatedly nodes-num  #(g/make-node world Container)))
        container-nodes    (g/tx-nodes-added tx-result)
        container-node-ids (map g/node-id container-nodes)
        tx-result          (g/transact (map #(g/connect %1 :a-property %2 :nodes) named-nodes container-nodes))]
   {:tx-result tx-result
    :named-node-ids named-node-ids
    :container-node-ids container-node-ids}))


(defn network-creation []
  (do-benchmark "Whole Graph Network Creation"
                (with-clean-system
                  (build-network world))))

(defn add-one-node []
  (println "Benching: Adding one node")
  (with-clean-system
    (build-network world)
    (do-benchmark "Add One Node"
                  (g/transact (g/make-node world AThing)))))

(defn add-one-node-delete-one-node []
  (with-clean-system
    (build-network world)
    (do-benchmark "Add One Node and Delete One Node"
                  (let [[new-node] (g/tx-nodes-added (g/transact (g/make-node world AThing)))]
                    (g/transact (g/delete-node new-node))))))

(defn set-property-one-node []
  (with-clean-system
    (let [r              (build-network world)
          basis          (get-in r [:tx-result :basis])
          chosen-node-id (first (:named-node-ids r))
          chosen-node    (g/node-by-id basis chosen-node-id)]
      (do-benchmark  "Set Property on One Node"
                     (g/transact (g/set-property chosen-node :a-property "foo"))))))

(defn add-two-nodes-and-connect-them []
  (with-clean-system
    (build-network world)
    (do-benchmark "Add Two Nodes and Connect Them"
                  (let [txn-results (g/transact [(g/make-node world AThing)
                                                 (g/make-node world Container)])
                        [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
                    (g/transact (g/connect new-input-node :a-property new-output-node :nodes))))))

(defn add-two-nodes-and-connect-and-disconnect-them []
  (with-clean-system
    (build-network world)
    (do-benchmark "Add Two Nodes Connect and Disconnect Them"
     (let [txn-results (g/transact [(g/make-node world AThing)
                                    (g/make-node world Container)])
           [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
       (g/transact (g/connect new-input-node :a-property new-output-node :nodes))
       (g/transact (g/disconnect new-input-node :a-property new-output-node :nodes))))))


(defn run-many-transactions []
  (let [num-trans 50000
        bench-results (do-benchmark (format "Run %s transactions of adding a node" num-trans)
                                    (with-clean-system
                                      (doseq [i (range num-trans)]
                                        (g/transact (g/make-node world AThing)))))
        mean (first (:mean bench-results))]
    (when mean
      (println "=======")
      (println)
      (println (str "TPS Report: " (/ num-trans mean) " transactions per second"))
      (println)
      (println "======="))))

(defn run-benchmarks []
  (network-creation)
  (add-one-node)
  (add-one-node-delete-one-node)
  (set-property-one-node)
  (add-two-nodes-and-connect-them)
  (add-two-nodes-and-connect-and-disconnect-them)
  (run-many-transactions))

(defn -main [& args]
  (println "Running benchmarks and outputing results to ./test/benchmark/bench-results.txt")
  (with-open [w (clojure.java.io/writer "./test/benchmark/bench-results.txt")]
    (binding [*out* w]
      (run-benchmarks))))

