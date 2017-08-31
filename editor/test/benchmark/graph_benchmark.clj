(ns benchmark.graph-benchmark
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [criterium.core :as cc]
            [dynamo.graph :as g]
            [support.test-support :refer :all]
            [editor.core :as core]
            [integration.test-util :as test-util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.system :as isys]))


(defn load-test-project!
  [world]
  (let [[workspace project app-view] (test-util/setup! world)
        project-graph        (g/node-id->graph-id project)
        [atlas-node view]    (test-util/open-scene-view! project app-view "/switcher/fish.atlas" 128 128)]
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

  (input children g/Any))

(g/defnode FResource
  (property path g/Int)

  (output contents g/Int :cached (g/fnk [path] path)))

(g/defnode FEditable
  (input alpha g/Int)
  (input beta  g/Int)
  (input gamma g/Int)

  (output omega g/Int :cached (g/fnk [alpha beta gamma] (+ alpha beta gamma)))
  (output epsilon g/Int :cached (g/fnk [omega] (inc omega))))

(g/defnode FView
  (input omega g/Int)
  (input view g/Int)

  (output scene g/Int :cached (g/fnk [omega view] (+ omega view))))

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
     [(for [b bottom-layer] (g/connect b :_node-id workspace :children))
      (for [m middle-layer] (g/connect m :_node-id workspace :children))
      (for [t top-layer]    (g/connect t :_node-id workspace :children))])

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
  (property a-property g/Str (default "Hey"))
  (output an-output g/Str (g/fnk [] "Boo.")))

(g/defnode Container
  (input nodes g/Any))

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

(defn- safe-rand-nth [coll]
  (when (seq? coll)
    (rand-nth coll)))

(defn set-property-some-nodes []
  (with-clean-system
    (let [r               (load-test-project! world)
          project-graph   (isys/graph @g/*the-system* (:project-graph r))
          affected-num    100
          chosen-node-ids (repeatedly affected-num (partial rand-nth (ig/node-ids project-graph)))
          chosen-props    (mapv (fn [node-id]  (safe-rand-nth (vec (disj (-> node-id g/node-type g/declared-property-labels) :id)))) chosen-node-ids)]
      (str "Set Property on " affected-num " Nodes")
      (do-benchmark (str "Set Property on " affected-num " Nodes")
                    (mapv (fn [node property] (g/set-property node property nil)) chosen-node-ids chosen-props)))))

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

(defn one-node-value []
  (with-clean-system
    (let [txn-results (g/transact [(g/make-node world AThing)])
          [new-input-node] (g/tx-nodes-added txn-results)]
      (g/node-value new-input-node :an-output))))

(defn one-node-value-bench []
  (with-clean-system {:cache-size 0}
    (let [txn-results (g/transact [(g/make-node world AThing)])
          [new-input-node] (g/tx-nodes-added txn-results)]
      (do-benchmark "Pull :an-output"
                    (g/node-value new-input-node :an-output)))))

(defn- run-transactions-for-tracing
  [actions pulls]
  (loop [i       0
         actions actions
         pulls   pulls]
    (when (seq actions)
      (let [[node property value] (first actions)]
        (g/transact
         (g/set-property node property value))

        (if (= 0 (mod i 11))
          (let [[view output] (first pulls)]
            (g/node-value view output)
            (recur (inc i) (next actions) (next pulls)))
          (recur (inc i) (next actions) pulls))))))

(defn do-transactions-for-tracing [n]
  (with-clean-system
    (let [[resources views] (build-fake-graphs! 1000 100)
          actions           (map (fn [& a] a)
                                 (repeatedly n #(rand-nth resources))
                                 (repeat n :path)
                                 (repeat :scene))
          pulls             (map (fn [& a] a)
                                 (repeatedly #(rand-nth views))
                                 (repeatedly #(rand-nth [:view :omega])))]
      (run-transactions-for-tracing actions pulls))))

(defn do-transactions [n]
  (with-clean-system
    (let [[resources views] (build-fake-graphs! 1000 100)
          actions           (map (fn [& a] a)
                                 (repeatedly n #(rand-nth resources))
                                 (repeat n :path)
                                 (repeatedly n #(rand-int 10000)))
          pulls             (map (fn [& a] a)
                                 (repeatedly #(rand-nth views))
                                 (repeat :scene))]
      (do-benchmark
       (format "Run %s transactions of setting a property and pulling values" n)
       (loop [i       0
              actions actions
              pulls   pulls]
         (when (seq actions)
           (let [[node property value] (first actions)]
             (g/transact
              (g/set-property node property value))

             (if (= 0 (mod i 11))
               (let [[view output] (first pulls)]
                 (g/node-value view output)
                 (recur (inc i) (next actions) (next pulls)))
               (recur (inc i) (next actions) pulls)))))))))

(defn run-many-transactions []
  (println "=======")
  (println)
  (doseq [num-trans (take 3 (iterate #(* 10 %) 500))]
    (let [bench-results (do-transactions num-trans)
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
 (run-many-transactions)
 (one-node-value-bench)
  )

(defn -main [& args]
  (println "Running benchmarks and outputing results to ./test/benchmark/bench-results.txt")
  (with-open [w (clojure.java.io/writer "./test/benchmark/bench-results.txt")]
    (binding [*out* w]
      (run-benchmarks))))
