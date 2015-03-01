(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [fnk defnk]]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world tx-nodes]]
            [dynamo.types :as dt]
            [internal.system :as is]
            [editor.atlas :as atlas]
             [internal.clojure :as clojure])
  (:import [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")
(def branch "dummy-branch")

(n/defnode DummyEditor
  (inherits n/Scope)
  (input node s/Any)
  (output node s/Any (fnk [node] node)) ; TODO remove, workaround for not being able to pull on inputs
  )

(defn- create-atlas-editor [project atlas-node]
  (let [editor (n/construct DummyEditor)]
    (ds/in
      (ds/add editor)
      (ds/connect atlas-node :self editor :node))
    editor))

(defn- load-test-project []
  (let [root            (File. project-path)
        project         (ds/in (p/load-project root branch)
                              (ds/transactional
                                (p/register-editor "atlas" #'create-atlas-editor)
                                (p/register-node-type "atlas" atlas/AtlasNode)))
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    (p/load-resource-nodes project non-sources)
    project))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
           (with-clean-world
             (let [project (load-test-project)
                   history-count 0 ; TODO retrieve actual undo-history count
                   ]
               (is (= history-count 0))
               (let [atlas-nodes (p/nodes-with-extensions project ["atlas"])]
                 (is (> (count atlas-nodes) 0))
                 (let [atlas-node (first atlas-nodes)
                       editor-node (p/make-editor project (:filename atlas-node))]
                   (is (= atlas-node (n/get-node-value editor-node :node)))))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
           (with-clean-world
            (let [project (load-test-project)
                  atlas-node (first (p/nodes-with-extensions project ["atlas"]))
                  editor-node (p/make-editor project (:filename atlas-node))
                  history-count 0 ; TODO retrieve actual undo-history count
                  ]
              (is (not-nil? editor-node))
              (is (= history-count 0))))))

(deftest undo-node-deletion-reconnects-editor
  (testing "Undoing the deletion of a node reconnects it to its editor"
           (with-clean-world
            (let [project (load-test-project)
                  atlas-node-ref (dt/node-ref (first (p/nodes-with-extensions project ["atlas"])))
                  editor-node-ref (dt/node-ref (p/make-editor project (:filename @atlas-node-ref)))]
              (ds/transactional (ds/delete @atlas-node-ref))
              (is (not-nil? @editor-node-ref))
              (is (nil? (n/get-node-value @editor-node-ref :node)))
              (is/undo) ; TODO undo should not be in an internal ns
              (is (not-nil? (n/get-node-value @editor-node-ref :node)))))))
