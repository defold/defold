(ns benchmark.graph-benchmark
  (:require [clojure.test :refer :all]
            [criterium.core :as cc]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.generator :as ggen]
            [internal.system :as isys]
            [dynamo.graph.test-support :refer :all]
            [dynamo.types :as t]
            [clojure.test.check.generators :as gen]))


(def build-network
  (let [seed 21323424
        seeded-random (fn [] (java.util.Random. seed))]
    (with-redefs [gen/random seeded-random]
      (with-clean-system
        (ggen/make-random-graph-builder)))))

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

(defn network-creation []
  (do-benchmark "Whole Graph Network Creation"
                (with-clean-system
                  (build-network))))

(defn add-one-node []
  (println "Benching: Adding one node")
  (with-clean-system
    (build-network)
    (do-benchmark "Add One Node"
                  (g/transact (g/make-node world AThing)))))

(defn add-one-node-delete-one-node []
  (with-clean-system
    (build-network)
    (do-benchmark "Add One Node and Delete One Node"
                  (let [[new-node] (g/tx-nodes-added (g/transact (g/make-node world AThing)))]
                    (g/transact (g/delete-node new-node))))))

(defn set-property-some-nodes []
  (with-clean-system
    (let [r              (build-network)
          affected-num    100
          chosen-node-ids (repeatedly affected-num (partial rand-nth (ig/node-ids r)))
          chosen-nodes    (mapv #(g/node-by-id (g/now) %) chosen-node-ids)]
      (do-benchmark  (str "Set Property on " affected-num " Nodes")
                     (doseq [chosen-node chosen-nodes]
                       (g/transact (g/set-property chosen-node :a-property "foo")))))))

(defn add-two-nodes-and-connect-them []
  (with-clean-system
    (build-network)
    (do-benchmark "Add Two Nodes and Connect Them"
                  (let [txn-results (g/transact [(g/make-node world AThing)
                                                 (g/make-node world Container)])
                        [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
                    (g/transact (g/connect new-input-node :a-property new-output-node :nodes))))))

(defn add-two-nodes-and-connect-and-disconnect-them []
  (with-clean-system
    (build-network)
    (do-benchmark "Add Two Nodes Connect and Disconnect Them"
     (let [txn-results (g/transact [(g/make-node world AThing)
                                    (g/make-node world Container)])
           [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
       (g/transact (g/connect new-input-node :a-property new-output-node :nodes))
       (g/transact (g/disconnect new-input-node :a-property new-output-node :nodes))))))


(defn run-many-transactions []
  (println "=======")
  (println)
  (doseq [num-trans (take 3 (iterate #(* 10 %) 5000))]
    (let [bench-results (do-benchmark (format "Run %s transactions of adding a node" num-trans)
                                      (with-clean-system
                                        (doseq [i (range num-trans)]
                                          (g/transact (g/make-node world AThing)))))
          mean (first (:mean bench-results))]
      (when mean
        (println (str "TPS Report with " num-trans " txns: " (/ num-trans mean) " transactions per second")))))
  (println)
  (println "======="))

(defn run-benchmarks []
  (network-creation)
  (add-one-node)
  (add-one-node-delete-one-node)
  (set-property-some-nodes)
  (add-two-nodes-and-connect-them)
  (add-two-nodes-and-connect-and-disconnect-them)
  (run-many-transactions))

(defn -main [& args]
  (println "Running benchmarks and outputing results to ./test/benchmark/bench-results.txt")
  (with-open [w (clojure.java.io/writer "./test/benchmark/bench-results.txt")]
    (binding [*out* w]
      (run-benchmarks))))

