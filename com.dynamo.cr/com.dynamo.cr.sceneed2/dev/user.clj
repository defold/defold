(require '[dynamo.project :as p])
(require '[dynamo.file :as f])
(require '[internal.graph.dgraph :as dg])

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn nodes-and-classes-in-project
  []
  (p/with-current-project
       (map (fn [n v] [n (class v)]) (keys (get-in @p/*current-project* [:graph :nodes])) (vals (get-in @p/*current-project* [:graph :nodes])))))

(defn node-in-project
  [id]
  (p/with-current-project (dg/node (:graph @p/*current-project*) id)))

(defn load-resource-in-project
  [res]
  (p/with-current-project (p/load-resource p/*current-project* (f/project-path p/*current-project* res))))

(defn get-value-in-project
  [id label]
  (p/with-current-project
    (p/get-resource-value p/*current-project* (node-in-project id) label)))