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

(ns integration.perf-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.scene :as scene]
            [integration.test-util :as test-util]
            [internal.node :as in]
            [support.test-support :refer [enable-performance-tests]]
            [util.fn :as fn]))

(defn- gui-node [scene id]
  (let [id->node (->> (get-in (g/node-value scene :node-outline) [:children 0])
                      (tree-seq fn/constantly-true :children)
                      (map :node-id)
                      (map (fn [node-id] [(g/node-value node-id :id) node-id]))
                      (into {}))]
    (id->node id)))

(defn- drag-pull-outline! [scene-id node-id i]
  (g/set-property! node-id :position (assoc scene/default-position 0 (float i)))
  (g/node-value scene-id :scene)
  (g/node-value scene-id :node-outline))

(defn- clock []
  (/ (System/nanoTime) 1000000.0))

(defmacro measure [binding & body]
  `(let [start# (clock)]
     (dotimes ~binding
       ~@body)
     (let [end# (clock)]
       (/ (- end# start#) ~(second binding)))))

(when enable-performance-tests
  (defn- gui-template-outline-perf-warmup [node-id box]
    (dotimes [i 50]
      (drag-pull-outline! node-id box i)))

  (defn- gui-template-outline-perf-benchmark [node-id box]
    (measure [i 20]
             (drag-pull-outline! node-id box i)))

  (deftest gui-template-outline-perf
    (binding [in/*check-schemas* false]
      (testing "drag-pull-outline"
        (test-util/with-loaded-project
          (let [node-id (test-util/resource-node project "/gui/scene.gui")
                box (gui-node node-id "sub_scene/sub_box")]

            ;; WARM-UP
            (gui-template-outline-perf-warmup node-id box)
            (System/gc)

            ;; GO!
            (let [elapsed (gui-template-outline-perf-benchmark node-id box)]
              (is (< elapsed 14)))))))))
