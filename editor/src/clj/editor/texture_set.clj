;; Copyright 2020-2024 The Defold Foundation
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
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL2]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteOrder FloatBuffer]
           [javax.vecmath Matrix4d Point3d Vector3d]))

(set! *warn-on-reflection* true)

(def ^:const tex-coord-orders
  [[0 1 2 3]   ; no flip
   [1 0 3 2]   ; flip v
   [3 2 1 0]   ; flip h
   [2 3 0 1]]) ; flip vh

(defn- tex-coord-lookup
  [flip-horizontal flip-vertical]
  (nth tex-coord-orders (bit-xor flip-vertical
                                 (bit-shift-left flip-horizontal 1))))

(defn- ->uv-vertex
  [^long vert-index ^FloatBuffer tex-coords]
  (let [index (int (* vert-index 2))]
    (vector-of :double (.get tex-coords index) (.get tex-coords (inc index)))))

(defn- ->uv-quad
  [^long quad-index tex-coords tex-coord-order]
  (let [offset (* quad-index 4)]
    (mapv #(->uv-vertex (+ offset %) tex-coords) tex-coord-order)))

(defn- ->tex-dim
  [frame-index ^FloatBuffer tex-dims]
  (let [offset (* frame-index 2)
        width (.get tex-dims ^int (+ offset 0))
        height (.get tex-dims ^int (+ offset 1))]
    {:width width :height height}))

;; anim data

(defn- ->anim-frame
  [frame-index page-index tex-coords tex-dims tex-coord-order]
  (let [tex-coords-data (->uv-quad frame-index tex-coords tex-coord-order)
        {:keys [width height]} (->tex-dim frame-index tex-dims)]
    {:page-index page-index
     :tex-coords tex-coords-data
     :width width
     :height height}))

(defn- flat-array->2d-points [flat-array]
  (into []
        (partition-all 2)
        flat-array))

(defn- ->anim-frame-from-geometry
  [page-index frame-geometry]
  {:page-index page-index
   :tex-coords (flat-array->2d-points (:uvs frame-geometry))
   :vertex-coords (flat-array->2d-points (:vertices frame-geometry))
   :indices (:indices frame-geometry)
   :use-geometries true
   :width (:width frame-geometry)
   :height (:height frame-geometry)})

(defn- ->anim-data
  [{:keys [start end fps flip-horizontal flip-vertical playback]} tex-coords tex-dims uv-transforms frame-indices page-indices geometries use-geometries]
  (let [frames (mapv (fn [i]
                       (let [frame-index (frame-indices i)
                             page-index (page-indices frame-index)
                             frame-geometry (get geometries frame-index)]
                         (if use-geometries
                           (->anim-frame-from-geometry page-index frame-geometry)
                           (->anim-frame frame-index page-index tex-coords tex-dims (tex-coord-lookup flip-horizontal flip-vertical)))))
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
          (map #(vector (:id %) (->anim-data % tex-coords tex-dims uv-transforms frame-indices page-indices geometries use-geometries)))
          animations)))


;; vertex data

(defn- gen-vertex
  [^Matrix4d world-transform x y u v page-index]
  (let [p (Point3d. x y 0.0)]
    (.transform world-transform p)
    (vector-of :double (.x p) (.y p) (.z p) 1.0 u v page-index)))

(defn- animation-frame-corners [animation-frame]
  (let [^double width (:width animation-frame)
        ^double height (:height animation-frame)
        x1 (* 0.5 width)
        y1 (* 0.5 height)
        x0 (- x1)
        y0 (- y1)
        xynw (vector-of :double x0 y0 0.0 1.0)
        xyne (vector-of :double x1 y0 0.0 1.0)
        xysw (vector-of :double x0 y1 0.0 1.0)
        xyse (vector-of :double x1 y1 0.0 1.0)]
    [xynw xyne xysw xyse]))

(defn position-data [animation-frame]
  (if (:use-geometries animation-frame)
    (let [^double width (:width animation-frame)
          ^double height (:height animation-frame)
          vertex-coords (:vertex-coords animation-frame)
          indices (:indices animation-frame)]
      (mapv (fn [i]
              (let [p (get vertex-coords i)
                    x (* width (get p 0))
                    y (* height (get p 1))]
                (vector-of :double x y 0.0 1.0)))
            indices))
    (let [corner-points (animation-frame-corners animation-frame)
          xynw (get corner-points 0)
          xyne (get corner-points 1)
          xysw (get corner-points 2)
          xyse (get corner-points 3)]
      [xynw xyne xysw xyne xyse xysw])))

(defn uv-data [animation-frame]
  (if (:use-geometries animation-frame)
    (let [tex-coords (:tex-coords animation-frame)
          indices (:indices animation-frame)]
      (mapv (fn [i] (get tex-coords i)) indices))
    (let [[uvnw uvsw uvse uvne] (:tex-coords animation-frame)]
      [uvnw uvne uvsw uvne uvse uvsw])))

(defn- line-data [animation-frame]
  (let [corner-points (animation-frame-corners animation-frame)
        xynw (get corner-points 0)
        xyne (get corner-points 1)
        xysw (get corner-points 2)
        xyse (get corner-points 3)]
    [xynw xyne xyne xyse xyse xysw xysw xynw]))

(defn vertex-data [animation-frame]
  {:position-data (position-data animation-frame)
   :uv-data (uv-data animation-frame)
   :line-data (line-data animation-frame)})


;; animation

(defn- next-frame
  [frame playback frame-time frame-count]
  (case playback
    :playback-none frame

    (:playback-once-forward :playback-loop-forward)
    (mod frame-time frame-count)

    (:playback-once-backward :playback-loop-backward)
    (- (dec frame-count) (mod frame-time frame-count))

    (:playback-once-pingpong :playback-loop-pingpong)
    ;; Unwrap to frame-count + (frame-count - 2)
    (let [pingpong-frame-count (max (- (* frame-count 2) 2) 1)
          next-frame (mod frame-time pingpong-frame-count)]
      (if (< next-frame frame-count)
        next-frame
        (- (dec frame-count) (- next-frame frame-count) 1)))))

(defn step-animation
  [{:keys [t frame] :as state} dt anim-data]
  (let [t' (+ t dt)
        time-per-frame (/ 1.0 (:fps anim-data))
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

(def ^:const animation-preview-offset 40)

(defn animation-frame->vertex-pos-uv
  [animation-frame world-transform]
  (let [frame-vertex-data (vertex-data animation-frame)
        page-index (:page-index animation-frame)]
    (mapv (fn [positions uvs]
            (let [x (get positions 0)
                  y (get positions 1)
                  u (get uvs 0)
                  v (get uvs 1)]
              (gen-vertex world-transform x y u v page-index)))
          (:position-data frame-vertex-data)
          (:uv-data frame-vertex-data))))

(defn- anim-data->vbuf
  [anim-data frame-index world-transform make-vbuf-fn]
  (let [animation-data (get-in anim-data [:frames frame-index])
        animation-vertices (animation-frame->vertex-pos-uv animation-data world-transform)
        ^VertexBuffer vbuf (make-vbuf-fn (count animation-vertices))
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
  [^GL2 gl render-args renderables n make-vbuf-fn shader]
  (let [{:keys [pass camera viewport]} render-args
        [sx sy sz] (camera/scale-factor camera viewport)
        scale-m (doto (Matrix4d.)
                  (.setIdentity)
                  (.setM00 (/ 1 sx))
                  (.setM11 (- (/ 1 sy))) ; flip
                  (.setM22 (/ 1 sz))
                  (.setM33 1))
        world-pos (Vector3d. animation-preview-offset (- (:bottom viewport) animation-preview-offset) 0)]
    (doseq [renderable renderables]
      (let [state (-> renderable :updatable :state)]
        (when-let [frame (:frame state)]
          (let [user-data (:user-data renderable)
                anim-data (:anim-data user-data)
                world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setTranslation (Vector3d. (+ (.x world-pos)  (* 0.5 (/ 1 sx) (:width anim-data)))
                                                              (- (.y world-pos) (* 0.5 (/ 1 sy) (:height anim-data)))
                                                              0))
                                  (.mul scale-m))
                vertex-count (anim-data->vertex-count anim-data frame)
                vbuf (anim-data->vbuf anim-data frame world-transform make-vbuf-fn)]
            (when vbuf
              (let [vertex-binding (vtx/use-with ::animation vbuf shader)
                    gpu-texture (:gpu-texture user-data)
                    x0 (.x world-pos)
                    y0 (.y world-pos)
                    x1 (+ x0 (* (/ 1 sx) (:width anim-data)))
                    y1 (- y0 (* (/ 1 sy) (:height anim-data)))
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
                (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
                  (shader/set-samplers-by-index shader gl 0 (:texture-units gpu-texture))
                  (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 vertex-count))))))))))