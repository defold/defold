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

(ns editor.light
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.data :as data]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.image-util :as image-util]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.scene-picking :as scene-picking]
            [editor.scene-shapes :as scene-shapes]
            [editor.scene-tools :as scene-tools]
            [editor.shaders :as shaders]
            [editor.types :as types]
            [editor.validation :as validation]
            [util.array :as array]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix3d Matrix4d Point3d Quat4d Tuple3d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(vtx/defvertex tex-color-vtx
  (vec3 position)
  (vec2 texcoord0)
  (vec4 color))

(vtx/defvertex selection-icon-vtx
  (vec3 position)
  (vec2 texcoord0)
  (vec4 id-color))

(def ^:private outline-shader shaders/basic-color-world-space)
(def ^:private light-icon-shader shaders/basic-texture-color-world-space)
(def ^:private light-icon-selection-shader shaders/selection-attribute-world-space)

(def ^:private ^:const billboard-circle-segments 32)
(def ^:private ^:const gizmo-target-pixels 100.0)
;; Screen-space half-extent (pixels) for the light icon quad; world size = scale-factor * this (see camera preview mesh).
(def ^:private ^:const light-icon-pixels 8.0)
(def ^:private ^:const max-spot-cone-angle 180.0)

(def ^:private outline-icon "icons/64/Icons_21-Light.png")

(def ^:private intensity-message (properties/label-message :light :intensity))
(def ^:private range-message (properties/label-message :light :range))
(def ^:private inner-cone-angle-message (properties/label-message :light :inner-cone-angle))
(def ^:private outer-cone-angle-message (properties/label-message :light :outer-cone-angle))

(defn- make-icon-gpu-texture-delay [request-id icon-resource-pathname]
  (delay
    (texture/image-texture
      request-id
      (image-util/read-image (io/resource icon-resource-pathname))
      (merge texture/default-image-texture-params
             {:min-filter gl/linear
              :mag-filter gl/linear}))))

(def ^:private icon-omni-gpu-texture-delay (make-icon-gpu-texture-delay ::icon-omni-gpu-texture "icons/scene/light_omni.png"))
(def ^:private icon-spot-gpu-texture-delay (make-icon-gpu-texture-delay ::icon-spot-gpu-texture "icons/scene/light_spot.png"))
(def ^:private icon-sun-gpu-texture-delay (make-icon-gpu-texture-delay ::icon-sun-gpu-texture "icons/scene/light_sun.png"))
(def ^:private icon-ambient-gpu-texture-delay (make-icon-gpu-texture-delay ::icon-ambient-gpu-texture "icons/scene/light_ambient.png"))

(defn- clamp [v low high]
  (-> (double v)
      (max (double low))
      (min (double high))))

(defn- clamp-spot-cone-angles [inner-cone-angle outer-cone-angle]
  (let [outer-cone-angle (clamp outer-cone-angle 0.0 max-spot-cone-angle)
        inner-cone-angle (clamp inner-cone-angle 0.0 outer-cone-angle)]
    [inner-cone-angle outer-cone-angle]))

(defn- unify-scale [renderable]
  (let [{:keys [^Quat4d world-rotation
                ^Vector3d world-scale
                ^Vector3d world-translation]} renderable
        min-scale (min (Math/abs (.-x world-scale))
                       (Math/abs (.-y world-scale))
                       (Math/abs (.-z world-scale)))
        physics-world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setScale min-scale)
                                  (.setTranslation world-translation)
                                  (.setRotation world-rotation))]
    (assoc renderable :world-transform physics-world-transform)))

(defn- renderable-min-scale [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (Math/abs (.-x ws))
         (Math/abs (.-y ws))
         (Math/abs (.-z ws)))
    1.0))

(defn- world-dir-from-light [renderable]
  (let [d (Vector3d. 0.0 0.0 -1.0)
        v (math/transform-vector (:world-transform renderable) d)]
    (if (> (.lengthSquared v) 1e-20)
      (doto v
        (.normalize))
      (Vector3d. 0.0 0.0 -1.0))))

