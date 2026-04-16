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

(ns editor.scene
  (:require [cljfx.api :as fx]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.text-area :as fx.text-area]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.grid :as grid]
            [editor.handler :as handler]
            [editor.input :as i]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.os :as os]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rulers :as rulers]
            [editor.scene-async :as scene-async]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.scene-selection :as selection]
            [editor.scene-shapes :as scene-shapes]
            [editor.scene-text :as scene-text]
            [editor.scene-tools :as scene-tools]
            [editor.scene-visibility :as scene-visibility]
            [editor.system :as system]
            [editor.shaders :as shaders]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.profiler :as profiler])
  (:import [com.jogamp.opengl GL GL2 GLAutoDrawable GLContext GLOffscreenAutoDrawable]
           [com.jogamp.opengl.glu GLU]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [editor.pose Pose]
           [editor.types AABB Camera Rect Region]
           [java.awt.image BufferedImage]
           [java.lang Math Runnable]
           [java.nio IntBuffer]
           [javafx.beans.value ChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent]
           [javafx.geometry HPos VPos]
           [javafx.scene Cursor Node Parent Scene]
           [javafx.scene.image ImageView WritableImage]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout AnchorPane Pane]
           [javafx.stage Window]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]
           [sun.awt.image IntegerComponentRaster]))

(set! *warn-on-reflection* true)

(def default-position protobuf/vector3-zero)

(def default-rotation protobuf/quat-identity)

(def default-scale protobuf/vector3-one)

(def identity-transform-properties
  {:position default-position
   :rotation default-rotation
   :scale default-scale})

(def ^:private camera-inset-margin 18.0)
(def ^:private camera-inset-width 240.0)
(def ^:private camera-inset-height 135.0)
(def ^:private camera-inset-render-scale 2.0)

(defn significant-scale? [value]
  (cond
    (nil? value)
    false

    (vector? value)
    (not= default-scale value)

    (number? value)
    (not= 1.0 value)

    :else
    (throw (ex-info "Unsupported scale value."
                    {:value value}))))

(defn displayed-node-properties [selected-node-properties preview-overrides]
  (if (or (coll/empty? selected-node-properties)
          (coll/empty? preview-overrides))
    selected-node-properties
    (coll/into-> selected-node-properties []
      (map (fn [selected-node-property]
             (let [node-id (:node-id selected-node-property)
                   prop-kw->override-value (get preview-overrides node-id)]
               (if (coll/empty? prop-kw->override-value)
                 selected-node-property
                 (update
                   selected-node-property :properties
                   (fn [prop-kw->prop-info]
                     (coll/reduce-kv-> prop-kw->override-value prop-kw->prop-info
                       (fn [prop-kw->prop-info prop-kw override-value]
                         (if (contains? prop-kw->prop-info prop-kw)
                           (assoc-in prop-kw->prop-info [prop-kw :value] override-value)
                           prop-kw->prop-info))))))))))))

(defn- get-resource-name [node-id]
  (when-let [resource (some-> node-id resource-node/owner-resource)]
    (resource/resource-name resource)))

(def ^:private error-render-title-message
  (localization/message "error.render.title"))

(def ^:private error-render-unknown-resource-message
  (localization/message "error.render.unknown-resource"))

(def ^:private error-render-and-more-message
  (localization/message "error.render.and-more"))

(defn- error-message-lines [error-values localization-state]
  (let [max-error-count 15 ; We limit the number of errors since traversing deep trees is slow.

        distinct-errors
        (coll/into-> error-values []
          (coll/tree-xf :causes :causes)
          (filter :message)
          (remove :causes)
          (map #(select-keys % [:_node-id :message]))
          (distinct)
          (take (inc max-error-count))) ; Produce one more error than we'll use so we'll know if we're over the limit.

        error-message-lines
        (coll/into-> distinct-errors [(localization-state error-render-title-message)
                                      ""]
          (take max-error-count)
          (map (fn [{:keys [_node-id message]}]
                 (let [resource-name (or (get-resource-name _node-id)
                                         (localization-state error-render-unknown-resource-message))]
                   (str "- " resource-name ": " (localization-state message))))))]

    (cond-> error-message-lines
            (< max-error-count (count distinct-errors))
            (conj (localization-state error-render-and-more-message)))))

(defn- render-error
  [gl render-args _renderables _nrenderables]
  (when (= pass/overlay (:pass render-args))
    (scene-text/overlay gl "RENDER ERROR" 24.0 -22.0)))

(defn substitute-render-data
  [error]
  [{pass/overlay [{:render-fn render-error
                   :user-data {:error error}
                   :batch-key ::error}]}])

(defn substitute-scene [error]
  {:aabb geom/null-aabb
   :error error})

;; Avoid recreating the image each frame
(defonce ^:private cached-buf-img-ref (atom nil))
(defonce ^:private cached-camera-inset-buf-img-ref (atom nil))

;; Replacement for Screenshot/readToBufferedImage but without expensive y-axis flip.
;; We flip in JavaFX instead
(defn- read-to-buffered-image
  ([^long w ^long h]
   (read-to-buffered-image cached-buf-img-ref w h))
  ([cache-ref ^long w ^long h]
   (let [^BufferedImage image (let [^BufferedImage image @cache-ref]
                               (when (or (nil? image) (not= (.getWidth image) w) (not= (.getHeight image) h))
                                 (reset! cache-ref (BufferedImage. w h BufferedImage/TYPE_INT_ARGB_PRE)))
                               @cache-ref)
        glc (GLContext/getCurrent)
        gl (.getGL glc)
        psm (GLPixelStorageModes.)]
     (.setPackAlignment psm gl 1)
     (.glReadPixels gl 0 0 w h GL2/GL_BGRA GL/GL_UNSIGNED_BYTE (IntBuffer/wrap (.getDataStorage ^IntegerComponentRaster (.getRaster image))))
     (.restore psm gl)
     image)))

(defn- camera-renderable? [renderable]
  (contains? (:tags renderable) :camera))

(defn- camera-inset-clear-color [selected-camera-renderable]
  (let [[clear-r clear-g clear-b clear-a] (:render-clear-color (:user-data selected-camera-renderable))]
    [(double (or clear-r 0.0))
     (double (or clear-g 0.0))
     (double (or clear-b 0.0))
     (double (or clear-a 1.0))]))

(defn- valid-camera-inset-orthographic-zoom? [selected-camera-renderable]
  (let [{:keys [is-orthographic orthographic-mode orthographic-zoom]} (:user-data selected-camera-renderable)]
    (or (not is-orthographic)
        (not= orthographic-mode :ortho-mode-fixed)
        (> (double (or orthographic-zoom 0.0)) 0.0))))

(defn- camera-inset-aspect-ratio [selected-camera-renderable]
  (let [{:keys [is-orthographic auto-aspect-ratio aspect-ratio display-width display-height]} (:user-data selected-camera-renderable)
        project-width (double (or display-width 0.0))
        project-height (double (or display-height 0.0))]
    (if (or is-orthographic auto-aspect-ratio)
      (when (and (pos? project-width) (pos? project-height))
        (/ project-width project-height))
      (let [camera-aspect-ratio (double (or aspect-ratio 0.0))]
        (when (pos? camera-aspect-ratio)
          camera-aspect-ratio)))))

(defn- make-camera-inset-camera
  ([selected-camera-renderable]
   (make-camera-inset-camera selected-camera-renderable camera-inset-width camera-inset-height))
  ([selected-camera-renderable _inset-width _inset-height]
   (when (valid-camera-inset-orthographic-zoom? selected-camera-renderable)
     (when-let [aspect-ratio (camera-inset-aspect-ratio selected-camera-renderable)]
       (let [{:keys [is-orthographic near-z far-z fov display-height orthographic-zoom orthographic-mode]} (:user-data selected-camera-renderable)
             world-translation ^Vector3d (:world-translation selected-camera-renderable)
             world-rotation ^Quat4d (:world-rotation selected-camera-renderable)
             fov-y (double (or fov c/fov-y-35mm-full-frame))
             fov-x (* fov-y aspect-ratio)
             zoom (double (or orthographic-zoom 1.0))
             zoom (if (= orthographic-mode :ortho-mode-fixed)
                    zoom
                    1.0)
             source-height (double (or display-height camera-inset-height))
             oh (/ source-height zoom)
             ow (* oh aspect-ratio)
             camera-type (if is-orthographic :orthographic :perspective)
             camera-fov-x (if is-orthographic ow fov-x)
             camera-fov-y (if is-orthographic oh fov-y)
             camera (c/make-camera camera-type identity {:fov-x camera-fov-x
                                                         :fov-y camera-fov-y})]
         (assoc camera
           :position (Point3d. (.x world-translation) (.y world-translation) (.z world-translation))
           :rotation (Quat4d. world-rotation)
           :z-near (double (or near-z 1.0))
           :z-far (double (or far-z 1000.0))))))))

(defn- camera-inset-dimensions [selected-camera-renderable]
  (when (valid-camera-inset-orthographic-zoom? selected-camera-renderable)
    (when-let [aspect-ratio (camera-inset-aspect-ratio selected-camera-renderable)]
      (let [display-height camera-inset-height
            display-width (* display-height aspect-ratio)]
        {:display-width display-width
         :display-height display-height
         :render-width (* display-width camera-inset-render-scale)
         :render-height (* display-height camera-inset-render-scale)}))))

(defn- remove-camera-outline-renderables [all-renderables]
  (into {}
        (map (fn [[render-pass renderables]]
               [render-pass
                (into [] (remove #(or (contains? (:tags %) :camera)
                                      (contains? (:tags %) :outline)))
                      renderables)]))
        all-renderables))

(defn vp-dims [^Region viewport]
  (types/dimensions viewport))

(defn- render-camera-inset-border! [^GL2 gl ^Region viewport]
  (let [border-shader shaders/basic-color-local-space
        vertex-description (shaders/vertex-description border-shader)
        [viewport-width viewport-height] (vp-dims viewport)
        border-inset-pixels 1.0
        border-min-x (- 1.0 (/ (* 2.0 border-inset-pixels) viewport-width))
        border-max-x (- border-min-x)
        border-min-y (- 1.0 (/ (* 2.0 border-inset-pixels) viewport-height))
        border-max-y (- border-min-y)
        border-z 0.0
        [border-r border-g border-b border-a] colors/selected-outline-color
        border-vbuf (vtx/make-vertex-buffer vertex-description :stream 8)
        border-buf (vtx/buf border-vbuf)
        border-line-points [[border-min-x border-min-y border-z]
                            [border-max-x border-min-y border-z]
                            [border-max-x border-min-y border-z]
                            [border-max-x border-max-y border-z]
                            [border-max-x border-max-y border-z]
                            [border-min-x border-max-y border-z]
                            [border-min-x border-max-y border-z]
                            [border-min-x border-min-y border-z]]
        border-binding (vtx/use-with ::camera-inset-border border-vbuf border-shader)
        render-args (math/derive-render-transforms geom/Identity4d geom/Identity4d geom/Identity4d geom/Identity4d)]
    (doseq [[x y z] border-line-points]
      (vtx/buf-push-floats! border-buf [x y z border-r border-g border-b border-a]))
    (vtx/flip! border-vbuf)
    (.glDisable gl GL/GL_DEPTH_TEST)
    (.glLineWidth gl 2.0)
    (gl/with-gl-bindings gl render-args [border-shader border-binding]
      (gl/gl-draw-arrays gl GL2/GL_LINES 0 (count border-vbuf)))
    (.glLineWidth gl 1.0)))

(defn- produce-selected-camera-renderable [scene-render-data selection]
  (let [selection-set (set selection)
        selected-cameras
        (into []
              (comp cat
                    (filter camera-renderable?)
                    (filter (fn [renderable]
                              (or (contains? selection-set (:node-id renderable))
                                  (some selection-set (:node-id-path renderable))))))
              (vals (:renderables scene-render-data)))]
    (when (= 1 (count selected-cameras))
      (first selected-cameras))))

(defn- back-to-front-depth-sort-value [^Matrix4d view-matrix ^Vector3d world-translation]
  (let [world-position (Point3d. world-translation)]
    (.transform view-matrix world-position)
    (.z world-position)))

(defn- render-key [^Matrix4d view-matrix ^Vector3d world-translation index is-topmost]
  [(boolean is-topmost)
   (if is-topmost Double/POSITIVE_INFINITY (back-to-front-depth-sort-value view-matrix world-translation))
   (or index 0)])

(defn- outline-render-key [^Matrix4d view-matrix ^Vector3d world-translation index is-topmost selection-state]
  ;; Draw selection outlines on top of other outlines.
  (into [(case selection-state
           :self-selected 2
           :parent-selected 1
           0)]
        (render-key view-matrix world-translation index is-topmost)))

