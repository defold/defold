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

(ns integration.subselection-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.particlefx :as particlefx]
            [editor.properties :as properties]
            [editor.types :as types]
            [integration.test-util :as test-util]
            [support.test-support :refer [tx-nodes]]
            [util.id-vec :as iv])
  (:import [javax.vecmath Matrix4d Point3d Vector3d]))

(defn- select! [app-view selection]
  (let [opseq (gensym)]
    (app-view/select! app-view (mapv first selection) opseq)
    (app-view/sub-select! app-view selection opseq)))

(defn- selection [app-view]
  (g/node-value app-view :sub-selection))

(defrecord View [fb select-fn])

(defn- ->view [select-fn]
  (View. {} select-fn))

(defn view-render [view point user-data]
  (update view :fb assoc point user-data))

(defmulti render (fn [evaluation-context node-id view] (g/node-type* (:basis evaluation-context) node-id)))

(defrecord Mesh [vertices]
  types/GeomCloud
  (types/geom-aabbs [_this ids]
    (->> (if ids
           (iv/iv-filter-ids vertices ids)
           vertices)
         (iv/iv-entries)
         (into {}
               (map (fn [[id v]]
                      [id [v v]])))))
  (types/geom-insert [this positions] (assoc this :vertices (iv/iv-into vertices positions)))
  (types/geom-delete [this ids] (assoc this :vertices (iv/iv-remove-ids vertices ids)))
  (types/geom-update [this ids f] (assoc this :vertices (iv/iv-update-ids f vertices ids)))
  (types/geom-transform [this ids transform]
    (let [p (Point3d.)]
      (types/geom-update this ids (fn [v]
                                    (let [[x y] v]
                                      (.set p x y 0.0)
                                      (.transform transform p)
                                      [(.getX p) (.getY p) 0.0]))))))

(defn ->mesh [vertices]
  (Mesh. (iv/iv-vec vertices)))

(g/defnode Model
  (property mesh Mesh))

(defn- ->p3 [v]
  (let [[x y z] v]
    (Point3d. x y (or z 0.0))))

(defn- p3-> [^Point3d p]
  [(.getX p) (.getY p) (.getZ p)])

(defn- centroid [aabb]
  (let [[^Point3d min ^Point3d max] (map ->p3 aabb)]
    (.sub max min)
    (.scaleAdd max 0.5 min)
    (p3-> max)))

(defn- render-geom-cloud [evaluation-context view node-id property]
  (let [render-data (-> (g/node-value node-id property evaluation-context)
                      (types/geom-aabbs nil))]
    (reduce (fn [view [id aabb]] (view-render view (centroid aabb) {:node-id node-id
                                                                    :property property
                                                                    :element-id id}))
            view render-data)))

(defmethod render Model [evaluation-context node-id view]
  (render-geom-cloud evaluation-context view node-id :mesh))

(defmethod render particlefx/EmitterNode [evaluation-context node-id view]
  (render-geom-cloud evaluation-context view node-id :particle-key-alpha))

(defn- render-clear [view]
  (assoc view :fb {}))

(defn- render-all [view renderables]
  (reduce (fn [view r] (render (g/make-evaluation-context) r view))
          view renderables))

(defn- box-select! [view box]
  (let [[minp maxp] box
        selection (->> (:fb view)
                    (filter (fn [[p v]]
                              (and (= minp (mapv min p minp))
                                   (= maxp (mapv max p maxp)))))
                    (map second)
                    (reduce (fn [s v]
                              (update-in s [(:node-id v) (:property v)]
                                         (fn [ids] (conj (or ids []) (:element-id v)))))
                            {})
                    (mapv identity))]
    ((:select-fn view) selection)))

;; Commands

(defn delete! [app-view]
  (let [s (selection app-view)]
    (g/transact (reduce (fn [tx-data v]
                          (if (sequential? v)
                            (let [[nid props] v]
                              (into tx-data
                                    (for [[k ids] props]
                                      (g/update-property nid k types/geom-delete ids))))
                            (into tx-data (g/delete-node s))))
                        [] s))))

(g/defnode MoveManip
  (input selection g/Any)
  (output position g/Any (g/fnk [selection]
                                (let [evaluation-context (g/make-evaluation-context)
                                      positions (->> (for [[nid props] selection
                                                           [k ids] props]
                                                       (map (fn [[id aabb]] [id (centroid aabb)]) (-> (g/node-value nid k evaluation-context)
                                                                                                      (types/geom-aabbs ids))))
                                                     (reduce into [])
                                                     (map second))
                                      avg (mapv / (reduce (fn [r p] (mapv + r p)) [0.0 0.0 0.0] positions)
                                                (repeat (double (count positions))))]
                                  avg))))

