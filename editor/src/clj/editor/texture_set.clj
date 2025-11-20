;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.texture-set
  (:require [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.shaders :as shaders]
            [editor.slice9 :as slice9]
            [util.coll :refer [pair]])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL2]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteOrder FloatBuffer]
           [javax.vecmath Matrix4d Point3d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private flip-strategy->tex-coord-order
  [(vector-of :long 0 1 2 3)   ; no flip
   (vector-of :long 1 0 3 2)   ; flip v
   (vector-of :long 3 2 1 0)   ; flip h
   (vector-of :long 2 3 0 1)]) ; flip vh

(def ^:private flip-strategy->scale-factors
  [(vector-of :double 1.0 1.0)     ; no flip
   (vector-of :double 1.0 -1.0)    ; flip v
   (vector-of :double -1.0 1.0)    ; flip h
   (vector-of :double -1.0 -1.0)]) ; flip vh

(defn- ->uv-vertex
  [^long vert-index ^FloatBuffer tex-coords]
  (let [index (int (* vert-index 2))]
    (vector-of :double (.get tex-coords index) (.get tex-coords (inc index)))))

(defn- ->quad-tex-coords
  [^long quad-index tex-coords tex-coord-order]
  (let [offset (* quad-index 4)]
    (mapv (fn [^long tex-coord-index]
            (->uv-vertex (+ offset tex-coord-index) tex-coords))
          tex-coord-order)))

(defn- ->tex-dim
  [^long frame-index ^FloatBuffer tex-dims]
  (let [offset (int (* frame-index 2))
        width (.get tex-dims offset)
        height (.get tex-dims (inc offset))]
    {:width width :height height}))

;; anim data

(defn- ->anim-frame
  [page-index quad-tex-coords tex-dim frame-geometry]
  {:page-index page-index
   :tex-coords quad-tex-coords
   :width (:width tex-dim)
   :height (:height tex-dim)
   :pivot [(or (:pivot-x frame-geometry) 0.0) (or (:pivot-y frame-geometry) 0.0)]})

(defn- double-vector->2d-points
  ([double-vector reverse]
   (double-vector->2d-points double-vector reverse 1.0 1.0))
  ([double-vector reverse ^double scale-x ^double scale-y]
   (mapv (fn [^long index]
           (let [^double x (double-vector index)
                 ^double y (double-vector (inc index))]
             (vector-of :double (* x scale-x) (* y scale-y))))
         (if reverse
           (range (- (count double-vector) 2) -2 -2)
           (range 0 (count double-vector) 2)))))

(defn- ->anim-frame-from-geometry
  [page-index quad-tex-coords frame-geometry scale-factors reverse]
  (let [^double scale-x (scale-factors 0)
        ^double scale-y (scale-factors 1)
        ^double pivot-x (or (:pivot-x frame-geometry) 0.0)
        ^double pivot-y (or (:pivot-y frame-geometry) 0.0)
        vertex-coords (double-vector->2d-points (:vertices frame-geometry) reverse scale-x scale-y)
        vertex-tex-coords (double-vector->2d-points (:uvs frame-geometry) reverse)]
    {:page-index page-index
     :tex-coords quad-tex-coords
     :vertex-coords vertex-coords
     :pivot [pivot-x pivot-y]
     :vertex-tex-coords vertex-tex-coords
     :indices (:indices frame-geometry)
     :use-geometries true
     :width (:width frame-geometry)
     :height (:height frame-geometry)}))

