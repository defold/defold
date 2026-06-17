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
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.camera :as camera]
            [editor.curve-view :as curve-view]
            [editor.input :as input]
            [editor.mouse-binding :as mouse-binding]
            [editor.mouse-binding-test :refer [with-mouse-bindings]]
            [editor.scene :as scene]
            [editor.scene-selection :as selection]
            [editor.tile-map :as tile-map]
            [editor.tile-map-common :as tile-map-common]
            [editor.types :as types]
            [integration.test-util :as test-util])
  (:import [javax.vecmath Point3d]))

(defn- action [type x y button modifiers]
  {:type type
   :x x
   :y y
   :screen-x x
   :screen-y y
   :click-count 1
   :button button
   :modifiers (set modifiers)})

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
  (test-util/open-scene-view! project app-view path width height
                               {:grid tile-map/TileMapGrid
                                :tool-controller tile-map/TileMapController}))

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


(defn- screen-pos->tile-cell
  [view resource-node screen-x screen-y]
  (let [[^double tile-width ^double tile-height] (g/node-value resource-node :tile-dimensions)
        world-pos (camera/camera-unproject (g/node-value view :camera)
                                           (g/node-value view :viewport)
                                           (Point3d. screen-x screen-y 0.0))]
    (tile-map/world-pos->tile (Point3d. (.x world-pos) (.y world-pos) (.z world-pos))
                              tile-width
                              tile-height)))

(defn- tile-screen-center-pos [view resource-node [tile-x tile-y]]
  (let [[^double tile-width ^double tile-height] (g/node-value resource-node :tile-dimensions)
        world-pos (Point3d. (* (+ ^double tile-x 0.5) tile-width)
                            (* (+ ^double tile-y 0.5) tile-height)
                            0.0)
        screen-pos (camera/camera-project (g/node-value view :camera)
                                          (g/node-value view :viewport)
                                          world-pos)]
    [(.x screen-pos) (.y screen-pos)]))

