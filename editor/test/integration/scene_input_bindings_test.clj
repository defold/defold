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

(ns integration.scene-input-bindings-test
  (:require [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.camera :as camera]
            [editor.input :as input]
            [editor.mouse-binding :as mouse-binding]
            [editor.scene :as scene]
            [editor.tile-map :as tile-map]
            [editor.tile-map-common :as tile-map-common]
            [integration.test-util :as test-util])
  (:import [javax.vecmath Point3d]))

(defn- action [type x y button modifiers]
  (reduce
    (fn [action modifier]
      (assoc action modifier true))
    {:type type
     :x x
     :y y
     :screen-x x
     :screen-y y
     :click-count 1
     :button button}
    modifiers))

(defn- dispatch-action! [view input-state action]
  (let [input-dispatch-context (scene/input-dispatch-context view)
        action (scene/augment-action view action)]
    (scene/dispatch-input-action input-dispatch-context input-state action)))

(defn- update-tick!
  ([view input-state]
   (update-tick! view input-state 1))
  ([view input-state frame-count]
   (loop [input-state input-state
          frames-remaining ^int frame-count]
     (if (pos? frames-remaining)
       (recur (scene/update-tick-handlers view input-state (/ 1.0 60.0))
              (dec frames-remaining))
       input-state))))

(defn- open-tile-map-scene-view! [project app-view path width height]
  (let [resource-node (test-util/resource-node project path)
        view-graph (test-util/make-view-graph!)
        view (scene/make-preview view-graph resource-node
                                  {:prefs (test-util/make-build-stage-test-prefs)
                                   :app-view app-view
                                   :project project
                                   :select-fn (partial app-view/select app-view)
                                   :grid tile-map/TileMapGrid
                                   :tool-controller tile-map/TileMapController}
                                  width
                                  height)]
    (g/transact
      (concat
        (g/connect resource-node :_node-id view :resource-node)
        (g/connect resource-node :valid-node-id+type+resource view :node-id+type+resource)
        (g/connect app-view :selected-node-properties view :selected-node-properties)
        (g/connect view :view-data app-view :open-views)
        (g/set-property app-view :active-view view)))
    (app-view/select! app-view [resource-node])
    [resource-node view]))

(defn- tile-screen-center-pos [view resource-node [tile-x tile-y]]
  (let [[^double tile-width ^double tile-height] (g/node-value resource-node :tile-dimensions)
        world-pos (Point3d. (* (+ ^double tile-x 0.5) tile-width)
                            (* (+ ^double tile-y 0.5) tile-height)
                            0.0)
        half-width (double (/ (double (first (tile-screen-size view resource-node))) 2.0))
        center ()
        screen-pos (camera/camera-project (g/node-value view :camera)
                                          (g/node-value view :viewport)
                                          world-pos)]
    [(+ half-width (.x screen-pos)) (+ half-width (.y screen-pos))]))

(defn- refresh-selection! [view]
  (g/node-value view :all-renderables)
  (g/node-value view :selected-renderables))

(defn- layer-node [resource-node]
  (:node-id (test-util/outline resource-node [0])))

(defn- tile-map-controller [view]
  (reduce
    (fn [_ [node-id]]
      (when (g/node-instance? tile-map/TileMapController node-id)
        (reduced node-id)))
    nil
    (g/sources-of view :input-handlers)))

(defn- camera-controller [view]
  (reduce
    (fn [_ [node-id]]
      (when (g/node-instance? camera/CameraController node-id)
        (reduced node-id)))
    nil
    (g/sources-of view :input-handlers)))

(defn- cell-at [layer-node [x y]]
  (tile-map-common/cell-at (g/node-value layer-node :cell-map) [x y]))

(defn- with-mouse-bindings* [f]
  (let [old-bindings @mouse-binding/bindings-atom]
    (try
      (f)
      (finally
        (reset! mouse-binding/bindings-atom old-bindings)))))

