(ns benchmark.graph-benchmark
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [clojure.test.check.generators :as gen]
            [clojure.test :refer :all]
            [criterium.core :as cc]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer :all]
            [dynamo.types :as t]
            [editor.core :as core]
            [integration.test-util :as test-util]
            [internal.graph :as ig]
            [internal.graph.generator :as ggen]
            [internal.system :as isys]))


(defn load-test-project!
  [world]
  (let [workspace            (test-util/setup-workspace! world)
        project              (test-util/setup-project! workspace)
        project-graph        (g/node->graph-id project)
        app-view             (test-util/setup-app-view!)
        atlas-node           (test-util/resource-node project "/switcher/fish.atlas")
        view                 (test-util/open-scene-view! project app-view atlas-node 128 128)]
    {:project-graph project-graph
     :app-view      app-view
     :resource-node atlas-node
     :resource-view view}))

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

(g/defnode FWorkspace
  (inherits core/Scope)

  (input children t/Any))

(g/defnode FResource
  (property path t/Int)

  (output contents t/Int :cached (g/fnk [path] path)))

(g/defnode FEditable
  (input alpha t/Int)
  (input beta  t/Int)
  (input gamma t/Int)

  (output omega t/Int :cached (g/fnk [alpha beta gamma] (+ alpha beta gamma)))
  (output epsilon t/Int :cached (g/fnk [omega] (inc omega))))

(g/defnode FView
  (input omega t/Int)
  (input view t/Int)

  (output scene t/Int :cached (g/fnk [omega view] (+ omega view))))