(defn gl-viewport [^GL2 gl ^Region viewport]
  (.glViewport gl (.left viewport) (.top viewport) (- (.right viewport) (.left viewport)) (- (.bottom viewport) (.top viewport))))

(defn setup-pass
  [^GL2 gl pass render-args]
  (let [glu (GLU.)]
    (.glMatrixMode gl GL2/GL_PROJECTION)
    (gl/gl-load-matrix-4d gl (:projection render-args))
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (gl/gl-load-matrix-4d gl (:world-view render-args))
    (pass/prepare-gl pass gl glu)))

(defn- render-nodes
  [^GL2 gl render-args [first-renderable :as renderables] count]
  (when-let [render-fn (:render-fn first-renderable)]
    (try
      (let [shared-world-transform (:world-transform first-renderable math/identity-mat4) ; rulers apparently don't have world-transform
            shared-world-rotation (:world-rotation first-renderable math/identity-quat)

            shared-render-args
            (-> render-args
                (assoc
                  :actual/world-rotation shared-world-rotation
                  :world-rotation shared-world-rotation)
                (coll/merge
                  (math/derive-render-transforms
                    shared-world-transform
                    (:view render-args)
                    (:projection render-args)
                    (:texture render-args))))]

        (render-fn gl shared-render-args renderables count))
      (catch Exception e
        (log/error :message "skipping renderable"
                   :pass-name (:name (:pass render-args))
                   :render-fn render-fn
                   :exception e
                   :ex-data (ex-data e))))))

(defn batch-render [gl render-args renderables key-fn]
  (loop [renderables renderables
         offset 0
         batch-index 0
         batches (transient [])]
    (if-let [renderable (first renderables)]
      (let [first-key (key-fn renderable)
            first-render-fn (:render-fn renderable)
            batch-count (loop [renderables (rest renderables)
                               batch-count 1]
                          (let [renderable (first renderables)
                                key (key-fn renderable)
                                render-fn (:render-fn renderable)
                                break? (or (not= first-render-fn render-fn)
                                           (nil? first-key)
                                           (nil? key)
                                           (not= first-key key))]
                            (if break?
                              batch-count
                              (recur (rest renderables) (inc batch-count)))))]
        (when (> batch-count 0)
          (let [batch (subvec renderables 0 batch-count)]
            (render-nodes gl render-args batch batch-count))
          (let [end (+ offset batch-count)]
            ;; TODO - long conversion should not be necessary?
            (recur (subvec renderables batch-count) (long end) (inc batch-index) (conj! batches [offset end])))))
      (persistent! batches))))

(defn render-sort [renderables]
  (sort-by :render-key renderables))

(defn- resort-renderables-for-camera [renderables-by-pass ^Camera camera]
  (let [view-matrix (c/camera-view-matrix camera)]
    (into {}
          (map (fn [[render-pass renderables]]
                 [render-pass
                  (->> renderables
                       (mapv (fn [renderable]
                               (let [world-translation ^Vector3d (:world-translation renderable)
                                     index (:index renderable)
                                     topmost? (:topmost? renderable)
                                     selection-state (:selected renderable)]
                                 (cond-> (assoc renderable :render-key (render-key view-matrix world-translation index topmost?))
                                   (contains? (:pass-overrides renderable) pass/outline)
                                   (assoc-in [:pass-overrides pass/outline :render-key]
                                             (outline-render-key view-matrix world-translation index topmost? selection-state))))))
                       render-sort)]))
          renderables-by-pass)))

(defn pass-render-args [^Region viewport ^Camera camera pass]
  (let [view (if (types/model-transform? pass)
               (c/camera-view-matrix camera)
               geom/Identity4d)
        camera (if (types/depth-clipping? pass)
                 camera
                 (c/camera-without-depth-clipping camera))
        proj (if (types/model-transform? pass)
               (c/camera-projection-matrix camera)
               (c/region-orthographic-projection-matrix viewport -1.0 1.0)) ; used for background & tile map overlay so no need to use camera settings
        world geom/Identity4d
        texture geom/Identity4d
        transforms (math/derive-render-transforms world view proj texture)]
    (assoc transforms
           :pass pass
           :camera camera
           :viewport viewport)))

(defn- assoc-updatable-states
  [renderables updatable-states]
  (mapv (fn [renderable]
          (if-let [updatable-node-id (get-in renderable [:updatable :node-id])]
            (assoc-in renderable [:updatable :state] (get-in updatable-states [updatable-node-id]))
            renderable))
        renderables))