(defn- billboard-axes [^Vector3d world-center camera]
  (let [cam-pos (Vector3d. (types/position camera))
        to-cam (doto (Vector3d.) (.sub cam-pos world-center))
        nlen (Math/sqrt (+ (* (.x to-cam) (.x to-cam)) (* (.y to-cam) (.y to-cam)) (* (.z to-cam) (.z to-cam))))
        to-cam (if (< nlen 1e-8)
                 (Vector3d. 0.0 0.0 1.0)
                 (doto (Vector3d. (.x to-cam) (.y to-cam) (.z to-cam)) (.scale (/ 1.0 nlen))))
        world-up (Vector3d. 0.0 1.0 0.0)
        right (doto (Vector3d.) (.cross world-up to-cam))
        rlen (Math/sqrt (+ (* (.x right) (.x right)) (* (.y right) (.y right)) (* (.z right) (.z right))))]
    (if (< rlen 1e-6)
      (let [alt (Vector3d. 1.0 0.0 0.0)
            right (doto (Vector3d.) (.cross alt to-cam))]
        (.normalize right)
        (let [up (doto (Vector3d.) (.cross to-cam right) (.normalize))]
          [right up to-cam]))
      (let [right (doto right (.scale (/ 1.0 rlen)))
            up (doto (Vector3d.) (.cross to-cam right) (.normalize))]
        [right up to-cam]))))

(defn- billboard-point-from-axes
  "World position = center + coeff-right * right + coeff-up * up (same basis as billboard-axes)."
  [^Vector3d center ^Vector3d right ^Vector3d up coeff-right coeff-up]
  (let [ar (double coeff-right)
        au (double coeff-up)]
    [(+ (.x center) (* ar (.x right)) (* au (.x up)))
     (+ (.y center) (* ar (.y right)) (* au (.y up)))
     (+ (.z center) (* ar (.z right)) (* au (.z up)))]))

(defn- billboard-quad-corners
  "World-space corners of a camera-facing quad: bottom-left, top-left, top-right, bottom-right."
  [^Vector3d c ^Vector3d right ^Vector3d up h]
  (let [h (double h)
        bl (billboard-point-from-axes c right up (- h) (- h))
        tl (billboard-point-from-axes c right up (- h) h)
        tr (billboard-point-from-axes c right up h h)
        br (billboard-point-from-axes c right up h (- h))]
    [bl tl tr br]))

(defn- vbuf-push-quad!
  [vbuf corners cr cg cb]
  (let [cr (double cr)
        cg (double cg)
        cb (double cb)
        [[blx bly blz] [tlx tly tlz] [trx try trz] [brx bry brz]] corners]
    (-> vbuf
        (conj! [blx bly blz 0.0 0.0 cr cg cb 1.0])
        (conj! [tlx tly tlz 0.0 1.0 cr cg cb 1.0])
        (conj! [trx try trz 1.0 1.0 cr cg cb 1.0])
        (conj! [trx try trz 1.0 1.0 cr cg cb 1.0])
        (conj! [brx bry brz 1.0 0.0 cr cg cb 1.0])
        (conj! [blx bly blz 0.0 0.0 cr cg cb 1.0]))))

;; Same mesh as scene-tools/move-arrow-vertex-groups (translation move-x/y/z): cone + shaft along +X, length ~100 units.
(def ^:private directional-arrow-vertex-groups (scene-tools/move-arrow-vertex-groups))

(def ^:private directional-arrow-mesh-lens
  (reduce (fn [[^long tris ^long lines] [mode vs]]
            (let [c (long (count vs))]
              (cond
                (= mode GL/GL_TRIANGLES) [(+ tris c) lines]
                (= mode GL/GL_LINES) [tris (+ lines c)]
                :else [tris lines])))
          [0 0]
          directional-arrow-vertex-groups))

(def ^:private directional-arrow-tri-vert-count (long (first directional-arrow-mesh-lens)))
(def ^:private directional-arrow-line-vert-count (long (second directional-arrow-mesh-lens)))

(defn- mat3-x-axis-to-dir
  ^Matrix3d [^Vector3d dir]
  ;; Translation gizmo arrow is modeled on +X; map local +X to world light direction (column 0 = dir).
  (let [x (Vector3d. dir)
        x-len (.length x)]
    (if (< x-len 1e-10)
      (doto (Matrix3d.) (.setIdentity))
      (do
        (.scale x (/ 1.0 x-len))
        (let [ref (if (< (Math/abs (.x x)) 0.9)
                    (Vector3d. 1.0 0.0 0.0)
                    (Vector3d. 0.0 1.0 0.0))
              y (doto (Vector3d.) (.cross ref x))
              y-len (.length y)]
          (if (< y-len 1e-8)
            (doto (Matrix3d.) (.setIdentity))
            (let [y (doto y (.scale (/ 1.0 y-len)))
                  z (doto (Vector3d.) (.cross x y))]
              (.normalize z)
              (doto (Matrix3d.)
                (.setColumn 0 x)
                (.setColumn 1 y)
                (.setColumn 2 z)))))))))

