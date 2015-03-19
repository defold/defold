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
            [internal.node :as in])
  (:import [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")
(def branch "dummy-branch")

(g/defnode DummyEditor
  (inherits core/Scope)
  (input node t/Any)
  (output node t/Any (g/fnk [node] node)) ; TODO remove, workaround for not being able to pull on inputs
  )

(defn- create-atlas-editor [project atlas-node]
  (let [editor (n/construct DummyEditor)]
    (ds/in
      (g/add editor)
      (g/connect atlas-node :self editor :node))
    editor))

(defn- load-test-project []
  (let [root            (File. project-path)
        project         (ds/in (p/load-project root branch)
                              (g/transactional
                                (p/register-editor "atlas" #'create-atlas-editor)
                                (p/register-node-type "atlas" atlas/AtlasNode)))
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    (p/load-resource-nodes project non-sources)
    project))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
           (with-clean-system
             (let [project (load-test-project)
                   history-count 0 ; TODO retrieve actual undo-history count
                   ]
               (is (= history-count 0))
               (let [atlas-nodes (p/nodes-with-extensions project ["atlas"])]
                 (is (> (count atlas-nodes) 0))
                 (let [atlas-node (first atlas-nodes)
                       editor-node (p/make-editor project (:filename atlas-node))]
                   (is (= atlas-node (g/node-value (:graph @world-ref) cache editor-node :node)))))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
           (with-clean-system
            (let [project (load-test-project)
                  atlas-node (first (p/nodes-with-extensions project ["atlas"]))
                  editor-node (p/make-editor project (:filename atlas-node))
                  history-count 0 ; TODO retrieve actual undo-history count
                  ]
              (is (not-nil? editor-node))
              (is (= history-count 0))))))

(deftest undo-node-deletion-reconnects-editor
  (testing "Undoing the deletion of a node reconnects it to its editor"
           (with-clean-system
            (let [project (load-test-project)
                  atlas-node-ref (g/node-ref (first (p/nodes-with-extensions project ["atlas"])))
                  editor-node-ref (g/node-ref (p/make-editor project (:filename @atlas-node-ref)))]
              (g/transactional (g/delete @atlas-node-ref))
              (is (not-nil? @editor-node-ref))
              (is (nil? (g/node-value (:graph @world-ref) cache @editor-node-ref :node)))
              (ds/undo)
              (is (not-nil? (g/node-value (:graph @world-ref) cache @editor-node-ref :node)))))))
