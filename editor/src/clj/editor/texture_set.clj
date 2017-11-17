(ns editor.texture-set
  (:require
   [dynamo.graph :as g]
   [editor.camera :as camera]
   [editor.colors :as colors]
   [editor.gl :as gl]
   [editor.gl.pass :as pass]
   [editor.gl.shader :as shader]
   [editor.gl.vertex :as vtx]
   [editor.scene :as scene]
   [editor.types :as types])
  (:import
   (java.nio ByteBuffer ByteOrder FloatBuffer)
   (javax.vecmath Point3d Vector3d Matrix4d)
   (com.google.protobuf ByteString)
   (com.jogamp.opengl GL2)))

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


;; anim data

(defn- ->anim-frame
  [frame-index tex-coords tex-coord-order dimensions]
  (let [tex-coords-data (->uv-quad frame-index tex-coords tex-coord-order)
        {:keys [width height]} (nth dimensions frame-index)]
    {:tex-coords tex-coords-data
     :width width
     :height height}))

(defn- ->anim-data
  [{:keys [start end fps flip-horizontal flip-vertical playback]} tex-coords uv-transforms dimensions]
  (let [tex-coord-order (tex-coord-lookup flip-horizontal flip-vertical)
        frames (mapv #(->anim-frame % tex-coords tex-coord-order dimensions) (range start end))]
    {:width (transduce (map :width) max 0 frames)
     :height (transduce (map :height) max 0 frames)
     :playback playback
     :fps fps
     :frames frames
     :uv-transforms (subvec uv-transforms start end)}))

(defn- decode-vertices
  [texture-set]
  (let [vs-buf (-> ^ByteString (:vertices texture-set)
                   (.asReadOnlyByteBuffer)
                   (.order ByteOrder/LITTLE_ENDIAN))]
    (loop [vs (transient [])]
      (if (.hasRemaining vs-buf)
        (recur (conj! vs [(.getFloat vs-buf)
                          (.getFloat vs-buf)
                          (.getFloat vs-buf)
                          (.getShort vs-buf)
                          (.getShort vs-buf)]))
        (persistent! vs)))))

(defn- frame-vertices
  [texture-set]
  (let [vs (decode-vertices texture-set)]
    (mapv (fn [start n]
            (mapv (fn [i] (nth vs i)) (range start (+ start n))))
          (:vertex-start texture-set)
          (:vertex-count texture-set))))

(defn- mind ^double [^double a ^double b] (Math/min a b))
(defn- maxd ^double [^double a ^double b] (Math/max a b))

(defn- dimension
  [vertices]
  (loop [[[x y :as v] & more] vertices
         x0 0.0, y0 0.0, x1 0.0, y1 0.0]
    (if v
      (recur more (mind x x0) (mind y y0) (maxd x x1) (maxd y y1))
      {:width (long (- x1 x0))
       :height (long (- y1 y0))})))

(defn- frame-dimensions
  [texture-set]
  (mapv dimension (frame-vertices texture-set)))

(defn make-anim-data
  [texture-set uv-transforms]
  (let [tex-coords (-> ^ByteString (:tex-coords texture-set)
                       (.asReadOnlyByteBuffer)
                       (.order ByteOrder/LITTLE_ENDIAN)
                       (.asFloatBuffer))
        animations (:animations texture-set)
        frame-dimensions (frame-dimensions texture-set)]
    (into {}
          (map #(vector (:id %) (->anim-data % tex-coords uv-transforms frame-dimensions)))
          animations)))


;; vertex data

(defn- gen-vertex
  [^Matrix4d world-transform x y u v]
  (let [p (Point3d. x y 0)]
    (.transform world-transform p)
    (vector-of :double (.x p) (.y p) (.z p) 1 u v)))

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
                    [cr cg cb ca] scene/outline-color
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
                (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
                  (shader/set-uniform shader gl "texture" 0)
                  (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (* n 6)))))))))))