(defn- picking-render-args [render-args ^Region viewport ^Rect picking-rect]
  (let [{:keys [world view ^Matrix4d projection texture]} render-args
        picking-matrix (c/pick-matrix viewport picking-rect)
        projection' (doto (Matrix4d. picking-matrix) (.mul projection))]
    (merge render-args
           (math/derive-render-transforms world view projection' texture))))

(def render-mode-transitions {:normal :aabbs
                              :aabbs :picking-color
                              :picking-color :picking-rect
                              :picking-rect :normal})
(def last-picking-rect (atom nil))
(def render-mode-passes {:normal pass/render-passes
                         :aabbs pass/render-passes
                         :picking-color pass/selection-passes
                         :picking-rect pass/selection-passes})
(def render-mode-batch-key {:normal :batch-key
                            :aabbs (constantly nil)
                            :picking-color :select-batch-key
                            :picking-rect :select-batch-key})

(defn- make-aabb-renderables [renderables]
  (coll/into-> renderables []
    (keep :aabb)
    (map #(assoc (render-util/make-aabb-outline-renderable #{}) :aabb %))))

(defn render!
  ([^GLContext context render-mode renderables updatable-states viewport pass->render-args]
   (render! context render-mode renderables updatable-states viewport pass->render-args [0.0 0.0 0.0 1.0]))
  ([^GLContext context render-mode renderables updatable-states viewport pass->render-args [clear-r clear-g clear-b clear-a]]
   (let [^GL2 gl (.getGL context)
         batch-key (render-mode-batch-key render-mode)]
     (gl/gl-clear gl clear-r clear-g clear-b clear-a)
     (.glColor4f gl 1.0 1.0 1.0 1.0)
     (gl-viewport gl viewport)
     (doseq [pass (render-mode-passes render-mode)
             :let [pass-render-args (cond-> (pass->render-args pass)
                                      (and (= render-mode :picking-rect)
                                           (some? @last-picking-rect))
                                      (picking-render-args viewport @last-picking-rect))
                   pass-renderables (-> (get renderables pass)
                                        (assoc-updatable-states updatable-states))]]
       (setup-pass gl pass pass-render-args)
       (if (= render-mode :aabbs)
         (batch-render gl pass-render-args (make-aabb-renderables pass-renderables) batch-key)
         (batch-render gl pass-render-args pass-renderables batch-key))))))

(g/defnk produce-camera-inset-data [scene-render-data selection updatable-states camera-inset-frame-version ^GLAutoDrawable camera-inset-drawable]
  (let [hidden-result {:visible false
                       :image nil
                       :width camera-inset-width
                       :height camera-inset-height}]
    (if-some [selected-camera-renderable (produce-selected-camera-renderable scene-render-data selection)]
      (if-some [{:keys [display-width display-height render-width render-height]} (camera-inset-dimensions selected-camera-renderable)]
        (if-some [camera-inset-camera (make-camera-inset-camera selected-camera-renderable render-width render-height)]
          (let [clear-color (camera-inset-clear-color selected-camera-renderable)
                camera-inset-viewport (types/->Region 0 render-width 0 render-height)
                camera-inset-pass->render-args (into {}
                                                    (map (juxt identity (partial pass-render-args camera-inset-viewport camera-inset-camera)))
                                                    pass/all-passes)
                scene-renderables (-> (:renderables scene-render-data)
                                      remove-camera-outline-renderables
                                      (resort-renderables-for-camera camera-inset-camera))
                camera-inset-frame (when camera-inset-drawable
                                     (gl/with-drawable-as-current camera-inset-drawable
                                       (.setSurfaceSize ^GLOffscreenAutoDrawable camera-inset-drawable (int render-width) (int render-height))
                                       (scene-cache/process-pending-deletions! gl)
                                       (render! gl-context :normal scene-renderables updatable-states camera-inset-viewport camera-inset-pass->render-args clear-color)
                                       (render-camera-inset-border! gl camera-inset-viewport)
                                       (.glActiveTexture gl GL/GL_TEXTURE0)
                                       (.glBindTexture gl GL/GL_TEXTURE_2D 0)
                                       (.glBindTexture gl GL2/GL_TEXTURE_CUBE_MAP 0)
                                       (.glUseProgram ^GL2 gl 0)
                                       (let [[w h] (vp-dims camera-inset-viewport)]
                                         (read-to-buffered-image cached-camera-inset-buf-img-ref w h))))
                camera-inset-image (some-> camera-inset-frame
                                           (SwingFXUtils/toFXImage nil))]
            {:visible (some? camera-inset-image)
             :image camera-inset-image
             :width display-width
             :height display-height})
          hidden-result)
        hidden-result)
      hidden-result)))

(defn- apply-pass-overrides
  [pass renderable]
  ;; No nested :pass-overrides like {... :pass-overrides {pass/outline {:pass-overrides {...}}}}
  (-> (merge renderable (get-in renderable [:pass-overrides pass]))
      (dissoc :pass-overrides)))

(defn- make-pass-renderables
  []
  (into {} (map #(vector % (transient []))) pass/all-passes))

(defn- persist-pass-renderables!
  [pass-renderables]
  (into {}
        (map (fn [[pass renderables]]
               [pass (persistent! renderables)]))
        pass-renderables))

(defn- update-pass-renderables!
  [pass-renderables passes flattened-renderable]
  (reduce (fn [pass-renderables pass]
            (update pass-renderables pass conj! (apply-pass-overrides pass flattened-renderable)))
          pass-renderables
          passes))

(defn- flatten-scene-renderables! [flattened-scene
                                   parent-shows-children
                                   apply-transform-preview-overrides
                                   scene
                                   preview-overrides
                                   selection-set
                                   hidden-renderable-tags
                                   hidden-node-outline-key-paths
                                   view-matrix
                                   node-id-path
                                   node-outline-key-path
                                   ^Quat4d parent-world-rotation
                                   ^Vector3d parent-world-scale
                                   ^Matrix4d parent-world-transform
                                   alloc-picking-id!]
  (let [node-id (peek node-id-path)
        renderable (:renderable scene)
        preview-fn (:preview-fn renderable)
        local-translation (Vector3d. 0.0 0.0 0.0)
        local-rotation (Quat4d. 0.0 0.0 0.0 1.0)
        local-scale (Vector3d. 1.0 1.0 1.0)

        prop-kw->override-value
        (let [prop-kw->override-value (get preview-overrides node-id)]
          (if apply-transform-preview-overrides

            ;; Apply transform property overrides, if present.
            (let [position-override (:position prop-kw->override-value)
                  rotation-override (:rotation prop-kw->override-value)
                  scale-override (:scale prop-kw->override-value)]

              ;; No need to extract non-overridden values from the local
              ;; transform if all the transform properties are overridden.
              (when-not (and position-override rotation-override scale-override)
                (when-some [local-transform (:transform scene)]
                  (math/split-mat4 local-transform local-translation local-rotation local-scale)))

              ;; Apply transform property overrides. The clj->vecmath function
              ;; writes directly to the supplied vecmath object.
              (when position-override
                (math/clj->vecmath local-translation position-override))
              (when rotation-override
                (math/clj->vecmath local-rotation rotation-override))
              (when scale-override
                (math/clj->vecmath local-scale scale-override)))

            ;; We're not applying transform property preview overrides. Simply
            ;; use the local transform, if present.
            (when-some [local-transform (:transform scene)]
              (math/split-mat4 local-transform local-translation local-rotation local-scale)))

          ;; Consume the transform properties to determine if there are
          ;; additional non-transform preview overrides, and we need to invoke
          ;; the preview-fn associated with the renderable, if any.
          (-> prop-kw->override-value
              (dissoc :position :rotation :scale)
              (coll/not-empty)))

        [visibility-aabb user-data]
        (let [visibility-aabb (or (:visibility-aabb scene)
                                  (:aabb scene)
                                  geom/null-aabb)
              user-data (:user-data renderable)]
          (if (and preview-fn prop-kw->override-value)
            (let [result (preview-fn visibility-aabb user-data prop-kw->override-value)]
              (assert (instance? AABB (first result)))
              (when (some? user-data)
                (assert (map? (second result))))
              result)
            (pair visibility-aabb user-data)))

        local-transform (math/->mat4-non-uniform local-translation local-rotation local-scale)
        world-transform (doto (Matrix4d. parent-world-transform) (.mul local-transform))
        world-translation (math/transform parent-world-transform local-translation)
        world-rotation (doto (Quat4d. parent-world-rotation) (.mul local-rotation))
        world-scale (math/multiply-vector parent-world-scale local-scale)
        picking-node-id (or (:picking-node-id scene) node-id)
        selection-state (cond
                          (contains? selection-set node-id) :self-selected ; This node is selected.
                          (some selection-set node-id-path) :parent-selected) ; Child nodes appear dimly selected if their parent is selected.
        is-visible (and parent-shows-children
                        (:visible-self? renderable true)
                        (not (scene-visibility/hidden-outline-key-path? hidden-node-outline-key-paths node-outline-key-path))
                        (not-any? (partial contains? hidden-renderable-tags) (:tags renderable)))
        local-aabb (:aabb scene)
        world-aabb (if (and local-aabb is-visible)
                     (geom/aabb-transform local-aabb world-transform)
                     geom/null-aabb)
        flat-renderable (-> scene
                            (dissoc :children :renderable)
                            (assoc :node-id-path node-id-path
                                   :node-outline-key-path node-outline-key-path
                                   :picking-node-id picking-node-id
                                   :picking-id (alloc-picking-id! picking-node-id)
                                   :tags (:tags renderable)
                                   :render-fn (:render-fn renderable)
                                   :world-translation world-translation
                                   :world-rotation world-rotation
                                   :world-scale world-scale
                                   :world-transform world-transform
                                   :parent-world-transform parent-world-transform
                                   :selected selection-state
                                   :user-data user-data
                                   :batch-key (:batch-key renderable)
                                   :aabb world-aabb
                                   :render-key (render-key view-matrix world-translation (:index renderable) (:topmost? renderable))
                                   :pass-overrides {pass/outline {:render-key (outline-render-key view-matrix world-translation (:index renderable) (:topmost? renderable) selection-state)}}))
        flat-renderable (if is-visible
                          flat-renderable
                          (dissoc flat-renderable :updatable))
        drawn-passes (cond
                       ;; Draw to all passes unless hidden.
                       is-visible
                       (:passes renderable)

                       ;; For selected objects, we always draw the outline and
                       ;; selection passes. This way, the visual part is hidden,
                       ;; but the selection highlight and hit test remains until
                       ;; the object is deselected. If we do not render the
                       ;; selection pass, objects can be deselected by clicking
                       ;; within their outlines, and more importantly, the
                       ;; manipulator disappears, since it aligns to selection
                       ;; pass renderables.
                       selection-state
                       (filterv #(or (= pass/outline %)
                                     (= pass/selection %))
                                (:passes renderable)))
        pass-renderables (update-pass-renderables! (:renderables flattened-scene) drawn-passes flat-renderable)
        scene-aabb (cond-> (:scene-aabb flattened-scene)
                           (and is-visible (not (geom/empty-aabb? visibility-aabb)))
                           (geom/aabb-union (geom/aabb-transform visibility-aabb world-transform)))]
    (reduce (fn [flattened-scene child-scene]
              (let [parent-node-id (:node-id scene)
                    child-node-id (:node-id child-scene)
                    is-child-node-same-as-parent (= parent-node-id child-node-id)
                    child-node-id-path (if is-child-node-same-as-parent
                                         node-id-path
                                         (conj node-id-path child-node-id))
                    child-node-outline-key-path (if is-child-node-same-as-parent
                                                  node-outline-key-path
                                                  (conj node-outline-key-path (:node-outline-key child-scene)))]
                (flatten-scene-renderables! flattened-scene
                                            (and parent-shows-children (:visible-children? renderable true))
                                            (not is-child-node-same-as-parent)
                                            child-scene
                                            preview-overrides
                                            selection-set
                                            hidden-renderable-tags
                                            hidden-node-outline-key-paths
                                            view-matrix
                                            child-node-id-path
                                            child-node-outline-key-path
                                            world-rotation
                                            world-scale
                                            world-transform
                                            alloc-picking-id!)))
            {:renderables pass-renderables
             :scene-aabb scene-aabb}
            (:children scene))))

;; Picking id's are in the range 1..2^24-1. To more easily distinguish
;; consecutive picking seeds (by color) we shuffle the bits by
;; multiplying by picking-id-multiplier modulo 2^24. picking-id-multiplier is
;; coprime to 2^24 so there will be no collisions. picking-id-inverse
;; can be used to find the original picking-seed if need be.
(def ^:private picking-id-multiplier 0x5b1047)
#_(def picking-id-inverse 0x79977)

(defn- picking-seed->picking-id [picking-seed]
  (assert (<= picking-seed 0x00ffffff))
  (let [picking-id (bit-and (* picking-seed picking-id-multiplier) 0x00ffffff)]
    (assert (<= picking-id 0x00ffffff))
    picking-id))

(defn- alloc-picking-id! [node-id->picking-id-atom node-id]
  (if-let [picking-id (get @node-id->picking-id-atom node-id)]
    picking-id
    (get (swap! node-id->picking-id-atom
                (fn [node-id->picking-id]
                  (let [picking-seed (inc (count node-id->picking-id))
                        picking-id (picking-seed->picking-id picking-seed)]
                    (assoc node-id->picking-id node-id picking-id))))
         node-id)))

(defn- flatten-scene [scene preview-overrides selection-set hidden-renderable-tags hidden-node-outline-key-paths view-matrix]
  (let [node-id->picking-id-atom (atom {})
        alloc-picking-id! (partial alloc-picking-id! node-id->picking-id-atom)
        node-id-path []
        node-outline-key-path [(:node-id scene)]
        parent-world-rotation geom/NoRotation
        parent-world-transform geom/Identity4d
        parent-world-scale (Vector3d. 1 1 1)]
    (let [{:keys [renderables scene-aabb]}
          (flatten-scene-renderables! {:renderables (make-pass-renderables)
                                       :scene-aabb geom/null-aabb}
                                      true
                                      true
                                      scene
                                      preview-overrides
                                      selection-set
                                      hidden-renderable-tags
                                      hidden-node-outline-key-paths
                                      view-matrix
                                      node-id-path
                                      node-outline-key-path
                                      parent-world-rotation
                                      parent-world-scale
                                      parent-world-transform
                                      alloc-picking-id!)]
      {:renderables (persist-pass-renderables! renderables)
       :scene-aabb (if (geom/null-aabb? scene-aabb)
                     geom/empty-bounding-box
                     scene-aabb)})))

(defn- get-selection-pass-renderables-by-node-id
  "Returns a map of renderables that were in a selection pass by their node id.
  If a renderable appears in multiple selection passes, the one from the latter
  pass will be picked. This should be fine, since the flat-renderable added
  by update-pass-renderables! will be the same for all passes."
  [renderables-by-pass]
  (into {}
        (comp (filter (comp types/selection? key))
              (mapcat (fn [[_pass renderables]]
                        (map (fn [renderable]
                               [(:node-id renderable) renderable])
                             renderables))))
        renderables-by-pass))

(g/defnk produce-scene-render-data [scene preview-overrides selection hidden-renderable-tags hidden-node-outline-key-paths local-camera]
  (if-let [error (:error scene)]
    {:error error
     :renderables {}
     :selected-renderables []
     :scene-aabb geom/empty-bounding-box}
    (let [selection-set (set selection)
          view-matrix (c/camera-view-matrix local-camera)
          {:keys [renderables] :as flattened-scene} (flatten-scene scene preview-overrides selection-set hidden-renderable-tags hidden-node-outline-key-paths view-matrix)
          selection-pass-renderables-by-node-id (get-selection-pass-renderables-by-node-id renderables)
          selected-renderables (into [] (keep selection-pass-renderables-by-node-id) selection)]
      (assoc flattened-scene :selected-renderables selected-renderables))))

(defn- make-aabb-renderables-by-pass [^AABB aabb color renderable-tag]
  (assert (keyword? renderable-tag))
  (let [^Point3d aabb-min (.min aabb)
        ^Point3d aabb-max (.max aabb)
        ^Vector3d aabb-mid (-> aabb-min
                               (math/add-vector aabb-max)
                               (math/scale-vector 0.5))
        world-transform (doto (Matrix4d.)
                          (.setIdentity)
                          (.setTranslation aabb-mid))
        aabb-ext [(- (.x aabb-max) (.x aabb-mid))
                  (- (.y aabb-max) (.y aabb-mid))
                  (- (.z aabb-max) (.z aabb-mid))]
        point-scale (float-array aabb-ext)]
    {pass/transparent
     [{:world-transform world-transform
       :render-fn scene-shapes/render-triangles
       :tags #{renderable-tag}
       :user-data {:color color
                   :point-scale point-scale
                   :geometry scene-shapes/box-triangles}}]

     pass/outline
     [{:world-transform world-transform
       :render-fn scene-shapes/render-lines
       :tags #{renderable-tag :outline}
       :user-data {:color color
                   :point-scale point-scale
                   :geometry scene-shapes/box-lines}}]}))

(g/defnk produce-internal-renderables [^AABB scene-aabb]
  (make-aabb-renderables-by-pass scene-aabb colors/bright-grey-light :dev-visibility-bounds))

(g/defnk produce-aux-render-data [aux-renderables internal-renderables hidden-renderable-tags]
  (assert (map? internal-renderables))
  (let [additional-renderables-by-pass (apply merge-with concat internal-renderables aux-renderables)
        filtered-additional-renderables-by-pass
        (into {}
              (map (fn [[pass renderables]]
                     [pass (remove #(not-empty (set/intersection hidden-renderable-tags (:tags %)))
                                   renderables)]))
              additional-renderables-by-pass)]
    {:renderables filtered-additional-renderables-by-pass}))

(g/defnk produce-pass->render-args [^Region viewport camera]
  (into {}
        (map (juxt identity (partial pass-render-args viewport camera)))
        pass/all-passes))

(g/defnk produce-renderables-aabb+picking-node-id [scene-render-data]
  (let [renderables-by-pass (:renderables scene-render-data)]
    (into []
          (comp cat
                (keep (fn [renderable]
                        (when-some [aabb (:aabb renderable)]
                          (when-not (geom/null-aabb? aabb)
                            (let [picking-node-id (:picking-node-id renderable)]
                              [aabb picking-node-id]))))))
          [(get renderables-by-pass pass/selection)
           (get renderables-by-pass pass/opaque-selection)])))

(g/defnode SceneRenderer
  (property overlay-anchor-pane AnchorPane (dynamic visible (g/constantly false)))
  (property render-mode g/Keyword (default :normal))

  (input active-view g/NodeID)
  (input scene g/Any :substitute substitute-scene)
  (input preview-overrides g/Any)
  (input selection g/Any)
  (input local-camera Camera)
  (input camera Camera)
  (input aux-renderables pass/RenderData :array :substitute gu/array-subst-remove-errors)
  (input hidden-renderable-tags types/RenderableTags)
  (input hidden-node-outline-key-paths types/NodeOutlineKeyPaths)
  (input cursor-type g/Keyword)

  (output viewport Region :abstract)
  (output all-renderables g/Any :abstract)

  (output camera-type g/Keyword (g/fnk [camera] (:type camera)))
  (output internal-renderables g/Any :cached produce-internal-renderables)
  (output scene-render-data g/Any :cached produce-scene-render-data)
  (output aux-render-data g/Any :cached produce-aux-render-data)

  (output selected-renderables g/Any (g/fnk [scene-render-data] (:selected-renderables scene-render-data)))
  (output scene-aabb AABB (g/fnk [scene-render-data] (:scene-aabb scene-render-data)))
  (output selected-aabb AABB :cached (g/fnk [scene-render-data]
                                       (let [selected-aabbs (sequence (comp (mapcat val)
                                                                            (filter :selected)
                                                                            (map :aabb))
                                                                      (:renderables scene-render-data))]
                                         (if (seq selected-aabbs)
                                           (reduce geom/aabb-union geom/null-aabb selected-aabbs)
                                           (:scene-aabb scene-render-data)))))
  (output renderables-aabb+picking-node-id g/Any :cached produce-renderables-aabb+picking-node-id)
  (output selected-updatables g/Any :cached (g/fnk [selected-renderables]
                                              (into {}
                                                    (comp (keep :updatable)
                                                          (map (juxt :node-id identity)))
                                                    selected-renderables)))
  (output updatables g/Any :cached (g/fnk [scene-render-data]
                                     ;; Currently updatables are implemented as extra info on the renderables.
                                     ;; The renderable associates an updatable with itself, which contains info
                                     ;; about the updatable. The updatable is identified by a node-id, for example
                                     ;; the id of a ParticleFXNode. In the case of ParticleFX, the same updatable
                                     ;; is also associated with every sub-element of the ParticleFX scene, such
                                     ;; as every emitter and modifier below it. This makes it possible to start
                                     ;; playback of the owning ParticleFX scene while a modifier is selected.
                                     ;; In order to find the owning ParticleFX scene so we can position the
                                     ;; effect in the world, we find the renderable with the shortest node-id-path
                                     ;; for that particular updatable.
                                     ;;
                                     ;; TODO:
                                     ;; We probably want to change how this works to make it possible to have
                                     ;; multiple instances of the same updatable in a scene.
                                     (let [flat-renderables (apply concat (map second (:renderables scene-render-data)))
                                           renderables-by-updatable-node-id (dissoc (group-by (comp :node-id :updatable) flat-renderables) nil)]
                                       (into {}
                                             (map (fn [[updatable-node-id renderables]]
                                                    (let [renderable (first (sort-by (comp count :node-id-path) renderables))
                                                          updatable (:updatable renderable)
                                                          preview-anim-data (get-in renderable [:user-data :anim-data])
                                                          world-transform (:world-transform renderable)
                                                          transformed-updatable (assoc updatable
                                                                                  :world-transform world-transform
                                                                                  :preview-anim-data preview-anim-data)]
                                                      [updatable-node-id transformed-updatable])))
                                             renderables-by-updatable-node-id))))
  (output pass->render-args g/Any :cached produce-pass->render-args))

;; Scene selection

(defn map-scene [f scene]
  (letfn [(scene-fn [scene]
            (let [children (:children scene)]
              (cond-> (f scene)
                children (update :children (partial mapv scene-fn)))))]
    (scene-fn scene)))

(defn claim-child-scene [child-scene old-node-id new-node-id new-node-outline-key]
  (if (= old-node-id (:node-id child-scene))
    (assoc child-scene :node-id new-node-id :node-outline-key new-node-outline-key)
    (assoc child-scene :picking-node-id new-node-id)))

(defn claim-scene [scene new-node-id new-node-outline-key]
  ;; When scenes reference other resources in the project, we want to treat the
  ;; referenced scene as a group when picking in the scene view. To make this
  ;; happen, the referencing scene claims ownership of the referenced scene and
  ;; its children. Note that sub-elements can still be selected using the
  ;; Outline view should the need arise.
  (let [old-node-id (:node-id scene)
        children (:children scene)
        finalize-claim-fn (:finalize-claim-fn scene)]
    (cond-> (assoc scene :node-id new-node-id :node-outline-key new-node-outline-key)
            children (assoc :children (mapv (partial map-scene #(claim-child-scene % old-node-id new-node-id new-node-outline-key))
                                            children))
            finalize-claim-fn (finalize-claim-fn old-node-id new-node-id))))

(defn- box-selection? [^Rect picking-rect]
  (or (> (.width picking-rect) selection/min-pick-size)
      (> (.height picking-rect) selection/min-pick-size)))

(defn- picking-rect->clamped-view-rect
  ^Rect [^Region viewport ^Rect picking-rect]
  (let [view-width (.width picking-rect)
        view-height (.height picking-rect)
        view-left (- (.x picking-rect) (/ view-width 2))
        view-right (+ (.x picking-rect) (/ view-width 2))
        view-top (- (.y picking-rect) (/ view-height 2))
        view-bottom (+ (.y picking-rect) (/ view-height 2))
        clamped-left (int (max (.left viewport) (min view-left (.right viewport))))
        clamped-right (int (max (.left viewport) (min view-right (.right viewport))))
        clamped-top (int (max (.top viewport) (min view-top (.bottom viewport))))
        clamped-bottom (int (max (.top viewport) (min view-bottom (.bottom viewport))))]
    (types/rect clamped-left clamped-top (int (- clamped-right clamped-left)) (int (- clamped-bottom clamped-top)))))

(def ^:private picking-drawable-size selection/min-pick-size)
(def ^:private picking-viewport (Region. 0 picking-drawable-size 0 picking-drawable-size))
(def ^:private picking-buf-spiral-indices
  ;; Indices for traversing the picking buffer in a spiral fashion
  ;; from the center starting left then going clockwise.
  ;; Indices work as long as the buffer is square. Even/odd side
  ;; length supported, where an even side puts center towards
  ;; bottom right.
  (let [size (* picking-drawable-size picking-drawable-size)
        side picking-drawable-size
        cx (int (/ side 2))
        cy (int (/ side 2))
        repeats (mapcat (partial repeat 2) (rest (range)))
        left-up-right-down [[-1 0] [0 -1] [1 0] [0 1]]
        deltas (take (dec size) (mapcat repeat repeats (cycle left-up-right-down)))
        coords (reductions (fn [[x y] [dx dy]] [(+ x dx) (+ y dy)]) [cx cy] deltas)]
    (map (fn [[x y]]
           (let [flipped-y (dec (- picking-drawable-size y))]
             (+ x (* flipped-y picking-drawable-size))))
         coords)))

(defn- picking-buf->spiral-seq [^ints buf]
  (assert (= (count buf) (* picking-drawable-size picking-drawable-size)) "picking buf of unexpected size")
  (map (partial aget buf) picking-buf-spiral-indices))

(def ^:private preview-close-button-size 18.0)

(defn- close-preview-tooltip [localization-state keymap]
  (localization-state
    (localization/message
      "command.tooltip"
      {"command" (localization/message "command.scene.stop")
       "shortcut" (keymap/display-text keymap :scene.stop "none")})))

(fxui/defc close-preview-button
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}]}
  [{:keys [localization-state keymap]}]
  {:fx/type fxui/button
   :focus-traversable false
   :style-class "scene-preview-close-button"
   :min-width preview-close-button-size
   :min-height preview-close-button-size
   :pref-width preview-close-button-size
   :pref-height preview-close-button-size
   :max-width preview-close-button-size
   :max-height preview-close-button-size
   :graphic {:fx/type fxui/icon-graphic
             :type :icon/cross-thick
             :size 8.0
             :style-class "scene-preview-close-icon"}
   :tooltip (close-preview-tooltip localization-state keymap)
   :on-action (fn [^ActionEvent event]
                (ui/run-command (.getSource event) :scene.stop))})

(defn- info-label [info-text]
  {:fx/type fx.label/lifecycle
   :style-class "info-label"
   :mouse-transparent true
   :anchor-pane/top 15
   :anchor-pane/left 30
   :text info-text})

(defn- camera-inset-image-view [camera-inset-image camera-inset-width camera-inset-height]
  {:fx/type fx.stack-pane/lifecycle
   :pref-width camera-inset-width
   :pref-height camera-inset-height
   :max-width camera-inset-width
   :max-height camera-inset-height
   :min-width camera-inset-width
   :min-height camera-inset-height
   :children [{:fx/type fx.image-view/lifecycle
               :image camera-inset-image
               :fit-width camera-inset-width
               :fit-height camera-inset-height
               :preserve-ratio false
               :smooth true
               :scale-y -1.0}]
   :mouse-transparent true
   :anchor-pane/bottom camera-inset-margin
   :anchor-pane/right camera-inset-margin})

(defn- animation-preview-anchor-props [camera viewport anim-data]
  (let [[sx sy _] (c/scale-factor camera viewport)
        offset texture-set/animation-preview-offset
        image-width (double (:width anim-data))
        image-height (double (:height anim-data))
        scaled-width (/ image-width sx)
        scaled-height (/ image-height sy)
        x0 offset
        y0 (- (double (:bottom viewport)) offset)
        x1 (+ x0 scaled-width)
        y1 (- y0 scaled-height)
        left (- x1 preview-close-button-size)
        top y1]
    {:anchor-pane/left (max 0.0 left)
     :anchor-pane/top (max 0.0 top)}))

(defn- active-animation-anim-data [updatables active-updatable-ids]
  (some (fn [updatable-id]
          (when-let [updatable (get updatables updatable-id)]
            (:preview-anim-data updatable)))
        active-updatable-ids))

(fxui/defc error-overlay
  {:compose [{:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}]}
  [{:keys [localization-state error]}]
  {:fx/type fx.text-area/lifecycle
   :style-class "info-text-area"
   :editable false
   :wrap-text true
   :text (coll/join-to-string "\n" (error-message-lines [error] localization-state))})

(g/defnk produce-overlay-anchor-pane-props [scene ^:try tool-info-text active-updatable-ids updatables camera viewport localization keymap camera-inset-data]
  (if-let [error (:error scene)]
    {:children [{:fx/type error-overlay
                 :anchor-pane/bottom 0
                 :anchor-pane/left 0
                 :anchor-pane/right 0
                 :anchor-pane/top 0
                 :localization localization
                 :error error}]}
    (if-let [overlay-anchor-pane-props (:overlay-anchor-pane-props scene)]
      overlay-anchor-pane-props
      (let [info-text
            (if (and (string? tool-info-text)
                     (pos? (count tool-info-text)))
              tool-info-text
              (let [scene-info-text (:info-text scene)]
                (when (and (string? scene-info-text)
                           (pos? (count scene-info-text)))
                  scene-info-text)))
            close-button (when-let [anim-data (and (not (coll/empty? active-updatable-ids))
                                                   (active-animation-anim-data updatables active-updatable-ids))]
                           (merge (animation-preview-anchor-props camera viewport anim-data)
                                  {:fx/type close-preview-button
                                   :localization localization
                                   :keymap keymap}))
            camera-inset-visible? (:visible camera-inset-data)
            camera-inset-image (:image camera-inset-data)
            camera-inset-width (:width camera-inset-data camera-inset-width)
            camera-inset-height (:height camera-inset-data camera-inset-height)
            children (cond-> []
                             info-text (conj (info-label info-text))
                             close-button (conj close-button)
                             (and camera-inset-visible? camera-inset-image) (conj (camera-inset-image-view camera-inset-image camera-inset-width camera-inset-height)))]
        (if (not (coll/empty? children))
          {:pick-on-bounds false
           :children children
           :mouse-transparent (nil? close-button)}
          {:visible false})))))

(g/defnk produce-selection [scene-render-data renderables-aabb+picking-node-id ^GLAutoDrawable picking-drawable camera ^Region viewport pass->render-args ^Rect picking-rect]
  (when (some? picking-rect)
    (cond
      (box-selection? picking-rect)
      (let [clamped-view-rect (picking-rect->clamped-view-rect viewport picking-rect)
            frustum (c/screen-rect-frustum camera viewport clamped-view-rect)]
        (keep (fn [[aabb picking-node-id]]
                (when (geom/aabb-inside-frustum? aabb frustum)
                  picking-node-id))
              renderables-aabb+picking-node-id))

      (not (geom/rect-empty? picking-rect))
      (gl/with-drawable-as-current picking-drawable
        (gl/gl-clear gl 0.0 0.0 0.0 1.0)
        (gl-viewport gl picking-viewport)
        (let [renderables (:renderables scene-render-data)
              picking-id->picking-node-id (into {}
                                                (comp
                                                  cat
                                                  (map (juxt :picking-id :picking-node-id)))
                                                (vals renderables))
              buf (int-array (* picking-drawable-size picking-drawable-size))]
          (reset! last-picking-rect picking-rect)
          (doseq [pass [pass/opaque-selection pass/selection]]
            (let [pass-render-args (picking-render-args (pass->render-args pass) viewport picking-rect)
                  pass-renderables (vec (render-sort (get renderables pass)))]
              (setup-pass gl pass pass-render-args)
              (batch-render gl pass-render-args pass-renderables :select-batch-key)))
          (.glFlush gl)
          (.glFinish gl)
          ;; Pixels read back are like 0xAARRGGBB
          (.glReadPixels ^GL2 gl 0 0 ^int picking-drawable-size ^int picking-drawable-size
                         GL2/GL_BGRA GL2/GL_UNSIGNED_BYTE (IntBuffer/wrap buf))
          (transduce (comp (map scene-picking/argb->picking-id)
                           (keep picking-id->picking-node-id)
                           (take 1))
                     conj
                     (picking-buf->spiral-seq buf)))))))

(g/defnk produce-tool-renderables [tool-renderables]
  ;; tool-renderables input is a [{pass [renderables]}]
  (let [all-tool-renderables (apply concat (vals (apply merge-with into tool-renderables)))
        tool-renderable-identifiers (into [] (comp (map (juxt :node-id :selection-data)) (distinct)) all-tool-renderables)
        tool-renderable-identifier->picking-id (zipmap tool-renderable-identifiers (map picking-seed->picking-id (rest (range))))]
    (mapv (fn [pass->renderables]
            (into {}
                  (map (fn [[pass renderables]]
                         (let [pickable-renderables (mapv (fn [renderable]
                                                            (let [identifier [(:node-id renderable) (:selection-data renderable)]]
                                                              (assoc renderable :picking-id (tool-renderable-identifier->picking-id identifier))))
                                                          renderables)]
                           [pass pickable-renderables])))
                  pass->renderables))
          tool-renderables)))

(g/defnk produce-tool-selection [tool-renderables ^GLAutoDrawable picking-drawable ^Region viewport pass->render-args ^Rect tool-picking-rect inactive?]
  (when (and (some? tool-picking-rect)
             (not inactive?)
             (not (geom/rect-empty? tool-picking-rect)))
    (gl/with-drawable-as-current picking-drawable
      (gl/gl-clear gl 0.0 0.0 0.0 1.0)
      (gl-viewport gl picking-viewport)
      (let [render-args (picking-render-args (pass->render-args pass/manipulator-selection) viewport tool-picking-rect)
            tool-renderables (apply merge-with into tool-renderables)
            pickable-tool-renderables (get tool-renderables pass/manipulator-selection)
            picking-id->renderable (into {} (map (juxt :picking-id identity)) pickable-tool-renderables)
            buf (int-array (* picking-drawable-size picking-drawable-size))]
        (reset! last-picking-rect tool-picking-rect)
        (setup-pass gl pass/manipulator-selection render-args)
        (batch-render gl render-args pickable-tool-renderables :select-batch-key)
        (.glFlush gl)
        (.glFinish gl)
        ;; Pixels read back are like 0xAARRGGBB
        (.glReadPixels ^GL2 gl 0 0 ^int picking-drawable-size ^int picking-drawable-size
                       GL2/GL_BGRA GL2/GL_UNSIGNED_BYTE (IntBuffer/wrap buf))
        (transduce (comp (map scene-picking/argb->picking-id)
                         (keep picking-id->renderable)
                         (take 1))
                   conj
                   (picking-buf->spiral-seq buf))))))

(g/defnk produce-selected-tool-renderables [tool-selection]
  (apply merge-with concat {} (map #(do {(:node-id %) [(:selection-data %)]}) tool-selection)))

(declare update-image-view!)

(defn merge-render-datas [aux-render-data tool-render-data scene-render-data]
  (if (:error scene-render-data)
    {:renderables {}
     :selected-renderables []}
    (let [all-renderables-by-pass
          (coll/merge-with
            coll/merge
            (:renderables aux-render-data)
            (:renderables tool-render-data)
            (:renderables scene-render-data))

          sorted-renderables-by-pass
          (coll/into-> all-renderables-by-pass {}
            (map (fn [[pass renderables]]
                   (pair pass
                         (vec (render-sort renderables))))))]
      {:renderables sorted-renderables-by-pass
       :selected-renderables (:selected-renderables scene-render-data)})))

(g/defnode SceneView
  (inherits view/WorkbenchView)
  (inherits SceneRenderer)

  (property app-view g/NodeID)
  (property image-view ImageView)
  (property viewport Region (default (g/constantly (types/->Region 0 0 0 0))))
  (property active-updatable-ids g/Any)
  ;; Some previewed animation state advances in scene caches rather than graph values,
  ;; so the inset needs an explicit frame tick dependency to invalidate its cached image.
  (property camera-inset-frame-version g/Int (default 0))
  (property play-mode g/Keyword)
  (property drawable GLAutoDrawable)
  (property camera-inset-drawable GLAutoDrawable)
  (property picking-drawable GLAutoDrawable)
  (property async-copy-state g/Any)
  (property cursor-pos types/Vec2)
  (property tool-picking-rect Rect)
  (property updatable-states g/Any)

  (input input-handlers Runnable :array)
  (input update-tick-handlers Runnable :array)
  (input picking-rect Rect)
  (input tool-info-text g/Str)
  (input tool-renderables pass/RenderData :array :substitute substitute-render-data)
  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input updatables g/Any)
  (input selected-updatables g/Any)
  (input localization g/Any)
  (input keymap g/Any)
  (input grid g/Any)
  (output inactive? g/Bool (g/fnk [_node-id active-view] (not= _node-id active-view)))
  (output info-text g/Str (g/fnk [scene tool-info-text]
                            (or tool-info-text (:info-text scene))))
  (output camera-inset-data g/Any :cached produce-camera-inset-data)
  (output overlay-anchor-pane-props g/Any :cached produce-overlay-anchor-pane-props)
  (output tool-renderables g/Any produce-tool-renderables)
  (output active-tool g/Keyword (gu/passthrough active-tool))
  (output manip-space g/Keyword (gu/passthrough manip-space))
  (output active-updatables g/Any :cached (g/fnk [updatables active-updatable-ids]
                                                 (into [] (keep updatables) active-updatable-ids)))

  (output selection g/Any (gu/passthrough selection))
  (output all-renderables pass/RenderData :cached (g/fnk [aux-render-data tool-render-data scene-render-data]
                                                    (:renderables (merge-render-datas aux-render-data tool-render-data scene-render-data))))
  (output tool-render-data g/Any :cached (g/fnk [tool-renderables inactive?]
                                           (if inactive?
                                             {:renderables []}
                                             {:renderables (reduce (partial merge-with into)
                                                                   {}
                                                                   tool-renderables)})))
  (output picking-selection g/Any :cached produce-selection)
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables)
  (output displayed-node-properties g/Any :cached
          (g/fnk [selected-node-properties preview-overrides]
            (displayed-node-properties selected-node-properties preview-overrides))))

(defn cursor
  "Maps inconsistent cursor types across platforms.
  See https://bugs.openjdk.org/browse/JDK-8101062"
  [cursor-type]
  (case cursor-type
    ;; `CLOSED_HAND ``OPEN_HAND_HAND ``HAND `all render a pointer cursor on Linux and Windows.
    ;; `MOVE `renders the default cursor on macOS.
    :pan (if (os/is-mac-os?) Cursor/CLOSED_HAND Cursor/MOVE)
    :none Cursor/NONE
    Cursor/DEFAULT))

(defn- screen->world
  ^Vector3d [camera viewport ^Vector3d screen-pos]
  (let [w4 (c/camera-unproject camera viewport screen-pos)]
    (Vector3d. (.x w4) (.y w4) (.z w4))))

(defn- view->camera
  ([view]
   (view->camera (g/now) view))
  ([basis view]
   (g/node-feeding-into basis view :camera)))

(defn augment-action [view action]
  (let [x          (:x action)
        y          (:y action)
        screen-pos (Vector3d. x y 0)
        camera     (g/node-value (view->camera view) :camera)
        viewport   (g/node-value view :viewport)
        world-pos  (Point3d. (screen->world camera viewport screen-pos))
        world-dir  (doto (screen->world camera viewport (doto (Vector3d. screen-pos) (.setZ 1)))
                         (.sub world-pos)
                         (.normalize))]
    (assoc action
           :screen-pos screen-pos
           :world-pos world-pos
           :world-dir world-dir)))

(defn refresh-scene-view! [node-id dt]
  (let [basis (g/now)
        node (g/node-by-id-at basis node-id)
        image-view (g/raw-property-value* basis node :image-view)]
    (when-not (ui/inside-hidden-tab? image-view)
      (let [drawable (g/raw-property-value* basis node :drawable)
            async-copy-state-atom (g/raw-property-value* basis node :async-copy-state)]
        (when (and (some? drawable)
                   (some? async-copy-state-atom))
          (update-image-view! node-id image-view drawable async-copy-state-atom dt)
          (when-let [cursor-type (g/maybe-node-value node-id :cursor-type)]
            (ui/set-cursor image-view (cursor cursor-type)))))
      (when-let [overlay-anchor-pane (g/raw-property-value* basis node :overlay-anchor-pane)]
        (let [overlay-anchor-pane-props (g/node-value node-id :overlay-anchor-pane-props)]
          (fxui/advance-graph-user-data-component!
            node-id :overlay-anchor-pane
            {:fx/type fxui/ext-with-anchor-pane-props
             :props overlay-anchor-pane-props
             :desc {:fx/type fxui/ext-value
                    :value overlay-anchor-pane}}))))))

(defn- supports-camera-inset-drawable? [node-id]
  (g/node-instance? (g/now) SceneView node-id))

(defn dispose-scene-view! [node-id]
  (when (g/node-by-id node-id)
    (when-let [^GLAutoDrawable drawable (g/node-value node-id :drawable)]
      (gl/with-drawable-as-current drawable
        (scene-cache/drop-context! gl)
        (when-let [async-copy-state-atom (g/node-value node-id :async-copy-state)]
          (scene-async/dispose! @async-copy-state-atom gl))
        (.glFinish gl))
      (.destroy drawable))
    (when-let [^GLAutoDrawable picking-drawable (g/node-value node-id :picking-drawable)]
      (gl/with-drawable-as-current picking-drawable
        (scene-cache/drop-context! gl)
        (.glFinish gl))
      (.destroy picking-drawable))
    (when (supports-camera-inset-drawable? node-id)
      (when-let [^GLAutoDrawable camera-inset-drawable (g/node-value node-id :camera-inset-drawable)]
        (gl/with-drawable-as-current camera-inset-drawable
          (scene-cache/drop-context! gl)
          (.glFinish gl))
        (.destroy camera-inset-drawable)))
    (fxui/advance-graph-user-data-component! node-id :overlay-anchor-pane nil)
    (g/transact
      (concat
        (g/set-property node-id :drawable nil)
        (g/set-property node-id :picking-drawable nil)
        (when (supports-camera-inset-drawable? node-id)
          (g/set-property node-id :camera-inset-drawable nil))
        (g/set-property node-id :async-copy-state nil)))))

(defn- active-scene-view
  ([app-view]
   (g/with-auto-evaluation-context evaluation-context
     (active-scene-view app-view evaluation-context)))
  ([app-view evaluation-context]
   (let [basis (:basis evaluation-context)
         view (g/node-value app-view :active-view evaluation-context)]
     (when (and view (g/node-instance? basis SceneView view))
       view))))

(defn- play-handler [view-id]
  (let [play-mode (g/node-value view-id :play-mode)
        selected-updatable-ids (set (keys (g/node-value view-id :selected-updatables)))
        active-updatable-ids (set (g/node-value view-id :active-updatable-ids))
        new-play-mode (if (= selected-updatable-ids active-updatable-ids)
                        (if (= play-mode :playing) :idle :playing)
                        :playing)]
    (g/transact
      (concat
        (g/set-property view-id :play-mode new-play-mode)
        (g/set-property view-id :active-updatable-ids selected-updatable-ids)))))

(handler/defhandler :scene.play :global
  (active? [app-view evaluation-context]
           (when-let [view (active-scene-view app-view evaluation-context)]
             (seq (g/node-value view :updatables evaluation-context))))
  (enabled? [app-view evaluation-context]
            (when-let [view (active-scene-view app-view evaluation-context)]
              (let [selected (g/node-value view :selected-updatables evaluation-context)]
                (not (empty? selected)))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (play-handler view))))

(defn- stop-handler [view-id]
  (when-let [image-view ^ImageView (g/node-value view-id :image-view)]
    ;; Force a redraw on next refresh without requiring input.
    (ui/user-data! image-view ::last-frame-version nil))
  (g/transact
    (concat
      (g/set-property view-id :play-mode :idle)
      (g/set-property view-id :active-updatable-ids [])
      (g/set-property view-id :updatable-states {}))))

(handler/defhandler :scene.stop :global
  (active? [app-view evaluation-context]
    (when-let [view (active-scene-view app-view evaluation-context)]
      (seq (g/node-value view :updatables evaluation-context))))
  (enabled? [app-view evaluation-context]
    (when-let [view (active-scene-view app-view evaluation-context)]
      (seq (g/node-value view :active-updatables evaluation-context))))
  (run [app-view] (when-let [view (active-scene-view app-view)]
                    (stop-handler view))))

(defn- fudge-empty-aabb
  ^AABB [^AABB aabb]
  (if (geom/null-aabb? aabb)
    aabb
    (let [min-p ^Point3d (.min aabb)
          max-p ^Point3d (.max aabb)
          zero-x (= (.x min-p) (.x max-p))
          zero-y (= (.y min-p) (.y max-p))
          zero-z (= (.z min-p) (.z max-p))]
      (geom/coords->aabb [(cond-> (.x min-p) zero-x dec)
                          (cond-> (.y min-p) zero-y dec)
                          (cond-> (.z min-p) zero-z dec)]
                         [(cond-> (.x max-p) zero-x inc)
                          (cond-> (.y max-p) zero-y inc)
                          (cond-> (.z max-p) zero-z inc)]))))

(defn- aabb-framing-info [view-node-id aabb evaluation-context]
  (let [basis (:basis evaluation-context)
        camera-node-id (view->camera basis view-node-id)
        viewport (g/node-value view-node-id :viewport evaluation-context)
        start-camera (g/node-value camera-node-id :local-camera evaluation-context)
        end-camera (c/camera-frame-aabb start-camera viewport aabb)]
    {:camera-node-id camera-node-id
     :start-camera start-camera
     :end-camera end-camera}))

(defn- selection-framing-info [view-node-id evaluation-context]
  (let [aabb (fudge-empty-aabb (g/node-value view-node-id :selected-aabb evaluation-context))]
    (aabb-framing-info view-node-id aabb evaluation-context)))

(defn- apply-framing-info! [framing-info animate?]
  (let [{:keys [camera-node-id start-camera end-camera]} framing-info]
    (c/set-camera! camera-node-id start-camera end-camera animate?)))

(defn- frame-selection! [view-node-id animate?]
  (let [framing-info (g/with-auto-evaluation-context evaluation-context
                       (selection-framing-info view-node-id evaluation-context))]
    (apply-framing-info! framing-info animate?)))

(handler/defhandler :scene.frame-selection :global
  (active? [app-view evaluation-context]
    (active-scene-view app-view evaluation-context))
  (enabled? [app-view evaluation-context]
    (when-let [view (active-scene-view app-view evaluation-context)]
      (let [selected (g/node-value view :selection evaluation-context)]
        (not (empty? selected)))))
  (run [app-view] (some-> (active-scene-view app-view)
                          (frame-selection! true))))

(defn- camera-animating?
  [app-view evaluation-context]
  (when-some [active-scene-view (active-scene-view app-view evaluation-context)]
    (some-> (view->camera (:basis evaluation-context) active-scene-view)
            (g/node-value :animating evaluation-context))))

(handler/defhandler :scene.realign-camera :global
  (active? [app-view evaluation-context]
    (active-scene-view app-view evaluation-context))
  (enabled? [app-view evaluation-context] (not (camera-animating? app-view evaluation-context)))
  (run [app-view]
    (g/with-auto-evaluation-context evaluation-context
      (when-let [scene-view (active-scene-view app-view evaluation-context)]
        (when-let [camera (view->camera (:basis evaluation-context) scene-view)]
          (c/realign-camera camera true evaluation-context))))))

(handler/defhandler :scene.set-camera-type :global
  (label [user-data]
    (if user-data
      (case (:camera-type user-data)
        :orthographic (localization/message "command.scene.set-camera-type.option.orthographic")
        :perspective (localization/message "command.scene.set-camera-type.option.perspective")
        (localization/message "command.scene.set-camera-type"))
      (localization/message "command.scene.set-camera-type")))
  (active? [app-view evaluation-context]
    (active-scene-view app-view evaluation-context))
  (run [app-view user-data]
    (g/with-auto-evaluation-context evaluation-context
      (when-some [view (active-scene-view app-view evaluation-context)]
        (c/set-camera-type! (view->camera (:basis evaluation-context) view) (:camera-type user-data)))))
  (options [user-data]
    (when-not user-data
      [{:label (localization/message "command.scene.set-camera-type.option.orthographic")
        :command :scene.set-camera-type
        :user-data {:camera-type :orthographic}}
       {:label (localization/message "command.scene.set-camera-type.option.perspective")
        :command :scene.set-camera-type
        :user-data {:camera-type :perspective}}]))
  (state [app-view user-data evaluation-context]
    (some-> (active-scene-view app-view evaluation-context)
            (g/node-value :camera-type evaluation-context)
            (= (:camera-type user-data)))))

(handler/defhandler :scene.toggle-interaction-mode :workbench
  (active? [app-view evaluation-context]
    (active-scene-view app-view evaluation-context))
  (enabled? [app-view evaluation-context] (not (camera-animating? app-view evaluation-context)))
  (run [app-view]
    (g/with-auto-evaluation-context evaluation-context
      (when-let [scene-view (active-scene-view app-view evaluation-context)]
        (when-let [camera (view->camera (:basis evaluation-context) scene-view)]
          (c/realign-camera camera true evaluation-context)))))
  (state [app-view evaluation-context]
    (let [basis (:basis evaluation-context)
          scene-view (active-scene-view app-view evaluation-context)]
      (c/camera-2d? (view->camera basis scene-view) evaluation-context))))

;; Used in the scene view tool bar.
(handler/defhandler :scene.toggle-camera-type :workbench
  (active? [app-view evaluation-context]
    (active-scene-view app-view evaluation-context))
  (enabled? [app-view evaluation-context]
    (and (not (let [basis (:basis evaluation-context)
                    scene-view (active-scene-view app-view evaluation-context)]
                (c/camera-2d? (view->camera basis scene-view) evaluation-context)))
         (not (camera-animating? app-view evaluation-context))))
  (run [app-view]
    (g/with-auto-evaluation-context evaluation-context
      (when-some [view (active-scene-view app-view evaluation-context)]
        (c/set-camera-type! (view->camera (:basis evaluation-context) view)
                            (case (g/node-value view :camera-type evaluation-context)
                              :orthographic :perspective
                              :perspective :orthographic)))))
  (state [app-view evaluation-context]
    (some-> (active-scene-view app-view evaluation-context)
            (g/node-value :camera-type evaluation-context)
            (= :perspective))))

(handler/defhandler :scene.free-camera.activate :workbench
  (enabled? [app-view evaluation-context]
    (when-let [scene-view (active-scene-view app-view evaluation-context)]
      (when-let [camera (view->camera (:basis evaluation-context) scene-view)]
        (contains? (g/node-value camera :movements-enabled evaluation-context) :look))))
  (run [app-view]
    (g/with-auto-evaluation-context evaluation-context
      (when-let [scene-view (active-scene-view app-view evaluation-context)]
        (let [camera (view->camera (:basis evaluation-context) scene-view)
              image-view (g/node-value scene-view :image-view evaluation-context)]
          (when (and camera image-view)
            (c/start-free-cam-mode! image-view camera (i/get-cursor-pos))))))))

(defn- set-manip-space! [app-view manip-space]
  (assert (contains? #{:local :world} manip-space))
  (g/set-property! app-view :manip-space manip-space))

(handler/defhandler :scene.set-manipulator-space :global
  (label [user-data]
    (if user-data
      (case (:manip-space user-data)
        :world (localization/message "command.scene.set-manipulator-space.option.world")
        :local (localization/message "command.scene.set-manipulator-space.option.local"))
      (localization/message "command.scene.set-manipulator-space")))
  (active? [app-view evaluation-context]
           (active-scene-view app-view evaluation-context))
  (enabled? [app-view user-data evaluation-context]
            (let [active-tool (g/node-value app-view :active-tool evaluation-context)]
              (contains? (scene-tools/supported-manip-spaces active-tool)
                         (:manip-space user-data))))
  (options [user-data]
    (when-not user-data
      [{:label (localization/message "command.scene.set-manipulator-space.option.world")
        :command :scene.set-manipulator-space
        :user-data {:manip-space :world}}
       {:label (localization/message "command.scene.set-manipulator-space.option.local")
        :command :scene.set-manipulator-space
        :user-data {:manip-space :local}}]))
  (run [app-view user-data] (set-manip-space! app-view (:manip-space user-data)))
  (state [app-view user-data evaluation-context] (= (g/node-value app-view :manip-space evaluation-context) (:manip-space user-data))))

(handler/defhandler :scene.toggle-move-whole-pixels :global
  (active? [app-view evaluation-context] (active-scene-view app-view evaluation-context))
  (state [prefs] (scene-tools/move-whole-pixels? prefs))
  (run [prefs] (scene-tools/set-move-whole-pixels! prefs (not (scene-tools/move-whole-pixels? prefs)))))

(handler/register-menu! ::menubar-edit :editor.app-view/edit-end
  [{:label :separator}
   {:label (localization/message "command.scene.set-manipulator-space.option.world")
    :command :scene.set-manipulator-space
    :user-data {:manip-space :world}
    :check true}
   {:label (localization/message "command.scene.set-manipulator-space.option.local")
    :command :scene.set-manipulator-space
    :user-data {:manip-space :local}
    :check true}
   {:label :separator}
   {:label (localization/message "command.scene.toggle-move-whole-pixels")
    :command :scene.toggle-move-whole-pixels
    :check true}])

(handler/register-menu! ::menubar-view :editor.app-view/view-end
  [{:label (localization/message "command.scene.visibility.toggle-filters")
    :command :scene.visibility.toggle-filters}
   {:label (localization/message "command.scene.visibility.toggle-component-guides")
    :command :scene.visibility.toggle-component-guides}
   {:label (localization/message "command.scene.visibility.toggle-grid")
    :command :scene.visibility.toggle-grid}
   {:label :separator}
   {:label (localization/message "command.scene.visibility.toggle-selection")
    :command :scene.visibility.toggle-selection}
   {:label (localization/message "command.scene.visibility.hide-unselected")
    :command :scene.visibility.hide-unselected}
   {:label (localization/message "command.scene.visibility.show-last-hidden")
    :command :scene.visibility.show-last-hidden}
   {:label (localization/message "command.scene.visibility.show-all")
    :command :scene.visibility.show-all}
   {:label :separator}
   {:label (localization/message "command.scene.play")
    :command :scene.play}
   {:label (localization/message "command.scene.stop")
    :command :scene.stop}
   {:label :separator}
   {:command :scene.set-camera-type
    :user-data {:camera-type :orthographic}
    :check true}
   {:command :scene.set-camera-type
    :user-data {:camera-type :perspective}
    :check true}
   {:label :separator}
   {:label (localization/message "command.scene.frame-selection")
    :command :scene.frame-selection}
   {:label (localization/message "command.scene.realign-camera")
    :command :scene.realign-camera}])

(defn dispatch-input [input-handlers input-state action user-data]
  (reduce (fn [action [node-id label]]
            (when action
              ((g/node-value node-id label) node-id input-state action user-data)))
          action input-handlers))

(defn- update-updatables
  [updatable-states play-mode active-updatables dt]
  (let [context {:dt (if (= play-mode :playing) dt 0)}]
    (reduce (fn [ret {:keys [update-fn node-id world-transform initial-state]}]
              (let [context (assoc context
                              :world-transform world-transform
                              :node-id node-id)
                    state (get-in updatable-states [node-id] initial-state)]
                (assoc ret node-id (update-fn state context))))
            {}
            active-updatables)))

(defn update-image-view! [view-id ^ImageView image-view ^GLAutoDrawable drawable async-copy-state-atom dt]
  (let [action-queue (g/user-data view-id ::input-action-queue)
        [render-mode tool-user-data play-mode active-updatables updatable-states]
        (g/with-auto-evaluation-context evaluation-context
          [(g/node-value view-id :render-mode evaluation-context)
           (g/node-value view-id :selected-tool-renderables evaluation-context) ; TODO: for what actions do we need selected tool renderables?
           (g/node-value view-id :play-mode evaluation-context)
           (g/node-value view-id :active-updatables evaluation-context)
           (g/node-value view-id :updatable-states evaluation-context)])
        has-active-updatables (not (coll/empty? active-updatables))
        new-updatable-states (if has-active-updatables
                               (profiler/profile "updatables" -1 (update-updatables updatable-states play-mode active-updatables dt))
                               updatable-states)
        renderables (g/node-value view-id :all-renderables)
        last-renderables (ui/user-data image-view ::last-renderables)
        last-frame-version (ui/user-data image-view ::last-frame-version)
        frame-version (cond-> (or last-frame-version 0)
                        (or (nil? last-renderables)
                            (not (identical? last-renderables renderables))
                            (and has-active-updatables (= :playing play-mode)))
                        inc)]
    (profiler/profile "input-dispatch" -1
      (let [input-handlers (g/sources-of view-id :input-handlers)]
        (g/user-data! view-id ::input-state
                      (reduce (fn [input-state action]
                                (let [input-state (i/update-input-state input-state action)]
                                  (dispatch-input input-handlers input-state action tool-user-data)
                                  input-state))
                              (g/user-data view-id ::input-state)
                              action-queue))
        (when (some #(#{:mouse-pressed :mouse-released} (:type %)) action-queue)
          (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true))))
    (profiler/profile "update-tick" -1
      (g/with-auto-evaluation-context evaluation-context
        (let [update-tick-handlers (g/sources-of (:basis evaluation-context) view-id :update-tick-handlers)
              input-state (g/user-data view-id ::input-state)]
          (reduce (fn [input-state [node-id label]]
                    (when input-state
                      ((g/node-value node-id label evaluation-context) node-id input-state dt)))
                  input-state update-tick-handlers))))
    (when (seq action-queue)
      (g/user-data! view-id ::input-action-queue [])
      (g/user-data-swap! view-id ::input-state assoc :scroll-delta [0.0 0.0]))
    (when has-active-updatables
      (g/set-property! view-id :updatable-states new-updatable-states))
    (when (supports-camera-inset-drawable? view-id)
      (g/set-property! view-id :camera-inset-frame-version frame-version))
    (profiler/profile "render" -1
      (when (not= last-frame-version frame-version)
        (gl/with-drawable-as-current drawable
          (let [[viewport pass->render-args]
                (g/with-auto-evaluation-context evaluation-context
                  [(g/node-value view-id :viewport evaluation-context)
                   (g/node-value view-id :pass->render-args evaluation-context)])]
            (scene-cache/process-pending-deletions! gl)
            (render! gl-context render-mode renderables new-updatable-states viewport pass->render-args)
            (ui/user-data! image-view ::last-renderables renderables)
            (ui/user-data! image-view ::last-frame-version frame-version)
            (scene-cache/prune-context! gl)
            (reset! async-copy-state-atom (scene-async/finish-image! (scene-async/begin-read! @async-copy-state-atom gl) gl))))))
    ;; call frame-selection if it's the very first aabb change for the scene
    (let [prev-aabb (ui/user-data image-view ::prev-scene-aabb)
          [scene-aabb reframing-info]
          (g/with-auto-evaluation-context evaluation-context
            (let [scene-aabb (g/node-value view-id :scene-aabb evaluation-context)
                  reframing-info (when (and prev-aabb
                                            (geom/predefined-aabb? prev-aabb)
                                            (not (geom/predefined-aabb? scene-aabb)))
                                   (aabb-framing-info view-id scene-aabb evaluation-context))]
              (pair scene-aabb
                    reframing-info)))]

      (ui/user-data! image-view ::prev-scene-aabb scene-aabb)
      (when reframing-info
        (apply-framing-info! reframing-info true)))
    (let [new-image (scene-async/image @async-copy-state-atom)]
      (when-not (identical? (.getImage image-view) new-image)
        (.setImage image-view new-image)))))

(defn- nudge! [scene-node-ids ^double dx ^double dy ^double dz]
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (for [node-id scene-node-ids]
        (let [delta (Vector3d. dx dy dz)]
          (s/assert
            :manip/tx-data
            (:manip/tx-data
              (scene-tools/manip-move node-id delta :manip-phase/commit evaluation-context))))))))

(defn- selection->movable
  ([selection]
   (g/with-auto-evaluation-context evaluation-context
     (selection->movable selection evaluation-context)))
  ([selection evaluation-context]
   (handler/selection->node-ids selection scene-tools/manip-movable? evaluation-context)))

(handler/defhandler :scene.move-up :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 0.0 1.0 0.0)))

(handler/defhandler :scene.move-down :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 0.0 -1.0 0.0)))

(handler/defhandler :scene.move-left :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) -1.0 0.0 0.0)))

(handler/defhandler :scene.move-right :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 1.0 0.0 0.0)))

(handler/defhandler :scene.move-up-major :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 0.0 10.0 0.0)))