(defn- transform-local-arrow-point
  ^Vector3d [^Matrix3d rotation-matrix ^Vector3d p s lx ly lz]
  ;; No ^double arg hints: primitive fns are limited to 4 parameters in Clojure.
  (let [s (double s)
        lx (double lx)
        ly (double ly)
        lz (double lz)
        tv (Vector3d. (* s lx) (* s ly) (* s lz))]
    (.transform rotation-matrix tv)
    (.add tv p)
    tv))

(defn- vbuf-push-camera-facing-circle! [vbuf ^Vector3d center ^Vector3d right ^Vector3d up radius cr cg cb]
  (let [segments (long billboard-circle-segments)
        radius (double radius)
        cr (double cr)
        cg (double cg)
        cb (double cb)]
    (reduce (fn [vbuf i]
              (let [i (long i)
                    t0 (* 2.0 Math/PI (/ (double i) segments))
                    t1 (* 2.0 Math/PI (/ (double (unchecked-inc i)) segments))
                    cos0 (Math/cos t0) sin0 (Math/sin t0)
                    cos1 (Math/cos t1) sin1 (Math/sin t1)
                    ar0 (* radius cos0) au0 (* radius sin0)
                    ar1 (* radius cos1) au1 (* radius sin1)
                    [x0 y0 z0] (billboard-point-from-axes center right up ar0 au0)
                    [x1 y1 z1] (billboard-point-from-axes center right up ar1 au1)]
                (-> vbuf
                    (conj! [x0 y0 z0 cr cg cb 1.0])
                    (conj! [x1 y1 z1 cr cg cb 1.0]))))
            vbuf
            (range segments))))