(deftest tile-map-rebound-drag-runs-tile-map-action
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::tile-map/tile-map-editor
        "Tile Map Editor"
        [{:command :scene.tile-map.paint
          :action ["Paint"]
          :binding {:button :primary :modifiers #{}}}
         {:command :scene.tile-map.erase
          :action ["Erase"]
          :binding {:button :primary :modifiers #{:shift}}}])
      (is (= :scene.tile-map.erase
             (mouse-binding/command-for-action
               ::tile-map/tile-map-editor
               {:button :primary
                :modifiers #{:shift}})))
      (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            tile [-2 -2]
            screen-pos (tile-screen-center-pos view tile-map-node tile)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)
        (is (= [layer-node] (g/node-value app-view :selected-node-ids)))
        (is (cell-at layer-node tile))
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
                             (action :mouse-released (first pos-c) (second pos-c) :primary [])])]
          (is (= tile-c (screen-pos->tile-cell view tile-map-node (first pos-c) (second pos-c))))
          (is (some? (cell-at layer-node tile-a)))
          (is (some? (cell-at layer-node tile-b)))
          (is (some? (cell-at layer-node tile-c))))
        (let [input-state (reduce
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
        "Scene 2D Camera"
        [{:command :scene.camera.pan
          :action ["Pan"]
          :binding {:button :primary :modifiers #{:shift}}}])
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
        "Scene 2D Camera"
        [{:command :scene.camera.pan
          :action ["Pan"]
          :binding {:button :primary :modifiers #{:shift}}}])
      (mouse-binding/register!
        ::tile-map/tile-map-editor
        "Tile Map Editor"
        [{:command :scene.tile-map.erase
          :action ["Erase"]
          :binding {:button :primary :modifiers #{:shift}}}])
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

(deftest empty-tool-camera-binding-falls-through-to-camera-context
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        "Scene 2D Camera"
        [{:command :scene.camera.pan
          :action ["Pan"]
          :binding {:button :primary :modifiers #{:shift}}}])
      (mouse-binding/register!
        ::tile-map/tile-map-editor
        "Tile Map Editor"
        [{:command :scene.camera.pan
          :action ["Pan"]}]
        {:fallback-context ::camera/scene-camera-orthographic})
      (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            camera-controller (camera-controller view)
            initial-camera (g/node-value view :camera)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)
        (is camera-controller)
        (let [input-state (reduce
                            (partial dispatch-action! view)
                            (input/make-input-state)
                            [(action :mouse-moved 64.0 64.0 :primary [:shift])
                             (action :mouse-pressed 64.0 64.0 :primary [:shift])
                             (action :drag-detected 64.0 64.0 :primary [:shift])])]
          (is (= :track (:movement (g/user-data camera-controller :editor.camera/camera-state))))
          (let [input-state (dispatch-action! view input-state (action :mouse-moved 80.0 64.0 :primary [:shift]))
                input-state (update-tick! view input-state 2)]
            (dispatch-action! view input-state (action :mouse-released 80.0 64.0 :primary [:shift]))))
        (is (not= initial-camera (g/node-value view :camera)))))))

(defn- run-persisted-override-pan-test! [view trigger-binding]
  (let [camera-ctrl (camera-controller view)
        initial-camera (g/node-value view :camera)
        {:keys [button modifiers]} trigger-binding
        input-state (reduce
                      (partial dispatch-action! view)
                      (input/make-input-state)
                      [(action :mouse-moved 64.0 64.0 button modifiers)
                       (action :mouse-pressed 64.0 64.0 button modifiers)
                       (action :drag-detected 64.0 64.0 button modifiers)])]
    (is camera-ctrl)
    (is (= :track (:movement (g/user-data camera-ctrl :editor.camera/camera-state))))
    (let [input-state (dispatch-action! view input-state (action :mouse-moved 80.0 64.0 button modifiers))
          input-state (update-tick! view input-state 2)]
      (dispatch-action! view input-state (action :mouse-released 80.0 64.0 button modifiers)))
    (is (not= initial-camera (g/node-value view :camera)))))

(deftest persisted-camera-pan-binding-runs-camera-action
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        "Scene 2D Camera"
        [{:command :scene.camera.pan
          :action ["Pan"]
          :binding {:button :primary :modifiers #{:shift}}}])

      (testing "registered binding works without overrides"
        (let [[_ view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)]
          (run-persisted-override-pan-test! view {:button :primary :modifiers #{:shift}})))

      (testing "single override binding in scene view"
        (mouse-binding/set-user-overrides!
          {::camera/scene-camera-orthographic
           {:scene.camera.pan
            {:bindings [{:button :primary :modifiers #{:shift}}]}}})
        (let [[_ view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)]
          (run-persisted-override-pan-test! view {:button :primary :modifiers #{:shift}})))

      (testing "appended override binding in scene view"
        (mouse-binding/set-user-overrides!
          {::camera/scene-camera-orthographic
           {:scene.camera.pan
            {:bindings [{:button :primary :modifiers #{:shift}}
                        {:button :middle :modifiers #{:control}}]}}})
        (let [[_ view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)]
          (run-persisted-override-pan-test! view {:button :middle :modifiers #{:control}})))

      (testing "override binding works in curve view"
        (mouse-binding/set-user-overrides!
          {::camera/scene-camera-orthographic
           {:scene.camera.pan
            {:bindings [{:button :primary :modifiers #{:shift}}]}}})
        (let [view (doto (curve-view/make-view! app-view (test-util/make-view-graph!) nil nil test-util/localization {} false)
                     (g/set-property! :viewport (types/->Region 0 128 0 128)))]
          (run-persisted-override-pan-test! view {:button :primary :modifiers #{:shift}}))))))

(deftest disallowed-curve-view-camera-bindings-are-ignored
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::curve-view/curve-view-camera
        "Curve Editor"
        [{:command :scene.camera.pan
          :action ["Pan"]}
         {:command :scene.camera.zoom
          :action ["Zoom"]}
         {:command :scene.camera.orbit
          :action ["Orbit"]
          :binding {:button :primary :modifiers #{:shift}}}
         {:command :scene.camera.free-look
          :action ["Free Look"]
          :binding {:button :primary :modifiers #{:control}}}])
      (let [view (doto (curve-view/make-view! app-view (test-util/make-view-graph!) nil nil test-util/localization {} false)
                   (g/set-property! :viewport (types/->Region 0 128 0 128)))
            camera-controller (camera-controller view)
            initial-camera (g/node-value view :camera)]
        (is camera-controller)
        (g/set-property! view :tool-picking-rect (selection/calc-picking-rect [64.0 64.0 0.0] [64.0 64.0 0.0]))
        (doseq [{:keys [name button modifiers expected-movement]}
                [{:name "orbit"
                  :button :primary
                  :modifiers #{:shift}
                  :expected-movement :tumble}
                 {:name "free look"
                  :button :primary
                  :modifiers #{:control}
                  :expected-movement :look}]]
          (testing name
            (let [input-state (reduce
                                (partial dispatch-action! view)
                                (input/make-input-state)
                                [(action :mouse-moved 64.0 64.0 button modifiers)
                                 (action :mouse-pressed 64.0 64.0 button modifiers)
                                 (action :drag-detected 64.0 64.0 button modifiers)])]
              (is (not= expected-movement
                        (:movement (g/user-data camera-controller :editor.camera/camera-state))))
              (is (not (:free-cam-mode (g/user-data camera-controller :editor.camera/camera-state))))
              (let [input-state (dispatch-action! view input-state (action :mouse-moved 96.0 64.0 button modifiers))
                    input-state (update-tick! view input-state 2)]
                (dispatch-action! view input-state (action :mouse-released 96.0 64.0 button modifiers))))
            (is (= initial-camera (g/node-value view :camera))
                "Curve view should ignore camera movements that are disabled there.")))))))

(deftest persisted-camera-override-works-in-tile-map-view
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        "Scene 2D Camera"
        [{:command :scene.camera.pan :action ["Pan"]}])
      ;; Override to something the base binding cannot match:
      ;; secondary button + control modifier.
      (mouse-binding/set-user-overrides!
        {::camera/scene-camera-orthographic
         {:scene.camera.pan
          {:bindings [{:button :secondary :modifiers #{:control}}]}}})
      (let [[tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            camera-controller (camera-controller view)
            initial-camera (g/node-value view :camera)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)

        ;; Sanity: the base binding (primary, no modifiers) must NOT pan,
        ;; because the override replaced it.
        (let [input-state (reduce
                           (partial dispatch-action! view)
                           (input/make-input-state)
                           [(action :mouse-moved 64.0 64.0 :primary [])
                            (action :mouse-pressed 64.0 64.0 :primary [])
                            (action :drag-detected 64.0 64.0 :primary [])])]
          (is (not= :track (:movement (g/user-data camera-controller :editor.camera/camera-state))))
          (dispatch-action! view input-state (action :mouse-released 64.0 64.0 :primary [])))

        (is (= initial-camera (g/node-value view :camera))
            "Camera should be unchanged when only the base binding's action is dispatched.")

        (let [input-state (reduce
                           (partial dispatch-action! view)
                           (input/make-input-state)
                           [(action :mouse-moved 64.0 64.0 :secondary [:control])
                            (action :mouse-pressed 64.0 64.0 :secondary [:control])
                            (action :drag-detected 64.0 64.0 :secondary [:control])])]
          (is (= :track (:movement (g/user-data camera-controller :editor.camera/camera-state))))
          (let [input-state (dispatch-action! view input-state (action :mouse-moved 80.0 64.0 :secondary [:control]))
                input-state (update-tick! view input-state 2)]
            (dispatch-action! view input-state (action :mouse-released 80.0 64.0 :secondary [:control]))))

        (is (not= initial-camera (g/node-value view :camera))
            "Camera should have panned via the overridden binding.")))))

(deftest empty-camera-binding-override-disables-default-pan
  (test-util/with-loaded-project
    (with-mouse-bindings
      (mouse-binding/register!
        ::camera/scene-camera-orthographic
        "Scene 2D Camera"
        [{:command :scene.camera.pan
          :action ["Pan"]
          :binding {:button :primary :modifiers #{:shift}}}])
      (mouse-binding/set-user-overrides!
        {::camera/scene-camera-orthographic
         {:scene.camera.pan
          {:bindings []}}})
      (let [[_ view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)
            camera-ctrl (camera-controller view)
            initial-camera (g/node-value view :camera)
            input-state (reduce
                          (partial dispatch-action! view)
                          (input/make-input-state)
                          [(action :mouse-moved 64.0 64.0 :primary [:shift])
                           (action :mouse-pressed 64.0 64.0 :primary [:shift])
                           (action :drag-detected 64.0 64.0 :primary [:shift])])]
        (is camera-ctrl)
        (is (not= :track (:movement (g/user-data camera-ctrl :editor.camera/camera-state))))
        (dispatch-action! view input-state (action :mouse-released 64.0 64.0 :primary [:shift]))
        (is (= initial-camera (g/node-value view :camera))
            "Camera should stay unchanged when the override explicitly clears all pan bindings.")))))