(handler/defhandler :scene.move-down-major :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 0.0 -10.0 0.0)))

(handler/defhandler :scene.move-left-major :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) -10.0 0.0 0.0)))

(handler/defhandler :scene.move-right-major :workbench
  (active? [selection evaluation-context] (selection->movable selection evaluation-context))
  (run [selection] (nudge! (selection->movable selection) 10.0 0.0 0.0)))

(defn- attempt-handle-arrow-key-commands! [^KeyEvent event]
  (if (or (.isAltDown event) (.isMetaDown event) (.isShiftDown event) (.isShortcutDown event))
    ::unhandled
    (condp = (.getCode event)
      KeyCode/UP (ui/run-command (.getSource event) :scene.move-up)
      KeyCode/DOWN (ui/run-command (.getSource event) :scene.move-down)
      KeyCode/LEFT (ui/run-command (.getSource event) :scene.move-left)
      KeyCode/RIGHT (ui/run-command (.getSource event) :scene.move-right)
      ::unhandled)))

(defn register-event-handler! [^Parent parent image-view view-id]
  (g/user-data! view-id ::input-state (i/make-input-state))
  (g/user-data! view-id ::input-action-queue [])
  (let [process-events? (atom true)
        camera-id (view->camera view-id)
        event-handler
        (ui/event-handler e
          (when @process-events?
            (try
              (profiler/profile "input-event" -1
                (if (c/free-cam-mode-active? camera-id)
                  (g/user-data-swap! view-id ::input-action-queue conj (i/action-from-jfx e))
                  (let [{:keys [x y screen-x screen-y] :as action} (augment-action view-id (i/action-from-jfx e))
                        pos [x y 0.0]
                        picking-rect (selection/calc-picking-rect pos pos)]
                    (when (= :mouse-pressed (:type action))
                      (.requestFocus parent)
                      (.consume e))
                    (g/user-data-swap! view-id ::input-action-queue conj action)
                    (g/transact
                      (concat
                        (when screen-x
                          (g/set-property view-id :cursor-pos [x y]))
                        (g/set-property view-id :tool-picking-rect picking-rect))))))
              (catch Throwable error
                (reset! process-events? false)
                (error-reporting/report-exception! error)))))
        window-focused-property (-> parent
                                    (.sceneProperty)
                                    (.flatMap Scene/.windowProperty)
                                    (.flatMap Window/.focusedProperty)
                                    (.orElse false))
        ^ChangeListener on-window-focus-changed
        (fn [_ _ focused]
          (when (and (not focused)
                     (c/free-cam-mode-active? camera-id))
            (c/stop-free-cam-mode! image-view camera-id)
            (g/user-data! view-id ::input-state (i/make-input-state))))]
    (.addListener window-focused-property on-window-focus-changed)
    ;; NOTE: Preserve a strong ref to prevent GC from collecting the weakly referenced ChangeLIstener
    (.put (.getProperties parent) ::window-focused-property window-focused-property)
    (doto parent
      (ui/on-mouse! (fn [type _]
                      (cond (= type :exit)
                            (g/set-property! view-id :cursor-pos nil))))
      (.setOnMousePressed event-handler)
      (.setOnMouseReleased event-handler)
      (.setOnMouseClicked event-handler)
      (.setOnMouseMoved event-handler)
      (.setOnMouseDragged event-handler)
      (.setOnDragDetected event-handler)
      (.setOnDragOver event-handler)
      (.setOnDragDropped event-handler)
      (.setOnScroll event-handler)
      (.addEventFilter KeyEvent/KEY_RELEASED
        (ui/event-handler e
          (let [action (i/action-from-jfx e)
                input-state (i/update-input-state (g/user-data view-id ::input-state) action)]
            (g/user-data-swap! view-id ::input-action-queue conj action)
            (when (contains? (:mouse-buttons input-state) :secondary)
              (.consume e)))))
      (.addEventFilter KeyEvent/KEY_PRESSED
        (ui/event-handler e
          (when @process-events?
            (let [action (i/action-from-jfx e)]
              (g/user-data-swap! view-id ::input-action-queue conj action)
              ;; Always interpret UP/DOWN/LEFT/RIGHT as move commands because otherwise they
              ;; would be consumed by the TabPane and will trigger next/prev tab selection.
              ;; Because of that, such key presses will not reach the workbench view and
              ;; will not trigger the commands as might be expected
              (when (or (c/free-cam-mode-active? camera-id)
                        (contains? (:mouse-buttons (g/user-data view-id ::input-state)) :secondary)
                        (not= ::unhandled (attempt-handle-arrow-key-commands! e)))
                (.consume e)))))))))

