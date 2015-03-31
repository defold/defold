(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.project :as p]
            [internal.clojure :as clojure]
            [internal.node :as in]
            [internal.system :as is])
  (:import [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")
(def branch "dummy-branch")

(g/defnode DummyEditor
  (inherits core/Scope)
  (input node t/Any)
  (output node t/Any (g/fnk [node] node)) ; TODO remove, workaround for not being able to pull on inputs
  )

(defn- create-atlas-editor [project atlas-node view-graph]
  (let [tx-result (ds/transact
                   (g/make-nodes view-graph
                                 [editor DummyEditor]
                                 (g/connect atlas-node :self editor :node)))]
    (first (ds/tx-nodes-added tx-result))))

(defn- load-test-project [graph]
  (let [root            (File. project-path)
        project         (p/load-project graph root branch)
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    (assert (not (nil? project)))
    (println 'load-test-project      (concat
      (p/register-editor project "atlas" #'create-atlas-editor)
      (p/register-node-type project "atlas" atlas/AtlasNode)))
    (ds/transact
     (concat
      (p/register-editor project "atlas" #'create-atlas-editor)
      (p/register-node-type project "atlas" atlas/AtlasNode)))
    (p/load-resource-nodes (g/refresh project) non-sources)
    project))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
    (with-clean-system
      (let [project (load-test-project world)
            history-count 0  ; TODO retrieve actual undo-history count
            ]
        (is (= history-count 0))
        (let [atlas-nodes (p/nodes-with-extensions project ["atlas"])]
          (is (> (count atlas-nodes) 0))
          (let [atlas-node  (first atlas-nodes)
                editor-node (p/make-editor project (:filename atlas-node))]
            (is (= atlas-node (g/node-value editor-node :node)))))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
           (with-clean-system
             (let [project       (load-test-project world)
                   atlas-node    (first (p/nodes-with-extensions project ["atlas"]))
                   editor-node   (p/make-editor project (:filename atlas-node))
                   history-count 0 ; TODO retrieve actual undo-history count
                  ]
              (is (not-nil? editor-node))
              (is (= history-count 0))))))

(deftest undo-node-deletion-reconnects-editor
  (testing "Undoing the deletion of a node reconnects it to its editor"
           (with-clean-system
             (let [project   (load-test-project world)
                   atlas-id  (g/node-id (first (p/nodes-with-extensions project ["atlas"])))
                   editor-id (g/node-id (p/make-editor project (:filename @atlas-id)))]
              (ds/transact (g/delete-node atlas-id))
              (is (not-nil? (g/node-by-id (ds/now) editor-id)))
              (is (nil? (g/node-value editor-id :node)))
              (ds/undo)
              (is (not-nil? (g/node-value editor-id :node)))))))