(defn- start-move [selection p]
  {:selection selection
   :origin p
   :evaluation-context (g/make-evaluation-context)})

(defn- move! [ctx p]
  (let [origin (->p3 (:origin ctx))
        delta (doto (->p3 p)
                (.sub origin))
        evaluation-context (:evaluation-context ctx)
        selection (:selection ctx)
        transform (doto (Matrix4d.)
                    (.set (Vector3d. delta)))]
    (g/transact
      (for [[nid props] selection
            [k ids] props
            :let [v (g/node-value nid k evaluation-context)]]
        (g/set-property nid k (types/geom-transform v ids transform))))))

;; Tests

(deftest delete-mixed
  (test-util/with-loaded-project
    (let [pfx-id   (test-util/open-tab! project app-view "/particlefx/fireworks_big.particlefx")
          emitter (doto (:node-id (test-util/outline pfx-id [0]))
                    (g/set-property! :particle-key-alpha (properties/->curve [[0.0 0.0 1.0 0.0]
                                                                              [0.6 0.6 1.0 0.0]
                                                                              [1.0 1.0 1.0 0.0]])))
          proj-graph (g/node-id->graph-id project)
          [model] (tx-nodes (g/make-nodes proj-graph [model [Model :mesh (->mesh [[0.5 0.5] [0.9 0.9]])]]))
          view (-> (->view (fn [s] (select! app-view s)))
                 (render-all [model emitter]))
          box [[0.5 0.5] [0.9 0.9]]]
      (box-select! view box)
      (is (not (empty? (selection app-view))))
      (delete! app-view)
      (let [view (-> view
                   render-clear
                   (render-all [model emitter]))]
        (box-select! view box)
        (is (empty? (selection app-view)))))))

(deftest move-mixed
  (test-util/with-loaded-project
    (let [pfx-id   (test-util/open-tab! project app-view "/particlefx/fireworks_big.particlefx")
          emitter (doto (:node-id (test-util/outline pfx-id [0]))
                    (g/set-property! :particle-key-alpha (properties/->curve [[0.0 0.0 1.0 0.0]
                                                                              [0.6 0.6 1.0 0.0]
                                                                              [1.0 1.0 1.0 0.0]])))
          model (-> (g/make-nodes (g/node-id->graph-id project) [model [Model :mesh (->mesh [[0.5 0.5] [0.9 0.9]])]])
                  tx-nodes
                  first)
          manip (-> (g/make-nodes (g/node-id->graph-id app-view) [manip MoveManip]
                      (g/connect app-view :sub-selection manip :selection))
                  tx-nodes
                  first)
          view (-> (->view (fn [s] (select! app-view s)))
                 (render-all [model emitter]))
          box [[0.5 0.5] [0.9 0.9]]]
      (box-select! view box)
      (is (not (empty? (selection app-view))))
      (is (= [(/ 2.0 3.0) (/ 2.0 3.0) 0.0] (g/node-value manip :position)))
      (-> (start-move (selection app-view) (g/node-value manip :position))
        (move! [2.0 2.0 0.0]))
      (let [view (-> view
                   render-clear
                   (render-all [model emitter]))]
        (box-select! view box)
        (is (empty? (selection app-view)))))))

(defn- near [v1 v2]
  (< (Math/abs (- v1 v2)) 0.000001))

(deftest insert-control-point
  (test-util/with-loaded-project
    (let [pfx-id   (test-util/resource-node project "/particlefx/fireworks_big.particlefx")
          emitter (doto (:node-id (test-util/outline pfx-id [0]))
                    (g/set-property! :particle-key-alpha (properties/->curve [[0.0 0.0 0.5 0.5]
                                                                              [0.5 0.5 0.5 0.5]
                                                                              [1.0 1.0 0.5 0.5]])))
          proj-graph (g/node-id->graph-id project)
          view (-> (->view (fn [s] (select! app-view s)))
                 (render-all [emitter]))
          box [[0.5 0.5] [1.0 1.0]]
          half-sq-2 (* 0.5 (Math/sqrt 2.0))]
      (g/transact
        (g/update-property emitter :particle-key-alpha types/geom-insert [[0.25 0.25 0.0]]))
      (let [[x y tx ty] (-> (g/node-value emitter :particle-key-alpha)
                          :points
                          (iv/iv-filter-ids [4])
                          iv/iv-vals
                          first)]
        (is (near half-sq-2 tx))
        (is (near half-sq-2 ty))))))
