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
            [integration.test-util :as test-util])
  (:import [javax.vecmath Point3d]))

(defn- annotate-action [view action]
  (if (and (g/node-value view :mouse-binding-context)
           (#{:mouse-pressed :mouse-moved} (:type action)))
    (let [context (g/node-value view :mouse-binding-context)
          command (mouse-binding/command-for-action context (assoc action :type :drag))]
      (cond-> action
        command (assoc :mouse-binding-command command)))
    action))

(defn- update-tick! [view input-state]
  (g/with-auto-evaluation-context evaluation-context
    (reduce
      (fn [input-state [node-id label]]
        (when input-state
          ((g/node-value node-id label evaluation-context) node-id input-state (/ 1.0 60.0))))
      input-state
      (g/sources-of (:basis evaluation-context) view :update-tick-handlers))))

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
  (let [input-state (input/update-input-state input-state action)
        action (->> action
                    (scene/augment-action view)
                    (annotate-action view))]
    (scene/dispatch-input
      (g/sources-of view :input-handlers)
      input-state
      action
      (g/node-value view :selected-tool-renderables))
    (update-tick! view input-state)
    input-state))

(defn- drag! [view [x0 y0] [x1 y1] button modifiers]
  (reduce
    (partial dispatch-action! view)
    (input/make-input-state)
    [(action :mouse-moved x0 y0 button modifiers)
     (action :mouse-pressed x0 y0 button modifiers)
     (action :drag-detected x0 y0 button modifiers)
     (action :mouse-moved x1 y1 button modifiers)
     (action :mouse-released x1 y1 button modifiers)]))

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

(defn- tile-screen-pos [view resource-node [tile-x tile-y]]
  (let [[tile-width tile-height] (g/node-value resource-node :tile-dimensions)
        world-pos (Point3d. (* (+ tile-x 0.5) tile-width)
                            (* (+ tile-y 0.5) tile-height)
                            0.0)
        screen-pos (camera/camera-project (g/node-value view :camera)
                                          (g/node-value view :viewport)
                                          world-pos)]
    [(.x screen-pos) (.y screen-pos)]))

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

(defn- cell-at [layer-node [x y]]
  (reduce-kv
    (fn [_ _ {cell-x :x cell-y :y :as cell}]
      (when (and (= x cell-x) (= y cell-y))
        (reduced cell)))
    nil
    (g/node-value layer-node :cell-map)))

(defn- with-mouse-bindings* [f]
  (let [old-bindings @mouse-binding/bindings-atom]
    (try
      (f)
      (finally
        (reset! mouse-binding/bindings-atom old-bindings)))))

(defmacro with-mouse-bindings [& forms]
  `(with-mouse-bindings* (fn [] ~@forms)))

(defn- dirty-delta [project baseline]
  (reduce disj
          (test-util/dirty-proj-paths project)
          baseline))

(deftest tile-map-primary-drag-paints-without-dirtying-other-open-resources
  (test-util/with-loaded-project
    (let [[collection-node] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)
          [tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
          layer-node (layer-node tile-map-node)
          tile [0 0]
          collection-save-value (g/node-value collection-node :save-value)
          screen-pos (tile-screen-pos view tile-map-node tile)]
      (app-view/select! app-view [layer-node])
      (refresh-selection! view)
      (test-util/clear-cached-save-data! project)
      (let [dirty-baseline (test-util/dirty-proj-paths project)]
        (is (= [layer-node] (g/node-value app-view :selected-node-ids)))
        (is (tile-map-controller view))
        (is (nil? (cell-at layer-node tile)))
        (drag! view screen-pos screen-pos :primary [])
        (is (cell-at layer-node tile))
        (is (= #{"/tilegrid/with_layers.tilemap"} (dirty-delta project dirty-baseline)))
        (is (= collection-save-value (g/node-value collection-node :save-value)))))))

(deftest tile-map-secondary-rebind-erases-instead-of-panning-camera
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
          :binding {:button :secondary :trigger :drag :modifiers []}}])
      (is (= :scene.tile-map.erase
             (mouse-binding/command-for-action
               ::tile-map/tile-map-editor
               {:type :drag
                :button :secondary})))
      (let [[collection-node] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)
            [tile-map-node view] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
            layer-node (layer-node tile-map-node)
            tile [-2 -2]
            collection-save-value (g/node-value collection-node :save-value)
            screen-pos (tile-screen-pos view tile-map-node tile)]
        (app-view/select! app-view [layer-node])
        (refresh-selection! view)
        (test-util/clear-cached-save-data! project)
        (let [dirty-baseline (test-util/dirty-proj-paths project)]
          (is (= [layer-node] (g/node-value app-view :selected-node-ids)))
          (is (tile-map-controller view))
          (is (cell-at layer-node tile))
          (let [input-state (reduce
                              (partial dispatch-action! view)
                              (input/make-input-state)
                              [(action :mouse-moved (first screen-pos) (second screen-pos) :secondary [])
                               (action :mouse-pressed (first screen-pos) (second screen-pos) :secondary [])])]
            (is (= :select (g/node-value (tile-map-controller view) :op)))
            (dispatch-action! view input-state (action :mouse-released (first screen-pos) (second screen-pos) :secondary []))
            (is (nil? (g/node-value (tile-map-controller view) :op)))
            (is (nil? (cell-at layer-node tile)))
            (is (= #{"/tilegrid/with_layers.tilemap"} (dirty-delta project dirty-baseline)))
            (is (= collection-save-value (g/node-value collection-node :save-value)))))))))

(deftest collection-secondary-drag-pans-camera-without-dirtying-open-resources
  (test-util/with-loaded-project
    (let [[tile-map-node] (open-tile-map-scene-view! project app-view "/tilegrid/with_layers.tilemap" 128 128)
          [collection-node view] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128)
          initial-camera (g/node-value view :camera)
          tile-map-save-value (g/node-value tile-map-node :save-value)
          collection-save-value (g/node-value collection-node :save-value)]
      (test-util/clear-cached-save-data! project)
      (let [dirty-baseline (test-util/dirty-proj-paths project)]
        (drag! view [64.0 64.0] [80.0 64.0] :secondary [])
        (is (not= initial-camera (g/node-value view :camera)))
        (is (= #{} (dirty-delta project dirty-baseline)))
        (is (= tile-map-save-value (g/node-value tile-map-node :save-value)))
        (is (= collection-save-value (g/node-value collection-node :save-value)))))))