(defn- light-gizmo-selected? [renderable]
  (#{:self-selected :parent-selected} (:selected renderable)))

(defn- light-gizmo-opened-in-isolation? [renderable]
  ;; Returns true if the renderable is from a light resource opened in an editor
  ;; tab in isolation.
  (and (nil? (:selected renderable))
       (empty? (:node-id-path renderable))))

(defn- light-gizmo-visible? [renderable]
  (or (light-gizmo-selected? renderable)
      (light-gizmo-opened-in-isolation? renderable)))

(defn- finite-positive? [^double x]
  (and (Double/isFinite x)
       (pos? x)))

(defn- camera-projectable? [camera ^Tuple3d point]
  (or (not= :perspective (:type camera))
      (let [to-point (doto (Vector3d. point)
                       (.sub (types/position camera)))
            view-depth (math/dot (camera/camera-forward-vector camera) to-point)]
        (not= 0.0 view-depth))))

(defn- light-gizmo-scale-factor [camera viewport ^Tuple3d reference-point]
  ;; camera-project asserts when a perspective-projected point ends up with
  ;; w == 0. That happens for points on the camera plane, so skip those gizmos
  ;; for this pass instead of letting a light icon take down scene rendering.
  (if (camera-projectable? camera reference-point)
    (scene-tools/scale-factor camera viewport reference-point)
    Double/NaN))

(defn- light-icon-render-color [renderable]
  (or (colors/selection-color (:selected renderable))
      (:color (:user-data renderable))))

(defn- light-icon-picking-color [renderable]
  (scene-picking/picking-id->color (:picking-id renderable)))

(defn- vbuf-push-light-icon-quads! [vbuf camera viewport renderables color-fn]
  (persistent!
    (reduce (fn [vbuf renderable]
              (let [^Vector3d world-translation (:world-translation renderable)
                    sf (light-gizmo-scale-factor camera viewport world-translation)
                    h (* 2.0 (double sf) (double light-icon-pixels))]
                (if (and (finite-positive? sf)
                         (finite-positive? h))
                  (if-some [axes (billboard-axes world-translation camera)]
                    (let [[^Vector3d right ^Vector3d up _] axes
                          [cr cg cb] (color-fn renderable)
                          corners (billboard-quad-corners world-translation right up h)]
                      (vbuf-push-quad! vbuf corners cr cg cb))
                    vbuf)
                  vbuf)))
            vbuf
            renderables)))

(defn- render-light-icon-impl [^GL2 gl render-args renderables renderable-count]
  (let [renderable-count (long renderable-count)]
    (when (pos? renderable-count)
      (let [camera (:camera render-args)
            viewport (:viewport render-args)
            icon-gpu-texture (:icon-gpu-texture (:user-data (first renderables)))]
        (condp = (:pass render-args)
          pass/transparent
          (let [vbuf (vbuf-push-light-icon-quads! (->tex-color-vtx (* renderable-count 6)) camera viewport renderables light-icon-render-color)
                vertex-binding (vtx/use-with ::light-icon vbuf light-icon-shader)]
            (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)
            (gl/gl-enable gl GL/GL_BLEND)
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
            (try
              (gl/with-gl-bindings gl render-args [light-icon-shader vertex-binding icon-gpu-texture]
                (shader/set-samplers-by-index light-icon-shader gl 0 (:texture-units icon-gpu-texture))
                (.glDrawArrays gl GL/GL_TRIANGLES 0 (count vbuf)))
              (finally
                (gl/gl-disable gl GL/GL_BLEND)
                (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_LINE))))

          pass/selection
          (let [vbuf (vbuf-push-light-icon-quads! (->selection-icon-vtx (* renderable-count 6)) camera viewport renderables light-icon-picking-color)
                vertex-binding (vtx/use-with ::light-icon-selection vbuf light-icon-selection-shader)]
            (gl/with-gl-bindings gl render-args [light-icon-selection-shader vertex-binding icon-gpu-texture]
              (shader/set-samplers-by-index light-icon-selection-shader gl 0 (:texture-units icon-gpu-texture))
              (.glDrawArrays gl GL/GL_TRIANGLES 0 (count vbuf)))))))))

(defn- render-point-outline-impl [^GL2 gl render-args renderables renderable-count]
  (assert (= pass/outline (:pass render-args)))
  (let [camera (:camera render-args)
        vbuf (persistent!
               (reduce (fn [vbuf renderable]
                         (if (light-gizmo-visible? renderable)
                           (let [{:keys [range]} (:user-data renderable)
                                 [cr cg cb] (colors/renderable-outline-color renderable)
                                 ^Vector3d world-translation (:world-translation renderable)
                                 r (* (double (or range 1.0)) (double (renderable-min-scale renderable)))
                                 r (max r 0.01)]
                             (if-some [axes (billboard-axes world-translation camera)]
                               (let [[^Vector3d right ^Vector3d up _] axes]
                                 (vbuf-push-camera-facing-circle! vbuf world-translation right up r cr cg cb))
                               vbuf))
                           vbuf))
                       (->color-vtx (* (long renderable-count) billboard-circle-segments 2))
                       renderables))]
    (when (pos? (count vbuf))
      (gl/gl-enable gl GL/GL_DEPTH_TEST)
      (.glDepthMask gl false)
      (try
        (let [vb (vtx/use-with ::light-point-gizmo-lines vbuf outline-shader)]
          (gl/with-gl-bindings gl render-args [outline-shader vb]
            (.glDrawArrays gl GL/GL_LINES 0 (count vbuf))))
        (finally
          (gl/gl-disable gl GL/GL_DEPTH_TEST))))))

(defn- vbuf-push-directional-arrow! [vbuf arrow-mode ^Vector3d p ^Vector3d d-world cr cg cb total-len ca]
  (let [rotation-matrix (mat3-x-axis-to-dir d-world)
        s (/ (double total-len) 100.0)
        cr (double cr)
        cg (double cg)
        cb (double cb)]
    (doseq [[mode vs] directional-arrow-vertex-groups
            :when (= mode arrow-mode)]
      (doseq [v vs]
        (let [^Vector3d w (transform-local-arrow-point rotation-matrix p s (double (nth v 0)) (double (nth v 1)) (double (nth v 2)))]
          (conj! vbuf [(.x w) (.y w) (.z w) cr cg cb ca]))))))

(defn- vbuf-push-directional-outlines! [initial-vbuf render-args renderables arrow-mode]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)]
    (persistent!
      (reduce (fn [vbuf renderable]
                (if (light-gizmo-visible? renderable)
                  (let [[cr cg cb] (colors/renderable-outline-color renderable)
                        ^Vector3d p (:world-translation renderable)
                        d (world-dir-from-light renderable)
                        sf (light-gizmo-scale-factor camera viewport p)
                        total-len (* (double sf) gizmo-target-pixels)]
                    (when (finite-positive? total-len)
                      (vbuf-push-directional-arrow! vbuf arrow-mode p d cr cg cb total-len 1.0))
                    vbuf)
                  vbuf))
              initial-vbuf
              renderables))))