(defn pile-of-nodes [where tp n] (tx-nodes (repeatedly (int n) #(g/make-node where tp))))
(defn connection-targets [how-many from] (partition how-many (repeatedly #(rand-nth from))))

(defn build-fake-graphs!
  [resource-count view-node-count]
  (let [project-graph  (g/make-graph! :history true :volatility 1)
        view-graph     (g/make-graph! :history false :volatility 10)
        workspace      (first (tx-nodes (g/make-node project-graph FWorkspace)))
        bottom-layer   (pile-of-nodes project-graph FResource resource-count)
        middle-layer   (pile-of-nodes project-graph FEditable resource-count)
        top-layer      (pile-of-nodes project-graph FEditable (bit-shift-right resource-count 2))
        view-layer     (pile-of-nodes view-graph    FView     view-node-count)]

    (g/transact
     [(for [b bottom-layer] (g/connect b :self workspace :children))
      (for [m middle-layer] (g/connect m :self workspace :children))
      (for [t top-layer]    (g/connect t :self workspace :children))])

    (g/transact
     (mapcat #(g/set-property % :path (rand-int 10000)) bottom-layer))

    (g/transact
     (mapcat (fn [m [a b c]]
               [(g/connect a :contents m :alpha)
                (g/connect b :contents m :beta)
                (g/connect c :contents m :gamma)])
             middle-layer (connection-targets 3 bottom-layer)))

    (g/transact
     (mapcat (fn [t [o e o2]]
               [(g/connect o :omega   t :alpha)
                (g/connect e :epsilon t :beta)
                (g/connect e :omega   t :gamma)])
             top-layer (connection-targets 3 middle-layer)))

    (g/transact
     (mapcat (fn [v [o e]]
               [(g/connect o :omega    v :omega)
                (g/connect e :epsilon  v :view)])
             view-layer (connection-targets 2 top-layer)))

    [bottom-layer view-layer]))

(g/defnode AThing
  (property a-property t/Str))

(g/defnode Container
  (input nodes t/Any))

(defn network-creation []
  (do-benchmark "Whole Graph Network Creation"
                (with-clean-system
                  (load-test-project! world))))

(defn add-one-node []
  (println "Benching: Adding one node")
  (with-clean-system
    (load-test-project! world)
    (do-benchmark "Add One Node"
                  (g/transact (g/make-node world AThing)))))

(defn add-one-node-delete-one-node []
  (with-clean-system
    (load-test-project! world)
    (do-benchmark "Add One Node and Delete One Node"
                  (let [[new-node] (g/tx-nodes-added (g/transact (g/make-node world AThing)))]
                    (g/transact (g/delete-node new-node))))))

(defn set-property-some-nodes []
  (with-clean-system
    (let [r              (load-test-project! world)
          project-graph (isys/graph @g/*the-system* (:project-graph r))
          affected-num    100
          chosen-node-ids (repeatedly affected-num (partial rand-nth (ig/node-ids project-graph)))
          chosen-nodes    (map #(ig/node project-graph %) chosen-node-ids)
          chosen-props    (map (fn [node] (rand-nth (vec (disj (into #{} (keys node)) :_id)))) chosen-nodes)]
      (do-benchmark  (str "Set Property on " affected-num " Nodes")
                     (doseq [chosen-node chosen-nodes]
                       (g/transact (g/set-property chosen-node :a-property nil)))))))

(defn add-two-nodes-and-connect-them []
  (with-clean-system
    (load-test-project! world)
    (do-benchmark "Add Two Nodes and Connect Them"
                  (let [txn-results (g/transact [(g/make-node world AThing)
                                                 (g/make-node world Container)])
                        [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
                    (g/transact (g/connect new-input-node :a-property new-output-node :nodes))))))

(defn add-two-nodes-and-connect-and-disconnect-them []
  (with-clean-system
    (load-test-project! world)
    (do-benchmark "Add Two Nodes Connect and Disconnect Them"
     (let [txn-results (g/transact [(g/make-node world AThing)
                                    (g/make-node world Container)])
           [new-input-node new-output-node] (g/tx-nodes-added txn-results)]
       (g/transact (g/connect new-input-node :a-property new-output-node :nodes))
       (g/transact (g/disconnect new-input-node :a-property new-output-node :nodes))))))

(defn tally-make []
  (agent (long-array 2)))

(defn tally-count [^longs tally nanos]
  (aset tally 0 (inc (aget tally 0)))
  (aset tally 1 (+ nanos (aget tally 1)))
  tally)

(defn tally-report [agt]
  (let [reps (aget @agt 0)
        total (aget @agt 1)]
    (when (not= 0 reps)
      (double (/ total reps)))))

(defmacro recording-time [agt & body]
  `(let [start# (. System (nanoTime))
         ret# ~@body]
     (send-off ~agt tally-count (- (. System (nanoTime)) start#))
     ret#))

(defn- run-transactions-for-tracing
  [actions pulls transaction-time evaluation-time]
  (loop [i       0
         actions actions
         pulls   pulls]
    (when (seq actions)
      (let [[node property value] (first actions)]
        (recording-time transaction-time
                        (g/transact
                         (g/set-property node property value)))

        (if (= 0 (mod i 11))
          (let [[view output] (first pulls)]
            (recording-time evaluation-time
                            (g/node-value view output))
            (recur (inc i) (next actions) (next pulls)))
          (recur (inc i) (next actions) pulls))))))

(defn do-transactions-for-tracing [n]
  (with-clean-system
    (let [transaction-time  (tally-make)
          evaluation-time   (tally-make)
          [resources views] (build-fake-graphs! 1000 100)
          actions           (map (fn [& a] a)
                                 (repeatedly n #(g/node-id (rand-nth resources)))
                                 (repeatedly n #(rand-nth [:alpha :beta :gamma]))
                                 (repeatedly n #(rand-int 10000)))
          pulls             (map (fn [& a] a)
                                 (repeatedly #(g/node-id (rand-nth views)))
                                 (repeat :scene))]
      (run-transactions-for-tracing actions pulls transaction-time evaluation-time)
      (println :transaction-tally (tally-report transaction-time)
               :evaluation-tally  (tally-report evaluation-time)))))

(defn do-transactions [n]
  (with-clean-system
    (let [transaction-time  (tally-make)
          evaluation-time   (tally-make)
          [resources views] (build-fake-graphs! 1000 100)
          actions           (map (fn [& a] a)
                                 (repeatedly n #(g/node-id (rand-nth resources)))
                                 (repeatedly n #(rand-nth [:alpha :beta :gamma]))
                                 (repeatedly n #(rand-int 10000)))
          pulls             (map (fn [& a] a)
                                 (repeatedly #(g/node-id (rand-nth views)))
                                 (repeat :scene))]
      (assoc
       (do-benchmark
        (format "Run %s transactions of setting a property and pulling values" n)
        (loop [i       0
               actions actions
               pulls   pulls]
          (when (seq actions)
            (let [[node property value] (first actions)]
              (recording-time transaction-time
                              (g/transact
                               (g/set-property node property value)))

              (if (= 0 (mod i 11))
                (let [[view output] (first pulls)]
                  (recording-time evaluation-time
                                  (g/node-value view output))
                  (recur (inc i) (next actions) (next pulls)))
                (recur (inc i) (next actions) pulls))))))
       :transaction-tally (tally-report transaction-time)
       :evaluation-tally  (tally-report evaluation-time)))))

(defn run-many-transactions []
  (println "=======")
  (println)
  (doseq [num-trans (take 3 (iterate #(* 10 %) 500))]
    (let [bench-results (do-transactions num-trans)
          mean (first (:mean bench-results))]
      (when mean
        (println (str "TPS Report with " num-trans " txns: " (/ num-trans mean) " transactions per second"))
        (println (str "The average transaction took " (/ (:transaction-tally bench-results) 1000000) " ms"))
        (println (str "The average evaluation took " (/ (:evaluation-tally bench-results) 1000000) " ms")))))
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
