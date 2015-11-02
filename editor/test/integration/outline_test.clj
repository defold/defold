(ns integration.outline-test
  (:require [clojure.test :refer :all]
            [service.log :as log]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.project :as project]
            [editor.outline :as outline]
            [integration.test-util :as test-util]))

(def ^:dynamic *project-path* test-util/project-path)

(defn- setup
  ([world] (setup world *project-path*))
  ([world project-path]
   (let [workspace       (test-util/setup-workspace! world project-path)
         project         (test-util/setup-project! workspace)]
     [workspace project])))

(defn- outline
  ([node]
    (outline node []))
  ([node path]
    (loop [outline (g/node-value node :node-outline)
           path path]
      (if-let [segment (first path)]
        (recur (get (vec (:children outline)) segment) (rest path))
        outline))))

(def ^:private ^:dynamic *clipboard* nil)
(def ^:private ^:dynamic *dragboard* nil)
(def ^:private ^:dynamic *drag-source-iterators* nil)

(defrecord TestItemIterator [root-node path]
  outline/ItemIterator
  (value [this] (outline root-node path))
  (parent [this] (when (not (empty? path))
                   (TestItemIterator. root-node (butlast path)))))

(defn- ->iterator [root-node path]
  (TestItemIterator. root-node path))

(defn- copy! [node path]
  (let [data (outline/copy [(->iterator node path)])]
    (alter-var-root #'*clipboard* (constantly data))))

(defn- cut? [node path]
  (outline/cut? [(->iterator node path)]))

(defn- cut! [node path]
  (let [data (outline/cut! [(->iterator node path)])]
    (alter-var-root #'*clipboard* (constantly data))))

(defn- paste!
  ([project node]
    (paste! project node []))
  ([project node path]
    (let [it (->iterator node path)]
      (outline/paste! (project/graph project) it *clipboard* (partial project/select project)))))

(defn- copy-paste! [project node path]
  (copy! node path)
  (paste! project node (butlast path)))

(defn- drag! [node path]
  (let [src-item-iterators [(->iterator node path)]
        data (outline/copy src-item-iterators)]
    (alter-var-root #'*dragboard* (constantly data))
    (alter-var-root #'*drag-source-iterators* (constantly src-item-iterators))))

(defn- drop!
  ([project node]
    (drop! project node []))
  ([project node path]
    (outline/drop! (project/graph project) *drag-source-iterators* (->iterator node path) *dragboard* (partial project/select project))))

(defn- drop?
  ([project node]
    (drop? project node []))
  ([project node path]
    (outline/drop? (project/graph project) *drag-source-iterators* (->iterator node path) *dragboard*)))

(defn- child-count
  ([node]
    (child-count node []))
  ([node path]
   (count (:children (outline node path)))))

(defn- outline-seq [root]
  (tree-seq :children :children (g/node-value root :node-outline)))

(deftest copy-paste-double-embed
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/collection/embedded_embedded_sounds.collection")]
      ; 1 go instance
      (is (= 2 (child-count root)))
      (copy! root [0])
      (paste! project root)
      ; 2 go instances
      (is (= 3 (child-count root))))))

(deftest copy-paste-game-object
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/logic/atlas_sprite.go")]
      ; 1 comp instance
      (is (= 1 (child-count root)))
      (copy! root [0])
      (paste! project root)
      ; 2 comp instances
      (is (= 2 (child-count root)))
      (is (contains? (outline root [1]) :icon))
      (cut! root [0])
      ; 1 comp instances
      (is (= 1 (child-count root))))))

(deftest copy-paste-collection
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/logic/atlas_sprite.collection")]
      ; 1 go instance
      (is (= 1 (child-count root)))
      ; 1 sprite comp
      (is (= 1 (child-count root [0])))
      (copy! root [0])
      (paste! project root)
      ; 2 go instances
      (is (= 2 (child-count root)))
      ; 1 sprite comp
      (is (= 1 (child-count root [1])))
      (paste! project root [0])
      ; 1 sprite comp + 1 go instance
      (is (= 2 (child-count root [0])))
      ; sprite can be cut
      (is (cut? root [0 0]))
      (cut! root [0 0])
      ; 1 go instance
      (is (= 1 (child-count root [0]))))))

(deftest copy-paste-collection-instance
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/collection/sub_props.collection")]
      ; 1 collection instance
      (is (= 1 (child-count root)))
      ; 1 go instance
      (is (= 1 (child-count root [0])))
      (copy! root [0])
      (paste! project root)
      (is (= 2 (child-count root)))
      (is (= 1 (child-count root [1])))
      (cut! root [0 0])
      (paste! project root)
      ; 2 collection instances + 1 go instances
      (is (= 3 (child-count root)))
      ; 0 go instances under coll instance
      (is (= 0 (child-count root [0])))
      (cut! root [2])
      (paste! project root [0])
      ; 2 collection instance
      (is (= 2 (child-count root)))
      ; 1 go instance
      (is (= 1 (child-count root [0]))))))

(deftest dnd-collection
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/logic/atlas_sprite.collection")]
      (is (= 1 (child-count root)))
      (let [first-id (get (outline root [0]) :label)]
        (drag! root [0])
        (is (not (drop? project root)))
        (is (not (drop? project root [0])))
        (copy-paste! project root [0])
        (is (= 2 (child-count root)))
        (let [second-id (get (outline root [1]) :label)]
          (is (not= first-id second-id))
          (drag! root [1])
          (is (drop? project root [0]))
          (drop! project root [0])
          (is (= 1 (child-count root)))
          (is (= 2 (child-count root [0])))
          (is (= second-id (get (outline root [0 1]) :label)))
          (drag! root [0 1])
          (drop! project root)
          (is (= 2 (child-count root)))
          (is (= second-id (get (outline root [1]) :label))))))))