(defn- render-directional-outline-impl [^GL2 gl render-args renderables renderable-count]
  (assert (= pass/outline (:pass render-args)))
  (let [renderable-count (long renderable-count)
        vbuf-tris (vbuf-push-directional-outlines!
                    (->color-vtx (* renderable-count (long directional-arrow-tri-vert-count)))
                    render-args
                    renderables
                    GL/GL_TRIANGLES)
        vbuf-lines (vbuf-push-directional-outlines!
                     (->color-vtx (* renderable-count (long directional-arrow-line-vert-count)))
                     render-args
                     renderables
                     GL/GL_LINES)]
    (when (or (pos? (count vbuf-tris))
              (pos? (count vbuf-lines)))
      (gl/gl-enable gl GL/GL_DEPTH_TEST)
      (.glDepthMask gl false)
      (try
        (when (pos? (count vbuf-tris))
          (let [vb (vtx/use-with ::light-directional-gizmo-tris vbuf-tris outline-shader)]
            (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)
            (try
              (gl/with-gl-bindings gl render-args [outline-shader vb]
                (.glDrawArrays gl GL/GL_TRIANGLES 0 (count vbuf-tris)))
              (finally
                (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_LINE)))))
        (let [vb (vtx/use-with ::light-directional-gizmo-lines vbuf-lines outline-shader)]
          (gl/with-gl-bindings gl render-args [outline-shader vb]
            (.glDrawArrays gl GL/GL_LINES 0 (count vbuf-lines))))
        (finally
          (gl/gl-disable gl GL/GL_DEPTH_TEST))))))

(defn- wrap-uniform-scale [render-fn]
  (fn [gl render-args renderables renderable-count]
    (render-fn gl render-args (mapv unify-scale renderables) renderable-count)))

(defn- render-spot-outline-impl [^GL2 gl render-args renderables _renderable-count]
  (assert (= pass/outline (:pass render-args)))
  (let [visible-gizmos (filterv light-gizmo-visible? renderables)]
    (when (pos? (count visible-gizmos))
      (gl/gl-enable gl GL/GL_DEPTH_TEST)
      (.glDepthMask gl false)
      (try
        ;; TODO: Batched line drawing. Not a good fit for scene-shapes/render-lines due to :point-scale uniform.
        (coll/run!-> visible-gizmos
          (fn [renderable]
            (let [{:keys [^double range ^double outer-cone-angle ^double inner-cone-angle] :as user-data} (:user-data renderable)
                  h (max range 0.01)
                  half-outer (* 0.5 (Math/toRadians outer-cone-angle))
                  half-inner (* 0.5 (Math/toRadians inner-cone-angle))
                  inner-radius-ratio (if (> half-outer 1e-8)
                                       (min 1.0 (max 0.0 (/ (Math/tan half-inner) (Math/tan half-outer))))
                                       0.0)
                  base-r (max (* h (Math/tan half-outer)) 0.02)
                  point-scale (array/of-floats (float base-r) (float base-r) (float h) 1.0)
                  geometry (scene-shapes/light-cone-lines inner-radius-ratio)

                  lines-renderable
                  (-> renderable
                      (unify-scale)
                      (select-keys [:selected :world-transform])
                      (assoc :user-data {:geometry geometry
                                         :point-scale point-scale
                                         :color (:color user-data)}))]
              (scene-shapes/render-lines gl render-args [lines-renderable] 1))))
        (finally
          (gl/gl-disable gl GL/GL_DEPTH_TEST))))))

(def ^:private render-light-icon (wrap-uniform-scale render-light-icon-impl))
(def ^:private render-point-outline (wrap-uniform-scale render-point-outline-impl))
(def ^:private render-directional-outline (wrap-uniform-scale render-directional-outline-impl))
(def ^:private render-spot-outline (wrap-uniform-scale render-spot-outline-impl))

(defn- point-light-preview-fn [visibility-aabb user-data prop-kw->override-value]
  (if-some [range-override (:range prop-kw->override-value)]
    (let [r (max (double range-override) 0.01)
          visibility-aabb (geom/mirrored-point->aabb (Point3d. r r r))
          user-data (assoc user-data
                      :range range-override
                      :editor-preview-light (assoc (:editor-preview-light user-data) :range range-override))]
      [visibility-aabb user-data])
    [visibility-aabb user-data]))

(defn- spot-light-preview-fn
  [visibility-aabb user-data prop-kw->override-value]
  (if-some [range-override (:range prop-kw->override-value)]
    (let [h (max (double range-override) 0.01)
          outer-cone-angle (double (:outer-cone-angle user-data 45.0))
          half-outer (* 0.5 (Math/toRadians outer-cone-angle))
          base-r (max (* h (Math/tan half-outer)) 0.02)
          point-scale (array/of-floats base-r base-r h 1.0)
          max-ext (max base-r h)
          visibility-aabb (geom/mirrored-point->aabb (Point3d. max-ext max-ext max-ext))
          user-data (assoc user-data
                      :range range-override
                      :editor-preview-light (assoc (:editor-preview-light user-data) :range range-override)
                      :point-scale point-scale)]
      [visibility-aabb user-data])
    [visibility-aabb user-data]))

