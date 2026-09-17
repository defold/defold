;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.scope-test
  (:require [clojure.set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.progress :as progress]
            [integration.test-util :as test-util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [support.test-support :refer [with-clean-system]]))

(defn node-count [graph]
  (count (gt/nodes graph)))

(deftest project-disposes-owned-nodes
  (with-clean-system
    (let [workspace (test-util/setup-workspace!)
          workspace-node-ids (set (ig/node-ids (g/now)))
          extensions (extensions/make)
          project-id (project/make-project workspace extensions)]
      (project/load-project! project-id)
      (is (< (count workspace-node-ids) (node-count (g/now))))
      (g/delete-node! project-id)
      (is (= workspace-node-ids
             (set (ig/node-ids (g/now))))))))

(defn check-disposes-nodes
  [resource-type-name inline-resource]
  (test-util/with-loaded-project
    (let [old-count (node-count (g/now))
          old-node-ids (set (ig/node-ids (g/now)))
          old-basis (g/now)
          mem-resource (project/make-embedded-resource project :editable resource-type-name inline-resource)
          node-id+resource-pairs (project/make-node-id+resource-pairs [mem-resource])
          node-load-infos (project/read-nodes node-id+resource-pairs)
          prelude-tx-data (project/make-resource-nodes-tx-data project node-id+resource-pairs)
          migrated-resource-node-ids (project/load-nodes! project prelude-tx-data node-load-infos progress/null-render-progress! nil nil)]
      (project/cache-loaded-save-data! node-load-infos project migrated-resource-node-ids)
      (let [new-resource-node (project/get-resource-node project mem-resource)
            new-count (node-count (g/now))]
        (is (> new-count old-count))
        (g/delete-node! new-resource-node)
        (let [final-count (node-count (g/now))
              final-node-ids (set (ig/node-ids (g/now)))
              new (clojure.set/difference final-node-ids old-node-ids)
              remainders (clojure.set/difference old-node-ids final-node-ids)]
          (is (= old-count final-count))
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