(defn make-gl-pane! [view-id opts]
  (let [image-view (doto (ImageView.)
                     (.setScaleY -1.0)
                     (.setFocusTraversable true)
                     (.setPreserveRatio false)
                     (.setSmooth false))
        pane (proxy [com.defold.control.Region] []
               (layoutChildren []
                 (let [this ^com.defold.control.Region this
                       width (.getWidth this)
                       height (.getHeight this)]
                   (try
                     (.setFitWidth image-view width)
                     (.setFitHeight image-view height)
                     (proxy-super layoutInArea ^Node image-view 0.0 0.0 width height 0.0 HPos/CENTER VPos/CENTER)
                     (when (and (> width 0) (> height 0))
                       (let [viewport (types/->Region 0 width 0 height)]
                         (g/transact (g/set-property view-id :viewport viewport))
                         (if-let [view-id (ui/user-data image-view ::view-id)]
                           (when-some [drawable ^GLOffscreenAutoDrawable (g/node-value view-id :drawable)]
                             (doto drawable
                               (.setSurfaceSize width height))
                             (let [async-copy-state-atom (g/node-value view-id :async-copy-state)]
                               (reset! async-copy-state-atom (scene-async/request-resize! @async-copy-state-atom width height))))
                           (let [drawable (gl/offscreen-drawable width height)
                                picking-drawable (gl/offscreen-drawable picking-drawable-size picking-drawable-size)
                                camera-inset-drawable (when (supports-camera-inset-drawable? view-id)
                                                        (gl/offscreen-drawable camera-inset-width camera-inset-height))]
                             (ui/user-data! image-view ::view-id view-id)
                             (register-event-handler! this image-view view-id)
                             (ui/on-closed! (:tab opts) (fn [_]
                                                          (ui/kill-event-dispatch! this)
                                                          (dispose-scene-view! view-id)))
                            (if camera-inset-drawable
                              (g/set-properties! view-id
                                                :drawable drawable
                                                :picking-drawable picking-drawable
                                                :camera-inset-drawable camera-inset-drawable
                                                :async-copy-state (atom (scene-async/make-async-copy-state width height)))
                              (g/set-properties! view-id
                                                :drawable drawable
                                                :picking-drawable picking-drawable
                                                :async-copy-state (atom (scene-async/make-async-copy-state width height))))
                             (frame-selection! view-id false)))))
                     (catch Throwable error
                       (error-reporting/report-exception! error)))
                   (proxy-super layoutChildren))))]
    (.setFocusTraversable pane true)
    (.add (.getChildren pane) image-view)
    (g/set-property! view-id :image-view image-view)
    pane))