(defn- light-type-icon-gpu-texture [light-type]
  (case light-type
    :point @icon-omni-gpu-texture-delay
    :directional @icon-sun-gpu-texture-delay
    :spot @icon-spot-gpu-texture-delay
    :ambient @icon-ambient-gpu-texture-delay))

(defn- make-light-scene [node-id light-type color intensity range inner-cone-angle outer-cone-angle]
  (let [gizmo-tags #{:light :outline}
        icon-gpu-texture (light-type-icon-gpu-texture light-type)
        icon-batch-key [icon-gpu-texture]

        icon-renderable
        {:render-fn render-light-icon
         :batch-key icon-batch-key
         :tags gizmo-tags
         :passes [pass/transparent pass/selection]
         :user-data {:color color
                     :icon-gpu-texture icon-gpu-texture}}

        preview-light
        {:light-type light-type
         :color color
         :intensity intensity
         :range range
         :inner-cone-angle inner-cone-angle
         :outer-cone-angle outer-cone-angle}]

    (case light-type
      :ambient
      (let [aabb (geom/mirrored-point->aabb (Point3d. 1.5 1.5 1.5))]
        {:node-id node-id
         :aabb aabb
         :renderable (assoc-in icon-renderable [:user-data :editor-preview-light] preview-light)})

      :point
      (let [r (max (double range) 0.01)
            aabb (geom/mirrored-point->aabb (Point3d. r r r))]
        {:node-id node-id
         :aabb aabb
         :renderable icon-renderable
         :children [{:node-id node-id
                     :aabb aabb
                     :renderable {:render-fn render-point-outline
                                  :preview-fn point-light-preview-fn
                                  :batch-key [outline-shader]
                                  :tags gizmo-tags
                                  :passes [pass/outline]
                                  :user-data {:editor-preview-light preview-light
                                              :range range}}}]})

      :directional
      (let [aabb (geom/mirrored-point->aabb (Point3d. 1.5 1.5 1.5))]
        {:node-id node-id
         :aabb aabb
         :renderable icon-renderable
         :children [{:node-id node-id
                     :aabb aabb
                     :renderable {:render-fn render-directional-outline
                                  :batch-key [outline-shader]
                                  :tags gizmo-tags
                                  :passes [pass/outline]
                                  :user-data {:editor-preview-light preview-light}}}]})

      :spot
      (let [h (max (double range) 0.01)
            half-outer (* 0.5 (Math/toRadians (double outer-cone-angle)))
            base-r (max (* h (Math/tan half-outer)) 0.02)
            max-ext (max base-r h)
            aabb (geom/mirrored-point->aabb (Point3d. max-ext max-ext max-ext))]
        {:node-id node-id
         :aabb aabb
         :renderable icon-renderable
         :children [{:node-id node-id
                     :aabb aabb
                     :renderable {:render-fn render-spot-outline
                                  :preview-fn spot-light-preview-fn
                                  :batch-key nil ; Batching is disabled since rendering requires a :point-scale uniform.
                                  :tags gizmo-tags
                                  :passes [pass/outline]
                                  :user-data {:editor-preview-light preview-light
                                              :range range
                                              :inner-cone-angle inner-cone-angle
                                              :outer-cone-angle outer-cone-angle
                                              :color colors/outline-color}}}]}))))

(defn- make-directional-light-scene [node-id color intensity]
  (make-light-scene node-id :directional color intensity 0.0 0.0 0.0))

(defn- make-ambient-light-scene [node-id color intensity]
  (make-light-scene node-id :ambient color intensity 0.0 0.0 0.0))

(defn- make-point-light-scene [node-id color intensity range]
  (make-light-scene node-id :point color intensity range 0.0 0.0))

(defn- make-spot-light-scene [node-id color intensity range inner-cone-angle outer-cone-angle]
  (make-light-scene node-id :spot color intensity range inner-cone-angle outer-cone-angle))

;; -----------------------------------------------------------------------------
;; AmbientLightNode
;; -----------------------------------------------------------------------------

(defn- validate-intensity [node-id intensity]
  (validation/prop-error :fatal node-id :intensity validation/prop-negative? intensity intensity-message))

