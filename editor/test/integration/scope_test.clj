(ns integration.scope-test
  (:require [clojure.test :refer :all]
            [clojure.set]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.atlas :as atlas]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.collection :as collection]))

(defn node-count [graph]
  (count (:nodes graph)))

(deftest project-disposes-nodes
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          proj-graph-id (g/make-graph! :history true :volatility 1)
          project-id (project/make-project proj-graph-id workspace)]
      (project/load-project project-id)
      (is (not= 0 (node-count (g/graph proj-graph-id))))
      (g/delete-node! project-id)
      (let [final-node-ids (set (ig/node-ids (g/graph proj-graph-id)))
            orphans (map g/node-by-id final-node-ids)]
        (is (= 0 (count orphans)))))))

(defn check-disposes-nodes
  [resource-type-name inline-resource]
  (test-util/with-loaded-project
    (let [graph-id (g/node-id->graph-id project)
          old-count (node-count (g/graph graph-id))
          old-node-ids (set (ig/node-ids (g/graph graph-id)))
          old-basis (g/now)
          mem-resource (project/make-embedded-resource project resource-type-name inline-resource)]
      (project/load-project project [mem-resource])
      (let [new-resource-node (project/get-resource-node project mem-resource)
            new-count (node-count (g/graph graph-id))]
        (is (> new-count old-count))
        (g/delete-node! new-resource-node)
        (let [final-count (node-count (g/graph graph-id))
              final-node-ids (set (ig/node-ids (g/graph graph-id)))
              new (clojure.set/difference final-node-ids old-node-ids)
              remainders (clojure.set/difference old-node-ids final-node-ids)]
          (is (= [] (map #(g/node-type* old-basis %) remainders)))
          (is (= [] (map g/node-type* new))))))))

(def inline-atlas
"
  images {
    image: \"/switcher/images/blue_candy.png\"
  }
  images {
    image: \"/switcher/images/blue_chameleon.png\"
  }
  animations {
    id: \"tile\"
    images {
      image: \"/switcher/images/nohole.png\"
    }
    playback: PLAYBACK_ONCE_FORWARD
    fps: 30
    flip_horizontal: 0
    flip_vertical: 0
  }
  margin: 4
  extrude_borders: 2
")

(def inline-collection
"
  name: \"main\"
  instances {
    id: \"parent_node-id\"
    prototype: \"/logic/atlas_sprite.go\"
    children: \"child_node-id\"
    position {
      x: 0.0
      y: 0.0
      z: 0.0
    }
    rotation {
      x: 0.0
      y: 0.0
      z: 0.0
      w: 1.0
    }
    scale: 1.0
  }
  instances {
    id: \"child_node-id\"
    prototype: \"/logic/atlas_sprite.go\"
    position {
      x: 100.0
      y: 0.0
      z: 0.0
    }
    rotation {
      x: 0.0
      y: 0.0
      z: 0.0
      w: 1.0
    }
    scale: 1.0
  }
  embedded_instances {
    id: \"embedded_node-id\"
    data: \"\"
    position {
      x: 100.0
      y: 0.0
      z: 0.0
    }
    rotation {
      x: 0.0
      y: 0.0
      z: 0.0
      w: 1.0
    }
    scale: 1.0
  }
")

(def inline-game-object
"
  embedded_components {
  id: \"co\"
  type: \"collisionobject\"
  data: \"collision_shape: \\\"/logic/session/ball.convexshape\\\"\\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\\nmass: 0.0\\nfriction: 0.0\\nrestitution: 1.0\\ngroup: \\\"4\\\"\\nmask: \\\"1\\\"\\nmask: \\\"2\\\"\\nmask: \\\"8\\\"\\n\"
  }
  embedded_components {
    id: \"co\"
    type: \"collisionobject\"
    data: \"collision_shape: \\\"/logic/session/ball.convexshape\\\"\\ntype: COLLISION_OBJECT_TYPE_KINEMATIC\\nmass: 0.0\\nfriction: 0.0\\nrestitution: 1.0\\ngroup: \\\"4\\\"\\nmask: \\\"1\\\"\\nmask: \\\"2\\\"\\nmask: \\\"8\\\"\\n\"
  }
")

(def test-cases
  {"atlas" inline-atlas
   "go" inline-game-object
   "collection" inline-collection})

(deftest resource-nodes-disposes-nodes
  (doseq [[resource-type-name inline-resource] test-cases]
    (check-disposes-nodes resource-type-name inline-resource)))