(defn- make-scene-view-pane [view-id opts]
  (let [scene-view-pane ^Pane (ui/load-fxml "scene-view.fxml")]
    (ui/fill-control scene-view-pane)
    (let [gl-pane (make-gl-pane! view-id opts)]
      (ui/fill-control gl-pane)
      (.add (.getChildren scene-view-pane) 0 gl-pane))
    (when (system/defold-dev?)
      (.setOnKeyPressed scene-view-pane (ui/event-handler event
                                          (let [key-event ^KeyEvent event]
                                            (when (and (.isShortcutDown key-event)
                                                       (= "t" (.getText key-event)))
                                              (g/update-property! view-id :render-mode render-mode-transitions))))))
    scene-view-pane))

(defn- make-scene-view [scene-graph ^Parent parent opts]
  (let [view-id (g/make-node! scene-graph SceneView :updatable-states {} :app-view (:app-view opts))
        scene-view-pane (make-scene-view-pane view-id opts)]
    (ui/children! parent [scene-view-pane])
    (ui/with-controls scene-view-pane [overlay-anchor-pane]
      (g/set-property! view-id :overlay-anchor-pane overlay-anchor-pane))
    view-id))

(g/defnk produce-frame [all-renderables ^Region viewport pass->render-args ^GLAutoDrawable drawable]
  (when drawable
    (gl/with-drawable-as-current drawable
      (scene-cache/process-pending-deletions! gl)
      (render! gl-context :normal all-renderables nil viewport pass->render-args)
      (let [[w h] (vp-dims viewport)
            buf-image (read-to-buffered-image w h)]
        (scene-cache/prune-context! gl)
        buf-image))))

