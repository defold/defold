(ns integration.copy-paste-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.project :as project]
            [integration.test-util :as test-util]))

(deftest copy-paste-links-resource
  (with-clean-system
    (let [workspace       (test-util/setup-workspace! world)
          project         (test-util/setup-project! workspace)
          project-graph   (g/node->graph-id project)
          app-view        (test-util/setup-app-view!)
          sprite-node     (test-util/resource-node project "/logic/atlas_sprite.go")
          collection-node (test-util/resource-node project "/logic/atlas_sprite.collection")
          view            (test-util/open-scene-view! project app-view collection-node 128 128)
          go-node         (ffirst (g/sources-of collection-node :child-scenes))
          fragment        (project/copy [(g/node-id collection-node)])]
      (is (not (nil? sprite-node)))
      (is (= 1 (count (:roots fragment))))
      (is (= 2 (count (:nodes fragment))))

      (let [paste-data          (project/paste workspace project fragment)
            tx-result           (g/transact (:tx-data paste-data))
            new-collection-id   (first (:root-node-ids paste-data))
            new-collection-node (g/node-by-id new-collection-id)
            new-go-node         (ffirst (g/sources-of new-collection-node :child-scenes))]

        (is (g/connected? (g/now) (g/node-id new-go-node) :scene (g/node-id new-collection-node) :child-scenes))

        (is (g/connected? (g/now) (g/node-id sprite-node) :_self (g/node-id go-node)     :source))
        (is (g/connected? (g/now) (g/node-id sprite-node) :_self (g/node-id new-go-node) :source))))))

(deftest undo-paste-removes-pasted-nodes
  (with-clean-system
    (let [workspace       (test-util/setup-workspace! world)
          project         (test-util/setup-project! workspace)
          project-graph   (g/node->graph-id project)
          app-view        (test-util/setup-app-view!)
          sprite-node     (test-util/resource-node project "/logic/atlas_sprite.go")
          collection-node (test-util/resource-node project "/logic/atlas_sprite.collection")
          view            (test-util/open-scene-view! project app-view collection-node 128 128)
          go-node         (ffirst (g/sources-of collection-node :child-scenes))
          fragment        (project/copy [(g/node-id collection-node)])]

      (let [paste-data          (project/paste workspace project fragment)
            tx-result           (g/transact (:tx-data paste-data))
            new-collection-id   (first (:root-node-ids paste-data))
            new-collection-node (g/node-by-id new-collection-id)
            new-go-node         (ffirst (g/sources-of new-collection-node :child-scenes))
            new-go-node-id      (g/node-id new-go-node)]

        (g/undo! project-graph)

        (is (nil? (g/node-by-id new-collection-id)))
        (is (nil? (g/node-by-id new-go-node-id)))

        (g/redo! project-graph)

        (is (not (nil? (g/node-by-id new-collection-id))))
        (is (not (nil? (g/node-by-id new-go-node-id))))

        (is (g/connected? (g/now) (g/node-id new-go-node) :scene (g/node-id new-collection-node) :child-scenes))
        (is (g/connected? (g/now) (g/node-id sprite-node) :_self  (g/node-id go-node)     :source))
        (is (g/connected? (g/now) (g/node-id sprite-node) :_self  (g/node-id new-go-node) :source))

        (g/undo! project-graph)

        (is (nil? (g/node-by-id new-collection-id)))
        (is (nil? (g/node-by-id new-go-node-id)))))))