(defn- ->anim-data
  [{:keys [start end fps flip-horizontal flip-vertical playback]} tex-coords tex-dims uv-transforms frame-indices page-indices geometries use-geometries]
  {:pre [(contains? #{nil 0 1} flip-horizontal)
         (contains? #{nil 0 1} flip-vertical)]}
  (let [^long flip-horizontal (or flip-horizontal 0)
        ^long flip-vertical (or flip-vertical 0)
        flip-strategy (bit-or flip-vertical (bit-shift-left flip-horizontal 1))
        tex-coord-order (flip-strategy->tex-coord-order flip-strategy)
        scale-factors (flip-strategy->scale-factors flip-strategy)
        reverse (not (zero? (bit-xor flip-horizontal flip-vertical)))
        frames (mapv (fn [i]
                       (let [frame-index (frame-indices i)
                             page-index (page-indices frame-index)
                             frame-geometry (geometries frame-index)
                             quad-tex-coords (->quad-tex-coords frame-index tex-coords tex-coord-order)]
                         (if (and use-geometries
                                  (not= :sprite-trim-mode-off
                                        (:trim-mode frame-geometry)))
                           (->anim-frame-from-geometry page-index quad-tex-coords frame-geometry scale-factors reverse)
                           (->anim-frame page-index quad-tex-coords (->tex-dim frame-index tex-dims) frame-geometry))))
                     (range start end))]
    {:width (transduce (map :width) max 0 frames)
     :height (transduce (map :height) max 0 frames)
     :playback playback
     :fps fps
     :frames frames
     :uv-transforms (subvec uv-transforms start end)}))

(defn make-anim-data
  [texture-set uv-transforms]
  (let [tex-coords (-> ^ByteString (:tex-coords texture-set)
                       (.asReadOnlyByteBuffer)
                       (.order ByteOrder/LITTLE_ENDIAN)
                       (.asFloatBuffer))
        tex-dims (-> ^ByteString (:tex-dims texture-set)
                     (.asReadOnlyByteBuffer)
                     (.order ByteOrder/LITTLE_ENDIAN)
                     (.asFloatBuffer))

        use-geometries (= (:use-geometries texture-set) 1)
        geometries (:geometries texture-set)
        animations (:animations texture-set)
        frame-indices (:frame-indices texture-set)
        page-indices (:page-indices texture-set)]
    (into {}
          (map #(pair (:id %) (->anim-data % tex-coords tex-dims uv-transforms frame-indices page-indices geometries use-geometries)))
          animations)))


;; vertex data

(def ^:private animation-overlay-shader shaders/basic-texture-paged-local-space)

(defn- gen-vertex
  [^Matrix4d world-transform x y u v page-index]
  (let [p (Point3d. x y 0.0)]
    (.transform world-transform p)
    (vector-of :double (.x p) (.y p) (.z p) u v page-index)))

(defn- corner-points [[^double width ^double height :as size] pivot]
  (let [[^double offset-x ^double offset-y] (geom/gui-pivot-offset pivot size)
        half-width (* 0.5 width)
        half-height (* 0.5 height)
        x0 (+ (- half-width) offset-x)
        y0 (+ (- half-height) offset-y)
        x1 (+ half-width offset-x)
        y1 (+ half-height offset-y)
        xynw (vector-of :double x0 y0 0.0 1.0)
        xyne (vector-of :double x1 y0 0.0 1.0)
        xysw (vector-of :double x0 y1 0.0 1.0)
        xyse (vector-of :double x1 y1 0.0 1.0)]
    [xynw xyne xysw xyse]))

(defn- corner-points->position-data [[xynw xyne xysw xyse]]
  [xynw xyne xysw xyne xyse xysw])

(defn- corner-points->line-data [[xynw xyne xysw xyse]]
  [xynw xyne xyne xyse xyse xysw xysw xynw])

(defn- offset-vertices [^double offset-x ^double offset-y vertices]
  ; Vertices is an array with arrays: [[x0 y0 u0 v0 ...] [x1 y1 u1 v1 ...] ...]
  (mapv (fn [vtx]
          (let [^double px (first vtx)
                ^double py (second vtx)
                x (- px offset-x)
                y (- py offset-y)]
            (assoc vtx 0 x 1 y)))
        vertices))

(defn- frame-vertex-data [animation-frame size pivot]
  (let [use-geometries (:use-geometries animation-frame)
        corner-points (corner-points size pivot)
        line-data (corner-points->line-data corner-points)

        position-data
        (if use-geometries
          (let [[^double width ^double height] size
                [^double offset-x ^double offset-y] (geom/gui-pivot-offset pivot size)
                vertex-coords (:vertex-coords animation-frame)
                indices (:indices animation-frame)]
            (mapv (fn [i]
                    (let [[^double px ^double py] (vertex-coords i)
                          x (+ (* width px) offset-x)
                          y (+ (* height py) offset-y)]
                      (vector-of :double x y 0.0 1.0)))
                  indices))
          (corner-points->position-data corner-points))

        uv-data
        (if use-geometries
          (let [tex-coords (:vertex-tex-coords animation-frame)
                indices (:indices animation-frame)]
            (mapv tex-coords indices))
          (let [[uvnw uvsw uvse uvne] (:tex-coords animation-frame)]
            [uvnw uvne uvsw uvne uvse uvsw]))]

    {:position-data position-data
     :uv-data uv-data
     :line-data line-data}))

(def ^:private default-quad-uv-data
  [(vector-of :double 0.0 0.0)
   (vector-of :double 1.0 0.0)
   (vector-of :double 0.0 1.0)
   (vector-of :double 1.0 0.0)
   (vector-of :double 1.0 1.0)
   (vector-of :double 0.0 1.0)])

(defn- quad-vertex-data [size pivot]
  (let [corner-points (corner-points size pivot)
        position-data (corner-points->position-data corner-points)
        line-data (corner-points->line-data corner-points)]
    {:position-data position-data
     :uv-data default-quad-uv-data
     :line-data line-data}))

(defn vertex-data [animation-frame size-mode size slice9 pivot]
  (let [out (-> (cond
                  (nil? animation-frame)
                  (quad-vertex-data size pivot)

                  (and (= :size-mode-manual size-mode)
                       (slice9/sliced? slice9))
                  (slice9/vertex-data animation-frame size slice9 pivot)

                  :else
                  (frame-vertex-data animation-frame size pivot))
                (assoc :page-index (:page-index animation-frame 0)))

        ; Pivot point comes from the SpriteGeometry, where (0,0) is center of the image and +Y is up.
        [^double image-pivot-x ^double image-pivot-y] (or (:pivot animation-frame) [0.0 0.0])
        [^double width ^double height] (if (or (and (= :size-mode-manual size-mode)) (nil? animation-frame)) size [(:width animation-frame) (:height animation-frame)])
        image-pivot-x (* width image-pivot-x)
        image-pivot-y (* height image-pivot-y)

        position-data (:position-data out)
        line-data (:line-data out)
        offset-positions (offset-vertices image-pivot-x image-pivot-y position-data)
        offset-lines (offset-vertices image-pivot-x image-pivot-y line-data)]
    (assoc out :position-data offset-positions :line-data offset-lines)))


;; animation

(defn- next-frame
  [frame playback frame-time ^long frame-count]
  (case playback
    :playback-none frame

    (:playback-once-forward :playback-loop-forward)
    (mod frame-time frame-count)

    (:playback-once-backward :playback-loop-backward)
    (- (dec frame-count) (long (mod frame-time frame-count)))

    (:playback-once-pingpong :playback-loop-pingpong)
    ;; Unwrap to frame-count + (frame-count - 2)
    (let [pingpong-frame-count (max (- (* frame-count 2) 2) 1)
          next-frame (long (mod frame-time pingpong-frame-count))]
      (if (< next-frame frame-count)
        next-frame
        (- (dec frame-count) (- next-frame frame-count) 1)))))

(defn step-animation
  [{:keys [^double t frame] :as state} ^double dt anim-data]
  (let [t' (+ t dt)
        time-per-frame (/ 1.0 (double (:fps anim-data)))
        frame-time (long (+ 0.5 (/ t' time-per-frame)))
        frame' (next-frame frame (:playback anim-data) frame-time (count (:frames anim-data)))]
    (assoc state :t t' :frame frame')))

(defn make-animation-updatable
  [node-id name anim-data]
  (when (seq anim-data)
    {:node-id       node-id
     :name          name
     :update-fn     (fn [state {:keys [dt]}]
                      (step-animation state dt anim-data))
     :initial-state {:t         0
                     :frame     0}}))


;; rendering

(def ^:const animation-preview-offset 40.0)

(defn- animation-frame->vertex-floats
  [animation-frame world-transform]
  (let [size [(:width animation-frame)
              (:height animation-frame)]
        frame-vertex-data (frame-vertex-data animation-frame size :pivot-center)
        page-index (:page-index animation-frame 0)]
    (mapv (fn [positions uvs]
            (let [x (get positions 0)
                  y (get positions 1)
                  u (get uvs 0)
                  v (get uvs 1)]
              (gen-vertex world-transform x y u v page-index)))
          (:position-data frame-vertex-data)
          (:uv-data frame-vertex-data))))

(defn- anim-data->vbuf
  [anim-data frame-index world-transform]
  (let [animation-data (get-in anim-data [:frames frame-index])
        animation-vertices (animation-frame->vertex-floats animation-data world-transform)
        vertex-description (shaders/vertex-description animation-overlay-shader)
        ^VertexBuffer vbuf (vtx/make-vertex-buffer vertex-description :stream (count animation-vertices))
        ^ByteBuffer buf (.buf vbuf)]
    (doseq [vertex animation-vertices]
      (vtx/buf-push-floats! buf vertex))
    (vtx/flip! vbuf)))

(defn- anim-data->vertex-count
  [anim-data frame-index]
  (let [anim-frame (get-in anim-data [:frames frame-index])]
    (if (:use-geometries anim-frame)
      (count (:indices anim-frame))
      6)))

(defn render-animation-overlay
  [^GL2 gl render-args renderables]
  (let [{:keys [camera viewport]} render-args
        [^double sx ^double sy ^double sz] (camera/scale-factor camera viewport)
        scale-m (doto (Matrix4d.)
                  (.setIdentity)
                  (.setM00 (/ 1.0 sx))
                  (.setM11 (- (/ 1.0 sy))) ; flip
                  (.setM22 (/ 1.0 sz))
                  (.setM33 1.0))
        world-pos (Vector3d. animation-preview-offset (- (double (:bottom viewport)) animation-preview-offset) 0.0)]
    (doseq [renderable renderables]
      (let [state (-> renderable :updatable :state)]
        (when-let [frame (:frame state)]
          (let [user-data (:user-data renderable)
                anim-data (:anim-data user-data)
                image-width (double (:width anim-data))
                image-height (double (:height anim-data))
                world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setTranslation (Vector3d. (+ (.x world-pos) (* 0.5 (/ 1.0 sx) image-width))
                                                              (- (.y world-pos) (* 0.5 (/ 1.0 sy) image-height))
                                                              0.0))
                                  (.mul scale-m))
                vertex-count (anim-data->vertex-count anim-data frame)
                vbuf (anim-data->vbuf anim-data frame world-transform)]
            (when vbuf
              (let [vertex-binding (vtx/use-with ::animation vbuf animation-overlay-shader)
                    gpu-texture (:gpu-texture user-data)
                    x0 (.x world-pos)
                    y0 (.y world-pos)
                    x1 (+ x0 (* (/ 1.0 sx) image-width))
                    y1 (- y0 (* (/ 1.0 sy) image-height))
                    [cr cg cb ca] colors/outline-color
                    [xr xg xb xa] colors/scene-background]
                (.glColor4d gl xr xg xb xa)
                (.glBegin gl GL2/GL_QUADS)
                (.glVertex3d gl x0 y0 0)
                (.glVertex3d gl x0 y1 0)
                (.glVertex3d gl x1 y1 0)
                (.glVertex3d gl x1 y0 0)
                (.glEnd gl)
                (.glColor4d gl cr cg cb ca)
                (.glBegin gl GL2/GL_LINE_LOOP)
                (.glVertex3d gl x0 y0 0)
                (.glVertex3d gl x0 y1 0)
                (.glVertex3d gl x1 y1 0)
                (.glVertex3d gl x1 y0 0)
                (.glEnd gl)
                (gl/with-gl-bindings gl render-args [animation-overlay-shader vertex-binding gpu-texture]
                  (shader/set-samplers-by-index animation-overlay-shader gl 0 (:texture-units gpu-texture))
                  (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 vertex-count))))))))))