(g/defnode PreviewView
  (inherits view/WorkbenchView)
  (inherits SceneRenderer)

  (property width g/Num)
  (property height g/Num)
  (property tool-picking-rect Rect)
  (property cursor-pos types/Vec2)
  (property image-view ImageView)
  (property drawable GLAutoDrawable)
  (property picking-drawable GLAutoDrawable)

  (input input-handlers Runnable :array)
  (input update-tick-handlers Runnable :array)
  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input localization g/Any)
  (input keymap g/Any)

  (input hidden-renderable-tags types/RenderableTags)
  (input hidden-node-outline-key-paths types/NodeOutlineKeyPaths)
  (input updatables g/Any)
  (input selected-updatables g/Any)
  (input picking-rect Rect)
  (input tool-info-text g/Str)
  (input tool-renderables pass/RenderData :array)
  (output tool-renderables g/Any produce-tool-renderables)
  (output inactive? g/Bool (g/constantly false))
  (output active-tool g/Keyword (gu/passthrough active-tool))
  (output manip-space g/Keyword (gu/passthrough manip-space))
  (output viewport Region (g/fnk [width height] (types/->Region 0 width 0 height)))
  (output selection g/Any (gu/passthrough selection))
  (output picking-selection g/Any :cached produce-selection) ; PreviewView is used for click-driven tests, so must support regular selection/picking.
  (output tool-selection g/Any :cached produce-tool-selection)
  (output selected-tool-renderables g/Any :cached produce-selected-tool-renderables)
  (output frame BufferedImage produce-frame)
  (output all-renderables pass/RenderData :cached (g/fnk [scene-render-data] (:renderables (merge-render-datas {} {} scene-render-data))))
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output displayed-node-properties g/Any :cached
          (g/fnk [selected-node-properties preview-overrides]
            (displayed-node-properties selected-node-properties preview-overrides))))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView
                :width width
                :height height
                :drawable (gl/offscreen-drawable width height)
                :picking-drawable (gl/offscreen-drawable picking-drawable-size picking-drawable-size)))