(g/defnode AmbientLightNode
  (inherits data/DataResourceNode)

  (property color types/Color (default [1.0 1.0 1.0])
            (dynamic label (properties/label-dynamic :light :color))
            (dynamic edit-type (g/constantly {:type types/Color
                                              :ignore-alpha true})))

  (property intensity g/Num (default 1.0)
            (dynamic label (properties/label-dynamic :light :intensity))
            (dynamic error (g/fnk [_node-id intensity] (validate-intensity _node-id intensity)))
            (dynamic edit-type (g/constantly {:type g/Num
                                              :min 0.0})))

  (output scene g/Any :cached
          (g/fnk [_node-id color intensity]
            (make-ambient-light-scene _node-id color intensity)))

  (output ambient-light-data g/Any :cached
          (g/fnk [color intensity]
            {"color" color
             "intensity" intensity}))

  (output ambient-light-build-errors g/Any :cached
          (g/fnk [_node-id intensity]
            (g/package-errors
              _node-id
              (validate-intensity _node-id intensity))))

  (output data g/Any (gu/passthrough ambient-light-data))
  (output own-build-errors g/Any (gu/passthrough ambient-light-build-errors))
  (output rt-tags g/Any (g/constantly ["light" "ambient_light"])))

(defn load-ambient-light [_project self _resource data-desc]
  {:pre [(map? data-desc)]} ; DataProto$Data in JSON map format.
  (let [data (:data data-desc)]
    (g/set-properties self
      :color (coll/resize (get data "color" [1.0 1.0 1.0]) 3 1.0)
      :intensity (get data "intensity"))))

;; -----------------------------------------------------------------------------
;; DirectionalLightNode
;; -----------------------------------------------------------------------------

(g/defnode DirectionalLightNode
  (inherits AmbientLightNode)

  (output scene g/Any :cached
          (g/fnk [_node-id color intensity]
            (make-directional-light-scene _node-id color intensity)))

  (output directional-light-data g/Any (gu/passthrough ambient-light-data))
  (output directional-light-build-errors g/Any (gu/passthrough ambient-light-build-errors))

  (output data g/Any (gu/passthrough directional-light-data))
  (output own-build-errors g/Any (gu/passthrough directional-light-build-errors))
  (output rt-tags g/Any (g/constantly ["light" "directional_light"])))

(defn load-directional-light [project self resource data-desc]
  (load-ambient-light project self resource data-desc))

;; -----------------------------------------------------------------------------
;; PointLightNode
;; -----------------------------------------------------------------------------

(defn- validate-range [node-id range]
  (validation/prop-error :fatal node-id :range validation/prop-negative? range range-message))

(g/defnode PointLightNode
  (inherits DirectionalLightNode)

  (property range g/Num (default 10.0)
            (dynamic label (properties/label-dynamic :light :range))
            (dynamic error (g/fnk [_node-id range] (validate-range _node-id range)))
            (dynamic edit-type (g/constantly {:type g/Num
                                              :min 0.0})))

  (output scene g/Any :cached
          (g/fnk [_node-id color intensity range]
            (make-point-light-scene _node-id color intensity range)))

  (output point-light-data g/Any :cached
          (g/fnk [directional-light-data range]
            (assoc directional-light-data "range" range)))

  (output point-light-build-errors g/Any :cached
          (g/fnk [_node-id directional-light-build-errors range]
            (g/package-errors
              _node-id
              directional-light-build-errors
              (validate-range _node-id range))))

  (output data g/Any (gu/passthrough point-light-data))
  (output own-build-errors g/Any (gu/passthrough point-light-build-errors))
  (output rt-tags g/Any (g/constantly ["light" "point_light"])))

(defn load-point-light [project self resource data-desc]
  {:pre [(map? data-desc)]} ; DataProto$Data in JSON map format.
  (let [data (:data data-desc)]
    (concat
      (load-directional-light project self resource data-desc)
      (g/set-property self :range (get data "range")))))

(defmethod scene-tools/manip-scalable? ::PointLightNode [_node-id] true)
(defmethod scene-tools/manip-scale-manips ::PointLightNode [_node-id] [:scale-uniform])

(defmethod scene-tools/manip-scale ::PointLightNode [node-id ^Vector3d delta manip-phase initial-evaluation-context]
  (let [old-range (g/node-value node-id :range initial-evaluation-context)
        new-range (properties/scale-by-absolute-value-and-round old-range (.getX delta))]
    (case manip-phase
      :manip-phase/commit
      {:manip/tx-data (g/set-property node-id :range new-range)}

      :manip-phase/preview
      {:manip/prop-kw->override-value {:range new-range}})))

;; -----------------------------------------------------------------------------
;; SpotLightNode
;; -----------------------------------------------------------------------------

(defn- validate-inner-cone-angle [node-id inner-cone-angle]
  (validation/prop-error :fatal node-id :inner-cone-angle validation/prop-negative? inner-cone-angle inner-cone-angle-message))

(defn- validate-outer-cone-angle [node-id outer-cone-angle]
  (validation/prop-error :fatal node-id :outer-cone-angle validation/prop-negative? outer-cone-angle outer-cone-angle-message))