(deftest dnd-gui
  (with-clean-system
    (let [[workspace project] (setup world)
          root (test-util/resource-node project "/logic/main.gui")]
      (let [first-id (get (outline root [0 1]) :label)]
        (drag! root [0 1])
        (drop! project root [0 0])
        (let [second-id (get (outline root [0 0 0]) :label)]
          (is (= first-id second-id))
          (drag! root [0 0 0])
          (drop! project root [0])
          (is (= second-id (get (outline root [0 1]) :label))))))))

(deftest outline-shows-missing-parts
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world "resources/missing_project"))]  ; no logging as purposely partially broken project
      (testing "Missing go file visible in collection outline"
        (let [root (test-util/resource-node project "/missing_go.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "non-existent"))))
      (testing "Missing sub collection visible in collection outline"
        (let [root (test-util/resource-node project "/missing_collection.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith  (:label (outline root [0])) "non-existent"))))
      (testing "Missing script visible in go outline"
        (let [root (test-util/resource-node project "/missing_component.go")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "non-existent"))))
      (testing "Missing script visible in collection-go-outline"
        (let [root (test-util/resource-node project "/missing_go_component.collection")
              labels (map :label (outline-seq root))
              expected-prefixes ["Collection" "missing_component" "non-existent"]]
          (is (= 3 (count labels))) ; collection + go + script
          (is (every? true? (map #(.startsWith %1 %2) labels expected-prefixes))))))))

(deftest outline-shows-nil-parts
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup world "resources/nil_project"))]  ; no logging as purposely partially broken project
      (testing "Nil go file visible in collection outline"
        (let [root (test-util/resource-node project "/nil_go.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "nil-go"))))
      (testing "Nil sub collection visible in collection outline"
        (let [root (test-util/resource-node project "/nil_collection.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith  (:label (outline root [0])) "nil-collection"))))
      (testing "Nil script visible in go outline"
        (let [root (test-util/resource-node project "/nil_component.go")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "nil-component"))))
      (testing "Nil script visible in collection-go-outline"
        (let [root (test-util/resource-node project "/nil_go_component.collection")
              labels (map :label (outline-seq root))
              expected-prefixes ["Collection" "nil-go" "nil-component"]]
          (is (= 3 (count labels))) ; collection + go + script
          (is (every? true? (map #(.startsWith %1 %2) labels expected-prefixes))))))))
  