(defmulti attach-grid
  (fn [grid-node-type grid-node-id view-id resource-node camera]
    (:key @grid-node-type)))

(defmethod attach-grid :editor.grid/Grid
  [_ grid-node-id view-id resource-node camera]
  (concat
    (g/connect grid-node-id :_node-id view-id :grid)
    (g/connect grid-node-id :renderable view-id :aux-renderables)
    (g/connect camera :camera grid-node-id :camera)))

(defmulti attach-tool-controller
  (fn [tool-node-type tool-node-id view-id resource-node]
    (:key @tool-node-type)))

(defmethod attach-tool-controller :default
  [_ tool-node view-id resource-node])

(defn setup-view [view-id resource-node opts]
  (let [view-graph           (g/node-id->graph-id view-id)
        app-view-id          (:app-view opts)
        select-fn            (:select-fn opts)
        prefs                (:prefs opts)
        grid-type            (cond
                               (true? (:grid opts)) grid/Grid
                               (:grid opts) (:grid opts)
                               :else grid/Grid)
        tool-controller-type (get opts :tool-controller scene-tools/ToolController)]
    (g/make-nodes view-graph
                  [background      background/Background
                   selection       [selection/SelectionController :drop-fn (:drop-fn opts)
                                                                  :select-fn (fn [selection op-seq]
                                                                               (g/transact
                                                                                 (concat
                                                                                   (g/operation-sequence op-seq)
                                                                                   (g/operation-label (localization/message "operation.select"))
                                                                                   (select-fn selection))))]
                   camera          [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic identity {:fov-x 1000 :fov-y 1000}))
                                                       :image-view (g/node-value view-id :image-view)
                                                       :prefs prefs]
                   grid            (grid-type :prefs prefs)
                   tool-controller [tool-controller-type :prefs prefs]
                   rulers          [rulers/Rulers]]

                  (g/connect resource-node   :scene                         view-id         :scene)

                  (g/connect background      :renderable                    view-id         :aux-renderables)

                  (g/connect camera          :local-camera                  view-id         :local-camera)
                  (g/connect camera          :camera                        view-id         :camera)
                  (g/connect camera          :input-handler                 view-id         :input-handlers)
                  (g/connect camera          :update-tick-handler           view-id         :update-tick-handlers)
                  (g/connect camera          :cursor-type                   view-id         :cursor-type)
                  (g/connect view-id         :scene-aabb                    camera          :scene-aabb)
                  (g/connect view-id         :viewport                      camera          :viewport)

                  (g/connect app-view-id     :selected-node-ids             view-id         :selection)
                  (g/connect app-view-id     :active-view                   view-id         :active-view)
                  (g/connect app-view-id     :active-tool                   view-id         :active-tool)
                  (g/connect app-view-id     :manip-space                   view-id         :manip-space)
                  (g/connect app-view-id     :hidden-renderable-tags        view-id         :hidden-renderable-tags)
                  (g/connect app-view-id     :hidden-node-outline-key-paths view-id         :hidden-node-outline-key-paths)
                  (g/connect app-view-id     :keymap                        view-id         :keymap)
                  (g/connect app-view-id     :localization                  view-id         :localization)
                  (g/connect app-view-id     :keymap                        camera          :keymap)

                  (g/connect tool-controller :input-handler                 view-id         :input-handlers)
                  (g/connect tool-controller :info-text                     view-id         :tool-info-text)
                  (g/connect tool-controller :renderables                   view-id         :tool-renderables)
                  (g/connect tool-controller :preview-overrides             view-id         :preview-overrides)
                  (g/connect view-id         :active-tool                   tool-controller :active-tool)
                  (g/connect view-id         :manip-space                   tool-controller :manip-space)
                  (g/connect view-id         :viewport                      tool-controller :viewport)
                  (g/connect camera          :camera                        tool-controller :camera)
                  (g/connect view-id         :selected-renderables          tool-controller :selected-renderables)

                  (attach-tool-controller tool-controller-type tool-controller view-id resource-node)

                  (if (:grid opts)
                    (attach-grid grid-type grid view-id resource-node camera)
                    (g/delete-node grid))

                  (g/connect resource-node   :_node-id                      selection       :root-id)
                  (g/connect selection       :renderable                    view-id         :tool-renderables)
                  (g/connect selection       :input-handler                 view-id         :input-handlers)
                  (g/connect selection       :picking-rect                  view-id         :picking-rect)
                  (g/connect view-id         :picking-selection             selection       :picking-selection)
                  (g/connect view-id         :selection                     selection       :selection)

                  (g/connect camera :camera rulers :camera)
                  (g/connect rulers :renderables view-id :aux-renderables)
                  (g/connect view-id :viewport rulers :viewport)
                  (g/connect view-id :cursor-pos rulers :cursor-pos)

                  (when-not (:manual-refresh? opts)
                    (g/connect view-id :_node-id app-view-id :scene-view-ids)))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view-id (make-scene-view graph parent opts)]
    (g/transact
      (setup-view view-id resource-node opts))
    view-id))

(defn make-preview [graph resource-node opts width height]
  (let [view-id (make-preview-view graph width height)
        opts (-> opts
                 (assoc :manual-refresh? true)
                 (dissoc :grid))]
    (g/transact
      (setup-view view-id resource-node opts))
    (frame-selection! view-id false)
    view-id))

(defn dispose-preview [node-id]
  (when-some [^GLAutoDrawable drawable (g/node-value node-id :drawable)]
    (gl/with-drawable-as-current drawable
      (scene-cache/drop-context! gl))
    (.destroy drawable)
    (g/set-property! node-id :drawable nil)))

(defn- focus-view! [view-id _opts done-fn]
  (if-some [^ImageView image-view (g/node-value view-id :image-view)]
    (do
      (.requestFocus image-view)
      (ui/run-later
        (refresh-scene-view! view-id 1/60)
        (done-fn)))
    (done-fn)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :scene
                                :label (localization/message "resource.view.scene")
                                :make-view-fn make-view
                                :make-preview-fn make-preview
                                :dispose-preview-fn dispose-preview
                                :focus-fn focus-view!))

(g/defnk produce-transform [position rotation scale]
  (math/clj->mat4 position rotation scale))

(g/defnk produce-pose [position rotation scale]
  (pose/make position rotation scale))

(def produce-no-transform-properties (g/constantly #{}))
(def produce-scalable-transform-properties (g/constantly #{:position :rotation :scale}))
(def produce-unscalable-transform-properties (g/constantly #{:position :rotation}))

;; Arbitrarily small value to avoid 0-determinants
(def ^:private ^:const ^float scale-min-float (float 0.000001))

(def ^:private ^:const ^double scale-min-double 0.000001)

(defn- non-zeroify-component [num]
  (cond
    (instance? Float num)
    (let [num-float (float num)]
      (if (< (Math/abs num-float) scale-min-float)
        (Math/copySign scale-min-float num-float)
        num-float))

    (double? num)
    (let [num-double (double num)]
      (if (< (Math/abs num-double) scale-min-double)
        (Math/copySign scale-min-double num-double)
        num-double))

    :else
    num))

(defn non-zeroify-scale [scale]
  (coll/transform-> scale
    (map non-zeroify-component)))

(g/defnode SceneNode
  (property position types/Vec3 (default default-position)
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :position))))
  (property rotation types/Vec4 (default default-rotation)
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :rotation)))
            (dynamic edit-type (g/constantly properties/quat-rotation-edit-type)))
  (property scale types/Vec3 (default default-scale)
            (dynamic edit-type (g/constantly {:type types/Vec3 :precision 0.1}))
            (dynamic visible (g/fnk [transform-properties] (contains? transform-properties :scale)))
            (set (fn [_evaluation-context self _old-value new-value]
                   (when (some? new-value)
                     (g/set-property self :scale (non-zeroify-scale new-value))))))

  (output transform-properties g/Any :abstract)
  (output transform Matrix4d :cached produce-transform)
  (output pose Pose produce-pose)
  (output scene g/Any :cached (g/fnk [^g/NodeID _node-id ^Matrix4d transform] {:node-id _node-id :transform transform})))

(defmethod scene-tools/manip-movable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :position))

(defmethod scene-tools/manip-rotatable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :rotation))

(defmethod scene-tools/manip-scalable? ::SceneNode [node-id]
  (contains? (g/node-value node-id :transform-properties) :scale))

(defn- manip-scene-node [node-id prop-kw apply-delta-fn vecmath-delta manip-phase initial-evaluation-context]
  (let [old-value (g/node-value node-id prop-kw initial-evaluation-context)
        new-value (apply-delta-fn old-value vecmath-delta)]
    (case manip-phase
      :manip-phase/commit
      {:manip/tx-data (g/set-property node-id prop-kw new-value)}

      :manip-phase/preview
      {:manip/prop-kw->override-value {prop-kw new-value}})))

(defn apply-move-delta [old-clj-position vecmath-delta]
  (let [old-vecmath-position (doto (Vector3d.) (math/clj->vecmath old-clj-position))
        new-vecmath-position (math/add-vector old-vecmath-position vecmath-delta)
        num-fn (if (math/float32? (first old-clj-position))
                 properties/round-scalar-float
                 properties/round-scalar)]
    (into (coll/empty-with-meta old-clj-position)
          (map num-fn)
          (math/vecmath->clj new-vecmath-position))))

(defn manip-move-scene-node [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (manip-scene-node node-id :position apply-move-delta delta manip-phase initial-evaluation-context))

(defmethod scene-tools/manip-move ::SceneNode [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (manip-move-scene-node node-id delta manip-phase initial-evaluation-context))

(defn apply-rotate-delta [old-clj-rotation vecmath-delta]
  ;; Note! The rotation is not rounded here like we do for apply-move-delta and
  ;; apply-scale-delta. As the user-facing property is the euler angles, they
  ;; will be rounded in properties/quat->euler.
  (let [new-vecmath-rotation (doto (Quat4d.) (math/clj->vecmath old-clj-rotation) (.mul vecmath-delta))]
    (if (math/float32? (first old-clj-rotation))
      (into (coll/empty-with-meta old-clj-rotation)
            (map float)
            (math/vecmath->clj new-vecmath-rotation))
      (math/vecmath-into-clj new-vecmath-rotation
                             (coll/empty-with-meta old-clj-rotation)))))

(defn manip-rotate-scene-node [node-id ^Quat4d delta manip-phase initial-evaluation-context]
  (manip-scene-node node-id :rotation apply-rotate-delta delta manip-phase initial-evaluation-context))

(defmethod scene-tools/manip-rotate ::SceneNode [node-id ^Quat4d delta manip-phase initial-evaluation-context]
  (manip-rotate-scene-node node-id delta manip-phase initial-evaluation-context))

(defn apply-scale-delta [old-scale vecmath-delta]
  (properties/scale-and-round-vec old-scale vecmath-delta))

(defn manip-scale-scene-node [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (manip-scene-node node-id :scale apply-scale-delta delta manip-phase initial-evaluation-context))

(defmethod scene-tools/manip-scale ::SceneNode [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (manip-scale-scene-node node-id delta manip-phase initial-evaluation-context))
