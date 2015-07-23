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
          project-graph   (g/node-id->graph-id project)
          app-view        (test-util/setup-app-view!)
          sprite-node     (test-util/resource-node project "/logic/atlas_sprite.go")
          collection-node (test-util/resource-node project "/logic/atlas_sprite.collection")
          view            (test-util/open-scene-view! project app-view collection-node 128 128)
          go-node         (ffirst (g/sources-of collection-node :child-scenes))
          fragment        (project/copy [collection-node])]
      (is (not (nil? sprite-node)))
      (is (= 1 (count (:roots fragment))))
      (is (= 2 (count (:nodes fragment))))

      (let [paste-data          (project/paste workspace project fragment)
            tx-result           (g/transact (:tx-data paste-data))
            new-collection-id   (first (:root-node-ids paste-data))
            new-collection-node (g/node-by-id new-collection-id)
            new-go-node         (ffirst (g/sources-of new-collection-id :child-scenes))]

        (is (g/connected? (g/now) new-go-node :scene new-collection-id :child-scenes))

        (is (g/connected? (g/now) sprite-node :_id go-node     :source))
        (is (g/connected? (g/now) sprite-node :_id new-go-node :source))))))

(deftest undo-paste-removes-pasted-nodes
  (with-clean-system
    (let [workspace          (test-util/setup-workspace! world)
          project            (test-util/setup-project! workspace)
          project-graph      (g/node-id->graph-id project)
          app-view           (test-util/setup-app-view!)
          sprite-node        (test-util/resource-node project "/logic/atlas_sprite.go")
          collection-node-id (test-util/resource-node project "/logic/atlas_sprite.collection")
          view               (test-util/open-scene-view! project app-view collection-node-id 128 128)
          go-node            (ffirst (g/sources-of collection-node-id :child-scenes))
          fragment           (project/copy [collection-node-id])]

      (let [paste-data          (project/paste workspace project fragment)
            tx-result           (g/transact (:tx-data paste-data))
            new-collection-id   (first (:root-node-ids paste-data))
            new-go-node-id      (ffirst (g/sources-of new-collection-id :child-scenes))]

        (g/undo! project-graph)

        (is (nil? (g/node-by-id new-collection-id)))
        (is (nil? (g/node-by-id new-go-node-id)))

        (g/redo! project-graph)

        (is (not (nil? (g/node-by-id new-collection-id))))
        (is (not (nil? (g/node-by-id new-go-node-id))))

        (is (g/connected? (g/now) new-go-node-id :scene new-collection-id :child-scenes))
        (is (g/connected? (g/now) sprite-node    :_id   go-node           :source))
        (is (g/connected? (g/now) sprite-node    :_id   new-go-node-id    :source))

        (g/undo! project-graph)

        (is (nil? (g/node-by-id new-collection-id)))
        (is (nil? (g/node-by-id new-go-node-id)))))))
