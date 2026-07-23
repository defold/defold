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

(ns integration.scene-resource-prefs-test
  "Per-resource scene view state: camera, visibility filters, and the project's
  split 2D/3D grid presets."
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.camera :as camera]
            [editor.fs :as fs]
            [editor.grid :as grid]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.scene :as scene]
            [editor.scene-visibility :as scene-visibility]
            [integration.test-util :as test-util])
  (:import [javax.vecmath Point3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private camera-prefs #'app-view/camera-prefs)
(def ^:private try-load-camera-from-prefs #'app-view/try-load-camera-from-prefs)
(def ^:private set-visibility-settings! #'scene-visibility/set-visibility-settings!)
(def ^:private grid-mode #'grid/grid-mode)

(def ^:private resource-settings-path [:scene :resource-settings])

(defn- make-isolated-prefs
  "prefs/set! mutates a process-global atom keyed by file path, and the shared
  test prefs file would let resource-settings leak between tests, which is
  exactly what the isolation tests below are checking. Give each test its own."
  []
  (let [file (fs/create-temp-file! "scene-resource-prefs-test" ".editor_settings")]
    (prefs/make :scopes {:global file :project file} :schemas [:default])))

(defn- store-camera! [prefs proj-path camera]
  (prefs/set-pref-entry-in! prefs resource-settings-path proj-path [:camera]
                            (camera-prefs camera)))

(defn- visibility-settings [scene-visibility]
  (g/with-auto-evaluation-context evaluation-context
    (scene-visibility/settings scene-visibility evaluation-context)))

;; -----------------------------------------------------------------------------
;; Camera <-> prefs
;; -----------------------------------------------------------------------------

(deftest camera-survives-a-prefs-round-trip
  (doseq [[label camera]
          [["orthographic"
            (assoc (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0})
              :position (Point3d. 12.5 -3.25 900.0)
              :focus-point (Vector4d. 1.0 2.0 3.0 1.0))]

           ["perspective, rotated off the front view"
            (assoc (camera/make-camera :perspective identity {:fov-x 54.4 :fov-y 37.8})
              :position (Point3d. -40.0 15.0 220.0)
              :rotation (math/euler->quat [0.0 45.0 0.0]))]]]
    (testing label
      (let [prefs (make-isolated-prefs)]
        (store-camera! prefs "/a.collection" camera)
        ;; try-load-camera-from-prefs rebuilds the record via make-camera and
        ;; restores only the persisted fields, so the whole Camera won't be = to
        ;; the original. Assert the restored camera serializes back identically.
        (is (= (camera-prefs camera)
               (camera-prefs (try-load-camera-from-prefs prefs "/a.collection"))))))))

(deftest incomplete-stored-camera-falls-back-to-the-default
  (let [prefs (make-isolated-prefs)
        camera (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0})]
    (testing "nothing stored for the resource"
      (is (nil? (try-load-camera-from-prefs prefs "/never-opened.collection"))))

    (testing "an entry exists but is missing a field"
      (doseq [dropped-key [:projection :position :rotation :fov-x :fov-y :focus-point]]
        (prefs/set-pref-entry-in! prefs resource-settings-path "/a.collection" [:camera]
                                  (dissoc (camera-prefs camera) dropped-key))
        (is (nil? (try-load-camera-from-prefs prefs "/a.collection"))
            (str "a camera missing " dropped-key " should be ignored, not partially restored"))))))

(deftest resource-entries-do-not-clobber-each-other
  ;; Every write rewrites the whole :object-of map, so a bad update would drop
  ;; sibling resources or the sibling key under the same resource.
  (let [prefs (make-isolated-prefs)
        collection-camera (assoc (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0})
                            :position (Point3d. 10.0 20.0 30.0))
        go-camera (assoc (camera/make-camera :perspective identity {:fov-x 54.4 :fov-y 37.8})
                    :position (Point3d. -5.0 -6.0 -7.0))]
    (store-camera! prefs "/a.collection" collection-camera)
    (store-camera! prefs "/b.go" go-camera)
    (prefs/set-pref-entry-in! prefs resource-settings-path "/a.collection" [:scene-visibility]
                              {:filters-enabled false :filtered-renderable-tags #{:sprite}})

    (testing "each resource keeps its own camera"
      (is (= [10.0 20.0 30.0] (:position (camera-prefs (try-load-camera-from-prefs prefs "/a.collection")))))
      (is (= [-5.0 -6.0 -7.0] (:position (camera-prefs (try-load-camera-from-prefs prefs "/b.go"))))))

    (testing "camera and visibility coexist under one resource"
      (is (= :orthographic (:projection (camera-prefs (try-load-camera-from-prefs prefs "/a.collection")))))
      (is (= {:filters-enabled false :filtered-renderable-tags #{:sprite}}
             (prefs/get-pref-entry-in prefs resource-settings-path "/a.collection" [:scene-visibility] nil))))))

;; -----------------------------------------------------------------------------
;; Visibility filters
;; -----------------------------------------------------------------------------

(deftest visibility-settings-load-per-resource
  (test-util/with-loaded-project
    (let [prefs (make-isolated-prefs)
          scene-visibility (scene-visibility/make-scene-visibility-node!
                             (test-util/make-view-graph!) prefs app-view)
          stored {:filters-enabled false :filtered-renderable-tags #{:sprite :model}}]
      (prefs/set-pref-entry-in! prefs resource-settings-path "/logic/atlas_sprite.collection" [:scene-visibility] stored)

      (testing "a resource with stored settings gets them back"
        (scene-visibility/load-settings! scene-visibility prefs "/logic/atlas_sprite.collection")
        (is (= stored (visibility-settings scene-visibility))))

      (testing "a resource with nothing stored resets to the defaults, not the previous resource's state"
        (scene-visibility/load-settings! scene-visibility prefs "/logic/atlas_sprite.go")
        (is (= scene-visibility/default-settings (visibility-settings scene-visibility)))))))

(deftest visibility-settings-persist-against-the-active-resource
  (test-util/with-loaded-project
    (let [prefs (make-isolated-prefs)
          scene-visibility (scene-visibility/make-scene-visibility-node!
                             (test-util/make-view-graph!) prefs app-view)
          collection-path "/logic/atlas_sprite.collection"
          go-path "/logic/atlas_sprite.go"
          stored-tags (fn [proj-path]
                        (prefs/get-pref-entry-in prefs resource-settings-path proj-path
                                                 [:scene-visibility :filtered-renderable-tags] nil))]
      (g/transact (g/connect app-view :active-resource-node+type scene-visibility :active-resource-node+type))

      (test-util/open-scene-view! project app-view collection-path 128 128 {:prefs prefs})
      (set-visibility-settings! scene-visibility #(update % :filtered-renderable-tags conj :sprite))

      (test-util/open-scene-view! project app-view go-path 128 128 {:prefs prefs})
      (scene-visibility/load-settings! scene-visibility prefs go-path)
      (set-visibility-settings! scene-visibility #(update % :filtered-renderable-tags conj :tilemap))

      (testing "each resource's toggles landed under its own key"
        (is (contains? (stored-tags collection-path) :sprite))
        (is (not (contains? (stored-tags collection-path) :tilemap)))
        (is (contains? (stored-tags go-path) :tilemap))
        (is (not (contains? (stored-tags go-path) :sprite))))

      (testing "switching back restores the first resource's state"
        (scene-visibility/load-settings! scene-visibility prefs collection-path)
        (let [tags (:filtered-renderable-tags (visibility-settings scene-visibility))]
          (is (contains? tags :sprite))
          (is (not (contains? tags :tilemap))))))))

(deftest visibility-settings-are-not-persisted-without-an-active-scene
  ;; The popup's toggle callbacks are not guarded by a handler active? predicate,
  ;; so a toggle can fire while no scene resource is active. The view state should
  ;; still update; it just must not invent a prefs entry.
  (test-util/with-loaded-project
    (let [prefs (make-isolated-prefs)
          scene-visibility (scene-visibility/make-scene-visibility-node!
                             (test-util/make-view-graph!) prefs app-view)]
      (set-visibility-settings! scene-visibility #(assoc % :filters-enabled false))
      (is (false? (:filters-enabled (visibility-settings scene-visibility)))
          "the toggle should still take effect in the view")
      (is (= {} (prefs/get prefs resource-settings-path))
          "no resource entry should have been written"))))

;; -----------------------------------------------------------------------------
;; Grid presets
;; -----------------------------------------------------------------------------

(deftest grid-mode-follows-the-view-not-just-the-projection
  (let [orthographic (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0})]
    (is (= :grid-2d (grid-mode orthographic)))
    (is (= :grid-3d (grid-mode (camera/make-camera :perspective identity {:fov-x 54.4 :fov-y 37.8}))))

    (testing "an orthographic camera rotated off the front view is a 3D view"
      (is (= :grid-3d (grid-mode (assoc orthographic :rotation (math/euler->quat [0.0 45.0 0.0]))))))))

(deftest grid-reads-the-preset-its-camera-selects
  (test-util/with-loaded-project
    (let [prefs (make-isolated-prefs)
          [_ view-id] (test-util/open-scene-view! project app-view "/logic/atlas_sprite.collection" 128 128 {:prefs prefs})
          camera-id (scene/view->camera view-id)
          ;; A preview view has no Grid: setup-view only attaches one when :grid
          ;; is in the view opts, and make-preview dissocs it. Attach a Grid to
          ;; the view's real CameraController the way attach-grid does, so the
          ;; camera we read is the one produce-camera actually hands the grid.
          grid-id (first
                    (g/tx-nodes-added
                      (g/transact
                        (g/make-nodes (g/node-id->graph-id view-id) [grid [grid/Grid :prefs prefs]]
                          (g/connect camera-id :camera grid :camera)))))
          merged-options #(g/node-value grid-id :merged-options)
          set-camera! #(g/set-property! camera-id :local-camera %)]
      (prefs/set! prefs [:scene :grid-2d :opacity] 0.125)
      (prefs/set! prefs [:scene :grid-3d :opacity] 0.875)

      (testing "a front-on orthographic camera uses the 2D preset"
        (set-camera! (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0}))
        (is (= 0.125 (:opacity (merged-options)))))

      (testing "a perspective camera uses the 3D preset"
        (set-camera! (camera/make-camera :perspective identity {:fov-x 54.4 :fov-y 37.8}))
        (is (= 0.875 (:opacity (merged-options)))))

      (testing "switching back picks the 2D preset up again"
        (set-camera! (camera/make-camera :orthographic identity {:fov-x 1000.0 :fov-y 1000.0}))
        (is (= 0.125 (:opacity (merged-options)))))

      (testing "2D mode uses the configured plane rather than forcing :z"
        (prefs/set! prefs [:scene :grid-2d :active-plane] :x)
        (is (= :x (:active-plane (merged-options))))))))