(defmacro with-mouse-bindings [& forms]
  `(with-mouse-bindings* (fn [] ~@forms)))

(defn- tile-screen-size [view resource-node]
  (let [[^double tw ^double th] (g/node-value resource-node :tile-dimensions)
        camera (g/node-value view :camera)
        viewport (g/node-value view :viewport)
        p0 (camera/camera-project camera viewport (Point3d. 0.0 0.0 0.0))
        p1 (camera/camera-project camera viewport (Point3d. tw th 0.0))]
    [(Math/abs (- (.x p1) (.x p0)))
     (Math/abs (- (.y p1) (.y p0)))]))

(defn- screen-pos->tile-cell
  ([view resource-node [screen-x screen-y]]
   (screen-pos->tile-cell view resource-node screen-x screen-y))
  ([view resource-node screen-x screen-y]
   (let [[^double tile-width ^double tile-height] (g/node-value resource-node :tile-dimensions)
         world-pos (camera/camera-unproject (g/node-value view :camera)
                                            (g/node-value view :viewport)
                                            (Point3d. screen-x screen-y 0.0))]
     (tile-map/world-pos->tile (Point3d. (.x world-pos) (.y world-pos) (.z world-pos))
                               tile-width
                               tile-height))))

(defn- tile-screen-center-pos [view resource-node [tile-x tile-y]]
  (let [[^double tile-width ^double tile-height] (g/node-value resource-node :tile-dimensions)
        world-pos (Point3d. (* (+ ^double tile-x 0.5) tile-width)
                            (* (+ ^double tile-y 0.5) tile-height)
                            0.0)
        half-width (double (/ (double (first (tile-screen-size view resource-node))) 2.0))
        screen-pos (camera/camera-project (g/node-value view :camera)
                                          (g/node-value view :viewport)
                                          world-pos)]
    [(+ (.x screen-pos)) (+ (.y screen-pos))]))

(comment
  (test-util/with-loaded-project
    (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
          layer-node (layer-node tile-map-node)
          tile [5 5]
          screen-pos (tile-screen-center-pos view tile-map-node tile)
          asdf (cell-at layer-node tile)]
      (app-view/select! app-view [layer-node])
      (refresh-selection! view)
      (screen-pos->tile-cell view tile-map-node 10.0 60.3035)))
  :-)

(defn print-cells [layer-node start-cell width height]
  (let [[sx sy] start-cell]
    (println (str "┌" (apply str (repeat width "─")) "┐"))
    (doseq [y (range sy (+ sy height))]
      (print "│")
      (doseq [x (range sx (+ sx width))]
        (print (if (cell-at layer-node [x y]) "█" "·")))
      (println "│"))
    (println (str "└" (apply str (repeat width "─")) "┘"))))

(deftest tile-map-rebound-drag-runs-tile-map-action
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::tile-map/tile-map-editor
        [{:command :scene.tile-map.paint
          :context-path ["Tile Map Editor"]
          :action "Paint"
          :binding {:button :primary :trigger :drag :modifiers []}}
         {:command :scene.tile-map.erase
          :context-path ["Tile Map Editor"]
          :action "Erase"
          :binding {:button :primary :trigger :drag :modifiers [:shift]}}])
      (is (= :scene.tile-map.erase
             (mouse-binding/command-for-action
               ::tile-map/tile-map-editor
               {:type :drag
                :button :primary
                :shift true})))
      (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            tile [-1 -1]
            screen-pos (tile-screen-center-pos view tile-map-node tile)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)
        (is (= [layer-node] (g/node-value app-view :selected-node-ids)))
        (is (nil? (cell-at layer-node tile)))
        (let [tile-a [5 5]
              tile-b [6 5]
              tile-c [7 5]
              pos-a (tile-screen-center-pos view tile-map-node tile-a)
              pos-b (tile-screen-center-pos view tile-map-node tile-b)
              pos-c (tile-screen-center-pos view tile-map-node tile-c)
              input-state (reduce
                            (partial dispatch-action! view)
                            (input/make-input-state)
                            [(action :mouse-moved    (first pos-a) (second pos-a) :primary [])
                             (action :mouse-pressed  (first pos-a) (second pos-a) :primary [])
                             (action :mouse-moved    (first pos-b) (second pos-b) :primary [])
                             (action :mouse-moved    (first pos-c) (second pos-c) :primary [])
                             (action :mouse-moved    (first pos-c) (second pos-c) :primary [])
                             (action :mouse-released (first pos-c) (second pos-c) :primary [])])]
          (println pos-a pos-b pos-c)
          (print-cells layer-node [0 0] 10 10)
          (is (some? (cell-at layer-node tile-a)))
          (is (some? (cell-at layer-node tile-b)))
          (is (some? (cell-at layer-node tile-c))))
        #_(let [input-state (reduce
                              (partial dispatch-action! view)
                              (input/make-input-state)
                              [(action :mouse-moved (first screen-pos) (second screen-pos) :primary [:shift])
                               (action :mouse-pressed (first screen-pos) (second screen-pos) :primary [:shift])])]
            (is (= :select (g/node-value (tile-map-controller view) :op)))
            (dispatch-action! view input-state (action :mouse-released (first screen-pos) (second screen-pos) :primary [:shift]))
            (is (nil? (g/node-value (tile-map-controller view) :op)))
            (is (nil? (cell-at layer-node tile))))))))

(deftest camera-rebound-drag-runs-camera-action
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        [{:command :scene.camera.pan
          :context-path ["Scene 2D Camera"]
          :action "Pan"
          :binding {:button :primary :trigger :drag :modifiers [:shift]}}])
      (let [[_collection-node view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)
            camera-controller (camera-controller view)
            initial-camera (g/node-value view :camera)
            input-state (reduce
                          (partial dispatch-action! view)
                          (input/make-input-state)
                          [(action :mouse-moved 64.0 64.0 :primary [:shift])
                           (action :mouse-pressed 64.0 64.0 :primary [:shift])
                           (action :drag-detected 64.0 64.0 :primary [:shift])])]
        (is camera-controller)
        (is (= :track (:movement (g/user-data camera-controller :editor.camera/camera-state))))
        (is (:is-dragging (g/user-data camera-controller :editor.camera/camera-state)))
        (let [input-state (dispatch-action! view input-state (action :mouse-moved 80.0 64.0 :primary [:shift]))
              input-state (update-tick! view input-state 2)]
          (dispatch-action! view input-state (action :mouse-released 80.0 64.0 :primary [:shift])))
        (is (not= initial-camera (g/node-value view :camera)))))))

(deftest tile-map-command-overrides-conflicting-camera-binding
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        [{:command :scene.camera.pan
          :context-path ["Scene 2D Camera"]
          :action "Pan"
          :binding {:button :primary :trigger :drag :modifiers [:shift]}}])
      (mouse-binding/register!
        ::tile-map/tile-map-editor
        [{:command :scene.tile-map.erase
          :context-path ["Tile Map Editor"]
          :action "Erase"
          :binding {:button :primary :trigger :drag :modifiers [:shift]}}])
      (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            tile [-2 -2]
            screen-pos (tile-screen-center-pos view tile-map-node tile)
            camera-controller (camera-controller view)
            initial-camera (g/node-value view :camera)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)
        (is camera-controller)
        (is (cell-at layer-node tile))
        (let [input-state (reduce
                            (partial dispatch-action! view)
                            (input/make-input-state)
                            [(action :mouse-moved (first screen-pos) (second screen-pos) :primary [:shift])
                             (action :mouse-pressed (first screen-pos) (second screen-pos) :primary [:shift])])]
          (is (not= :track (:movement (g/user-data camera-controller :editor.camera/camera-state))))
          (dispatch-action! view input-state (action :mouse-released (first screen-pos) (second screen-pos) :primary [:shift])))
        (is (nil? (cell-at layer-node tile)))
        (is (= initial-camera (g/node-value view :camera)))))))