(g/defnode SpotLightNode
  (inherits PointLightNode)

  (property inner-cone-angle g/Num (default 0.0)
            (dynamic label (properties/label-dynamic :light :inner-cone-angle))
            (dynamic error (g/fnk [_node-id inner-cone-angle] (validate-inner-cone-angle _node-id inner-cone-angle)))
            (dynamic edit-type (g/fnk [outer-cone-angle]
                                 {:type g/Num
                                  :min 0.0
                                  :max (clamp outer-cone-angle 0.0 max-spot-cone-angle)}))
            (set (fn [evaluation-context self _old-value new-value]
                   (when (some? new-value)
                     (let [outer-cone-angle (or (g/node-value self :outer-cone-angle evaluation-context) max-spot-cone-angle)
                           [inner-cone-angle _] (clamp-spot-cone-angles new-value outer-cone-angle)]
                       (g/set-property self :inner-cone-angle inner-cone-angle))))))

  (property outer-cone-angle g/Num (default 45.0)
            (dynamic label (properties/label-dynamic :light :outer-cone-angle))
            (dynamic error (g/fnk [_node-id outer-cone-angle] (validate-outer-cone-angle _node-id outer-cone-angle)))
            (dynamic edit-type (g/fnk [inner-cone-angle]
                                 {:type g/Num
                                  :min (clamp inner-cone-angle 0.0 max-spot-cone-angle)
                                  :max max-spot-cone-angle}))
            (set (fn [evaluation-context self _old-value new-value]
                   (when (some? new-value)
                     (let [inner-cone-angle (or (g/node-value self :inner-cone-angle evaluation-context) 0.0)
                           [inner-cone-angle outer-cone-angle] (clamp-spot-cone-angles inner-cone-angle new-value)]
                       (concat
                         (g/set-property self :inner-cone-angle inner-cone-angle)
                         (g/set-property self :outer-cone-angle outer-cone-angle)))))))

  (output scene g/Any :cached
          (g/fnk [_node-id color intensity range inner-cone-angle outer-cone-angle]
            (make-spot-light-scene _node-id color intensity range inner-cone-angle outer-cone-angle)))

  (output spot-light-data g/Any :cached
          (g/fnk [point-light-data inner-cone-angle outer-cone-angle]
            (assoc point-light-data
              "inner_cone_angle" inner-cone-angle
              "outer_cone_angle" outer-cone-angle)))

  (output spot-light-build-errors g/Any :cached
          (g/fnk [_node-id point-light-build-errors inner-cone-angle outer-cone-angle]
            (g/package-errors
              _node-id
              point-light-build-errors
              (validate-inner-cone-angle _node-id inner-cone-angle)
              (validate-outer-cone-angle _node-id outer-cone-angle))))

  (output data g/Any (gu/passthrough spot-light-data))
  (output own-build-errors g/Any (gu/passthrough spot-light-build-errors))
  (output rt-tags g/Any (g/constantly ["light" "spot_light"]))

  (output rt-data g/Any
          (g/fnk [spot-light-data]
            (-> spot-light-data
                (update "inner_cone_angle" math/deg->rad)
                (update "outer_cone_angle" math/deg->rad)))))

(defn load-spot-light [project self resource data-desc]
  {:pre [(map? data-desc)]} ; DataProto$Data in JSON map format.
  (let [data (:data data-desc)]
    (concat
      (load-point-light project self resource data-desc)
      (g/set-properties self
        :inner-cone-angle (get data "inner_cone_angle")
        :outer-cone-angle (get data "outer_cone_angle")))))

(defn register-resource-types [workspace]
  (let [common-args
        {:icon outline-icon
         :icon-class :design
         :category (localization/message "resource.category.lights")
         :view-types [:scene :text]
         :tags #{:component}
         :tag-opts {:component {:transform-properties #{}}}}

        args-per-type
        [{:ext "directional_light"
          :node-type DirectionalLightNode
          :load-fn load-directional-light
          :label (localization/message "resource.type.directional-light")}
         {:ext "point_light"
          :node-type PointLightNode
          :load-fn load-point-light
          :label (localization/message "resource.type.point-light")}
         {:ext "spot_light"
          :node-type SpotLightNode
          :load-fn load-spot-light
          :label (localization/message "resource.type.spot-light")}
         {:ext "ambient_light"
          :node-type AmbientLightNode
          :load-fn load-ambient-light
          :label (localization/message "resource.type.ambient-light")}]]

    (for [type-args args-per-type]
      (let [build-ext (str (:ext type-args) ".lightc")
            args (coll/merge common-args
                             type-args
                             {:build-ext build-ext})]
        (apply data/register-data-resource-type workspace (mapcat identity args))))))
