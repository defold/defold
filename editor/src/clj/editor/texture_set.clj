;; Copyright 2020-2022 The Defold Foundation
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
            [editor.gl.vertex :as vtx])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL2]
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
  [vert-index ^FloatBuffer tex-coords]
  (let [index (* vert-index 2)]
    [(.get tex-coords ^int index) (.get tex-coords ^int (inc index))]))

(defn- ->uv-quad
  [quad-index tex-coords tex-coord-order]
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
  [frame-index tex-coords tex-dims tex-coord-order frame-indices]
  (let [tex-coords-data (->uv-quad frame-index tex-coords tex-coord-order)
        {:keys [width height]} (->tex-dim frame-index tex-dims)]
    {:tex-coords tex-coords-data
     :width width
     :height height}))


(defn- ->anim-data
  [{:keys [start end fps flip-horizontal flip-vertical playback]} tex-coords tex-dims uv-transforms frame-indices]
  (let [tex-coord-order (tex-coord-lookup flip-horizontal flip-vertical)
        frames (mapv #(->anim-frame % tex-coords tex-dims tex-coord-order frame-indices) (range start end))]
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
        animations (:animations texture-set)
        frame-indices (:frame-indices texture-set)]
    (into {}
          (map #(vector (:id %) (->anim-data % tex-coords tex-dims uv-transforms frame-indices)))
          animations)))


;; vertex data

(defn- gen-vertex
  [^Matrix4d world-transform x y u v]
  (let [p (Point3d. x y 0.0)]
    (.transform world-transform p)
    (vector-of :double (.x p) (.y p) (.z p) 1.0 u v)))

(defn vertex-data
  [{:keys [width height tex-coords] :as frame} world-transform]
  (let [x1 (* 0.5 width)
        y1 (* 0.5 height)
        x0 (- x1)
        y0 (- y1)
        [[u0 v0] [u1 v1] [u2 v2] [u3 v3]] tex-coords]
    [(gen-vertex world-transform x0 y0 u0 v0)
     (gen-vertex world-transform x1 y0 u3 v3)
     (gen-vertex world-transform x0 y1 u1 v1)
     (gen-vertex world-transform x1 y0 u3 v3)
     (gen-vertex world-transform x1 y1 u2 v2)
     (gen-vertex world-transform x0 y1 u1 v1)]))


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

(defn- anim-data->vbuf
  [anim-data frame-index world-transform make-vbuf-fn]
  (let [vd (vertex-data (get-in anim-data [:frames frame-index]) world-transform)]
    (persistent! (reduce conj! (make-vbuf-fn (count vd)) vd))))

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
                vbuf (anim-data->vbuf anim-data frame world-transform make-vbuf-fn)]
            (when vbuf
              (let [vertex-binding (vtx/use-with ::animation vbuf shader)
                    gpu-texture (:gpu-texture user-data)
                    x0 (.x world-pos)
                    y0 (.y world-pos)
                    x1 (+ x0 (* (/ 1 sx) (:width anim-data)))
                    y1 (- y0 (* (/ 1 sy) (:height anim-data)) )
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
                  (shader/set-uniform shader gl "texture_sampler" 0)
                  (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (* n 6)))))))))))
