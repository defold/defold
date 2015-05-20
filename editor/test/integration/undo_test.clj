(ns integration.undo-test
  (:require [clojure.test :refer :all]
            [clojure.walk :as walk]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace]
            [internal.system :as is])
  (:import [dynamo.types Region]
           [java.awt.image BufferedImage]
           [java.io File]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")

(g/defnode DummySceneView
  (inherits core/Scope)

  (property width t/Num)
  (property height t/Num)
  (input frame BufferedImage)
  (input input-handlers Runnable :array)
  (output viewport Region (g/fnk [width height] (t/->Region 0 width 0 height))))

(defn make-dummy-view [graph width height]
  (g/make-node! graph DummySceneView :width width :height height))

(defn- load-test-workspace [graph]
  (let [workspace (workspace/make-workspace graph project-path)]
    (g/transact
      (concat
        (scene/register-view-types workspace)))
    (let [workspace (g/refresh workspace)]
      (g/transact
        (concat
          (collection/register-resource-types workspace)
          (game-object/register-resource-types workspace)
          (cubemap/register-resource-types workspace)
          (image/register-resource-types workspace)
          (atlas/register-resource-types workspace)
          (platformer/register-resource-types workspace)
          (switcher/register-resource-types workspace)
          (sprite/register-resource-types workspace))))
    (g/refresh workspace)))

(defn- load-test-project
  [workspace proj-graph]
  (let [project (project/make-project proj-graph workspace)
        project (project/load-project project)]
    (g/reset-undo! proj-graph)
    project))

(defn- headless-create-view
  [workspace project resource-node]
  (let [resource-type (project/get-resource-type resource-node)]
    (when (and resource-type (:view-types resource-type))
      (let [make-preview-fn (:make-preview-fn (first (:view-types resource-type)))
            view-opts (:scene (:view-opts resource-type))
            view-graph (g/make-graph! :volatility 100)]
        (make-preview-fn view-graph resource-node view-opts 128 128)))))

(defn- has-undo? [graph]
  (g/has-undo? graph))

(deftest preconditions
  (testing "Verify preconditions for remaining tests"
    (with-clean-system
      (let [workspace     (load-test-workspace world)
            project-graph (g/make-graph! :history true :volatility 1)
            project       (load-test-project workspace project-graph)]
        (is (false? (has-undo? project-graph)))
        (let [atlas-nodes (project/find-resources project "**/*.atlas")]
          (is (> (count atlas-nodes) 0)))))))

(deftest open-editor
  (testing "Opening editor does not alter undo history"
           (with-clean-system
             (let [workspace     (load-test-workspace world)
                   project-graph (g/make-graph! :history true :volatility 1)
                   project       (load-test-project workspace project-graph)]
               (is (not (has-undo? project-graph)))
               (let [atlas-nodes (project/find-resources project "**/*.atlas")
                     atlas-node  (second (first atlas-nodes))
                     view        (headless-create-view workspace project atlas-node)]
                 (is (not (has-undo? project-graph)))
                 #_(is (not-nil? (g/node-value view :frame)))
                 (is (not (has-undo? project-graph))))))))

(deftest undo-node-deletion-reconnects-editor
  (testing "Undoing the deletion of a node reconnects it to its editor"
    (with-clean-system
      (let [workspace                 (load-test-workspace world)
            project-graph             (g/make-graph! :history true :volatility 1)
            project                   (load-test-project workspace project-graph)
            atlas-nodes               (project/find-resources project "**/*.atlas")
            atlas-node                (second (first atlas-nodes))
            atlas-id                  (g/node-id atlas-node)
            view                      (headless-create-view workspace project atlas-node)
            view-id                   (g/node-id view)
            check-conn                #(g/connected? (g/now) atlas-id :scene view-id :scene)
            connected-after-open (check-conn)]
        (g/transact (g/delete-node atlas-id))
        (let [connected-after-delete (check-conn)]
          (g/undo project-graph)
          (let [connected-after-undo (g/connected? (g/now) atlas-id :scene view-id :scene)]
            (is connected-after-open)
            (is (not connected-after-delete))
            (is connected-after-undo)))))))

(defn outline-children [node] (:children (g/node-value node :outline)))

(deftest redo-undone-deletion-still-deletes
  (with-clean-system
    (let [workspace     (load-test-workspace world)
          project-graph (g/make-graph! :history true :volatility 1)
          project       (load-test-project workspace project-graph)]
      (let [game-object-node (second (first (project/find-resources project "switcher/test.go")))]

        (is (= 1 (count (outline-children game-object-node))))

        (g/transact (g/delete-node (:self (first (outline-children game-object-node)))))

        (is (= 0 (count (outline-children game-object-node))))

        ;; undo deletion
        (g/undo project-graph)
        (is (= 1 (count (outline-children game-object-node))))

        ;; redo deletion
        (g/redo project-graph)

        (is (= 0 (count (outline-children game-object-node))))))))

(g/defnode DummyComponent
  (output outline t/Any (g/fnk [self] {:self self :label "dummy" :icon nil :children []})))

(g/defnode OutlineViewSimulator
  (input outline t/Any)

  (property counter t/Any)

  (output outline t/Any :cached
          (g/fnk [self outline]
                 (swap! (:counter self) inc)
                 outline)))

(defn remove-handlers
  "Handlers are functions, so they are never equal. Strip them out of the outline"
  [outline]
  (walk/postwalk
   (fn [form]
     (if-not (and (vector? form) (= :handler-fn (first form)))
       form))
   outline))

(deftest undo-redo-undo-redo
  (with-clean-system
    (let [workspace        (load-test-workspace world)
          project-graph    (g/make-graph! :history true :volatility 1)
          project          (load-test-project workspace project-graph)
          game-object-node (second (first (project/find-resources project "switcher/test.go")))
          outline          (g/make-node! project-graph OutlineViewSimulator :counter (atom 0))
          component        (g/make-node! project-graph DummyComponent)]
      (g/transact [(g/connect game-object-node :outline outline :outline)
                   (g/connect component :outline game-object-node :outline)])

      (let [original-outline (remove-handlers (g/node-value outline :outline))]

        (g/reset-undo! project-graph)

        ;; delete the component
        (g/transact
         [(g/operation-label "delete node")
          (g/delete-node component)])

        ;; force :outline to be cached
        (let [outline-without-component (remove-handlers (g/node-value outline :outline))]

          ;; undo the deletion (component is back)
          (g/undo project-graph)

          ;; same :outline should be re-produced
          (let [outline-after-undo (remove-handlers (g/node-value outline :outline))]
            (is (= original-outline outline-after-undo)))

          ;; redo the deletion (component is gone)
          (g/redo project-graph)

          ;; :outline should be re-produced again
          (is (= outline-without-component (remove-handlers (g/node-value outline :outline))))

          ;; undo the deletion again (component is back again)
          (g/undo project-graph)

          ;; :outline should be re-produced
          (let [outline-after-second-undo (remove-handlers (g/node-value outline :outline))]
            (is (= original-outline outline-after-second-undo)))

          ;; redo the deletion yet again (component is gone again)
          (g/redo project-graph)

          ;; :outline should be re-produced yet again
          (is (= outline-without-component (remove-handlers (g/node-value outline :outline)))))))))
