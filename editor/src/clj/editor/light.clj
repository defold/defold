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
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.image-util :as image-util]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.scene-shapes :as scene-shapes]
            [editor.scene-tools :as scene-tools]
            [editor.shaders :as shaders]
            [editor.types :as types]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto DataProto$Data]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix3d Matrix4d Point3d Quat4d Vector3d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(vtx/defvertex tex-color-vtx
  (vec3 position)
  (vec2 texcoord0)
  (vec4 color))

(def ^:private outline-shader shaders/basic-color-world-space)

(def ^:private light-icon-shader shaders/basic-texture-color-world-space)

(def ^:private ^:const billboard-circle-segments 32)
(def ^:private ^:const gizmo-target-pixels 100.0)
;; Screen-space half-extent (pixels) for the origin icon quad; world size = scale-factor * this (see camera preview mesh).
(def ^:private ^:const origin-marker-pixels 8.0)

(def ^:private outline-icon "icons/64/Icons_21-Light.png")
(def ^:private scene-icon "icons/scene/light_omni.png")

(def ^:private light-icon-gpu-texture
  (delay
    (texture/image-texture
      ::light-scene-view-icon
      (image-util/read-image (io/resource scene-icon))
      (merge texture/default-image-texture-params
             {:min-filter gl/linear
              :mag-filter gl/linear}))))

;; Property inspector and form choicebox: [value label] pairs (see editor.properties-view/make-control-view :choicebox).
(def ^:private light-type-options
  [[:point "Point"]
   [:directional "Directional"]
   [:spot "Spot"]])

(defn- list-field-vec4 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- list-field-vec3 [v]
  {:list {:values (mapv (fn [x] {:number (double x)}) v)}})

(defn- field-num [n]
  {:number (double n)})

(defn- struct-fields [data-map]
  (let [raw (get-in data-map [:data :struct :fields])]
    (into {}
          (map (fn [[k v]]
                 [(if (keyword? k) (name k) k) v]))
          raw)))

(defn- tags->light-type [tags]
  (cond
    (some #{"directional_light"} tags) :directional
    (some #{"spot_light"} tags) :spot
    :else :point))

(defn- light-type->tags [light-type]
  (case light-type
    :directional ["light" "directional_light"]
    :spot ["light" "spot_light"]
    ["light" "point_light"]))

(defn- get-number [fields key default]
  (double (or (get-in fields [key :number]) default)))

(defn- get-vec4 [fields key]
  (let [vals (get-in fields [key :list :values])]
    (if (seq vals)
      (mapv #(double (or (:number %) 0.0)) vals)
      [1.0 1.0 1.0 1.0])))

(defn- get-vec3 [fields key]
  (let [vals (get-in fields [key :list :values])]
    (if (seq vals)
      (mapv #(double (or (:number %) 0.0)) vals)
      [0.0 0.0 -1.0])))

(defn- parse-data-desc [light-desc]
  ;; GameObject$Data in map format.
  (let [tags (vec (:tags light-desc))
        light-type (tags->light-type tags)
        fields (struct-fields light-desc)]
    {:light-type light-type
     :color (get-vec4 fields "color")
     :intensity (get-number fields "intensity" 1.0)
     :range (get-number fields "range" 10.0)
     :direction (get-vec3 fields "direction")
     :inner-cone-angle (get-number fields "inner_cone_angle" 0.0)
     :outer-cone-angle (get-number fields "outer_cone_angle" 45.0)}))

(defn- build-data-desc
  [light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (let [tags (light-type->tags light-type)
        c4 (vec (take 4 (concat color (repeat 1.0))))
        fields (case light-type
                 :point {"color" (list-field-vec4 c4)
                         "intensity" (field-num intensity)
                         "range" (field-num range)}
                 :directional {"color" (list-field-vec4 c4)
                               "intensity" (field-num intensity)
                               "direction" (list-field-vec3 (vec (take 3 (concat direction [0.0 0.0 -1.0]))))}
                 :spot {"color" (list-field-vec4 c4)
                        "intensity" (field-num intensity)
                        "range" (field-num range)
                        "inner_cone_angle" (field-num inner-cone-angle)
                        "outer_cone_angle" (field-num outer-cone-angle)})]
    (protobuf/make-map-without-defaults DataProto$Data
      :tags tags
      :data {:struct {:fields fields}})))

(defn- build-light [build-resource _dep-resources user-data]
  (let [{:keys [pb-map]} user-data]
    {:resource build-resource
     :content (protobuf/map->bytes DataProto$Data pb-map)}))

(g/defnk produce-build-targets [_node-id resource save-value]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-light
      :user-data {:pb-map save-value}})])

(g/defnk produce-save-value
  [light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (build-data-desc light-type color intensity range direction inner-cone-angle outer-cone-angle))

(g/defnk produce-outline-data [_node-id]
  {:node-id _node-id
   :node-outline-key "Light"
   :label (localization/message "resource.type.light")
   :icon outline-icon})

(defn- unify-scale [renderable]
  (let [{:keys [^Quat4d world-rotation
                ^Vector3d world-scale
                ^Vector3d world-translation]} renderable
        min-scale (min (.-x world-scale) (.-y world-scale) (.-z world-scale))
        physics-world-transform (doto (Matrix4d.)
                                (.setIdentity)
                                (.setScale min-scale)
                                (.setTranslation world-translation)
                                (.setRotation world-rotation))]
    (assoc renderable :world-transform physics-world-transform)))

(defn- wrap-uniform-scale [render-fn]
  (fn [gl render-args renderables n]
    (render-fn gl render-args (map unify-scale renderables) n)))

(def ^:private render-light-lines-base (wrap-uniform-scale scene-shapes/render-lines))

(defn- render-light-gizmo-lines [^GL2 gl render-args renderables n]
  (gl/gl-enable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl false)
  (try
    (render-light-lines-base gl render-args renderables n)
    (finally
      (gl/gl-disable gl GL/GL_DEPTH_TEST))))

(defn- renderable-min-scale [renderable]
  (if-some [^Vector3d ws (:world-scale renderable)]
    (min (.-x ws) (.-y ws) (.-z ws))
    1.0))

(defn- v3-normalize! [^Vector3d v ^Vector3d fallback]
  (let [len (Math/sqrt (+ (* (.x v) (.x v)) (* (.y v) (.y v)) (* (.z v) (.z v))))]
    (if (> len 1e-10)
      (doto v (.scale (/ 1.0 len)))
      (.set v fallback))))

(defn- world-dir-from-light [renderable local-dir]
  (let [d (Vector3d. (double (local-dir 0)) (double (local-dir 1)) (double (local-dir 2)))
        v (math/transform-vector (:world-transform renderable) d)]
    (v3-normalize! v (Vector3d. 0.0 0.0 -1.0))))

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

(defn- conj-line-vertex!
  ([vbuf x y z cr cg cb]
   (conj-line-vertex! vbuf x y z cr cg cb 1.0))
  ([vbuf x y z cr cg cb ca]
   (conj! vbuf [(double x) (double y) (double z) (double cr) (double cg) (double cb) (double ca)])))

(defn- fill-light-icon-quad!
  [vbuf ^Vector3d c ^Vector3d right ^Vector3d up h cr cg cb]
  (let [h (double h)
        cr (double cr)
        cg (double cg)
        cb (double cb)
        ca 1.0
        rx (* h (.x right))
        ry (* h (.y right))
        rz (* h (.z right))
        ux (* h (.x up))
        uy (* h (.y up))
        uz (* h (.z up))
        cx (.x c)
        cy (.y c)
        cz (.z c)
        blx (- cx rx ux)
        bly (- cy ry uy)
        blz (- cz rz uz)
        tlx (+ (- cx rx) ux)
        tly (+ (- cy ry) uy)
        tlz (+ (- cz rz) uz)
        trx (+ cx rx ux)
        try_ (+ cy ry uy)
        trz (+ cz rz uz)
        brx (- (+ cx rx) ux)
        bry (- (+ cy ry) uy)
        brz (- (+ cz rz) uz)]
    (-> vbuf
        (conj! [blx bly blz 0.0 0.0 cr cg cb ca])
        (conj! [tlx tly tlz 0.0 1.0 cr cg cb ca])
        (conj! [trx try_ trz 1.0 1.0 cr cg cb ca])
        (conj! [trx try_ trz 1.0 1.0 cr cg cb ca])
        (conj! [brx bry brz 1.0 0.0 cr cg cb ca])
        (conj! [blx bly blz 0.0 0.0 cr cg cb ca]))))

(defn- fill-solid-billboard-quad!
  [vbuf ^Vector3d c ^Vector3d right ^Vector3d up h cr cg cb ca]
  (let [h (double h)
        cr (double cr)
        cg (double cg)
        cb (double cb)
        ca (double ca)
        rx (* h (.x right))
        ry (* h (.y right))
        rz (* h (.z right))
        ux (* h (.x up))
        uy (* h (.y up))
        uz (* h (.z up))
        cx (.x c)
        cy (.y c)
        cz (.z c)
        blx (- cx rx ux)
        bly (- cy ry uy)
        blz (- cz rz uz)
        tlx (+ (- cx rx) ux)
        tly (+ (- cy ry) uy)
        tlz (+ (- cz rz) uz)
        trx (+ cx rx ux)
        try_ (+ cy ry uy)
        trz (+ cz rz uz)
        brx (- (+ cx rx) ux)
        bry (- (+ cy ry) uy)
        brz (- (+ cz rz) uz)]
    (-> vbuf
        (conj-line-vertex! blx bly blz cr cg cb ca)
        (conj-line-vertex! tlx tly tlz cr cg cb ca)
        (conj-line-vertex! trx try_ trz cr cg cb ca)
        (conj-line-vertex! trx try_ trz cr cg cb ca)
        (conj-line-vertex! brx bry brz cr cg cb ca)
        (conj-line-vertex! blx bly blz cr cg cb ca))))

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

;; Translation gizmo arrow is modeled on +X; map local +X to world light direction (column 0 = dir).
(defn- mat3-x-axis-to-dir ^Matrix3d [^Vector3d dir]
  (let [x (Vector3d. (.x dir) (.y dir) (.z dir))
        len (Math/sqrt (+ (* (.x x) (.x x)) (* (.y x) (.y x)) (* (.z x) (.z x))))]
    (if (< len 1e-10)
      (doto (Matrix3d.) (.setIdentity))
      (do
        (.scale x (/ 1.0 len))
        (let [ref (if (< (Math/abs (.x x)) 0.9)
                    (Vector3d. 1.0 0.0 0.0)
                    (Vector3d. 0.0 1.0 0.0))
              y (doto (Vector3d.) (.cross ref x))
              ylen (Math/sqrt (+ (* (.x y) (.x y)) (* (.y y) (.y y)) (* (.z y) (.z y))))]
          (if (< ylen 1e-8)
            (doto (Matrix3d.) (.setIdentity))
            (let [y (doto y (.scale (/ 1.0 ylen)))
                  z (doto (Vector3d.) (.cross x y))]
              (.normalize z)
              (doto (Matrix3d.)
                (.setColumn 0 (.x x) (.y x) (.z x))
                (.setColumn 1 (.x y) (.y y) (.z y))
                (.setColumn 2 (.x z) (.y z) (.z z))))))))))

(defn- transform-local-arrow-point ^Vector3d [^Matrix3d R ^Vector3d p s lx ly lz]
  ;; No ^double arg hints: primitive fns are limited to 4 parameters in Clojure.
  (let [s (double s)
        lx (double lx)
        ly (double ly)
        lz (double lz)
        tv (Vector3d. (* s lx) (* s ly) (* s lz))]
    (.transform R tv)
    (.add tv p)
    tv))

(defn- fill-directional-move-arrow! [vbuf-tris vbuf-lines ^Vector3d p ^Vector3d d-world cr cg cb total-len]
  ;; No ^double on total-len: any primitive param hint limits fns to 4 args in Clojure; this has 8 params.
  (let [^Matrix3d R (mat3-x-axis-to-dir d-world)
        s (/ (double total-len) 100.0)
        cr (double cr)
        cg (double cg)
        cb (double cb)]
    (doseq [[mode vs] directional-arrow-vertex-groups]
      (doseq [v vs]
        (let [^Vector3d w (transform-local-arrow-point R p s (double (nth v 0)) (double (nth v 1)) (double (nth v 2)))]
          (if (= mode GL/GL_TRIANGLES)
            (conj-line-vertex! vbuf-tris (.x w) (.y w) (.z w) cr cg cb)
            (conj-line-vertex! vbuf-lines (.x w) (.y w) (.z w) cr cg cb)))))))

(defn- fill-point-billboard-circle! [vbuf ^Vector3d center ^Vector3d right ^Vector3d up radius cr cg cb]
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
                    x0 (+ (.x center) (* radius (+ (* (.x right) cos0) (* (.x up) sin0))))
                    y0 (+ (.y center) (* radius (+ (* (.y right) cos0) (* (.y up) sin0))))
                    z0 (+ (.z center) (* radius (+ (* (.z right) cos0) (* (.z up) sin0))))
                    x1 (+ (.x center) (* radius (+ (* (.x right) cos1) (* (.x up) sin1))))
                    y1 (+ (.y center) (* radius (+ (* (.y right) cos1) (* (.y up) sin1))))
                    z1 (+ (.z center) (* radius (+ (* (.z right) cos1) (* (.z up) sin1))))]
                (-> vbuf
                    (conj-line-vertex! x0 y0 z0 cr cg cb)
                    (conj-line-vertex! x1 y1 z1 cr cg cb))))
            vbuf
            (range segments))))

(defn- outline-rgb-for-light [renderable _base-color]
  (if-let [sel (:selected renderable)]
    (let [c (colors/selection-color sel)]
      [(nth c 0) (nth c 1) (nth c 2)])
    [(nth colors/outline-color 0) (nth colors/outline-color 1) (nth colors/outline-color 2)]))

(defn- light-gizmo-selected? [renderable]
  (#{:self-selected :parent-selected} (:selected renderable)))

(defn- light-rgb [user-data]
  (or (:light-rgb user-data)
      (let [c (:color user-data)]
        [(double (nth c 0 1.0)) (double (nth c 1 1.0)) (double (nth c 2 1.0))])))

(defn- render-origin-markers-billboard [^GL2 gl render-args renderables n]
  (assert (= pass/outline (:pass render-args)))
  (let [n (long n)
        camera (:camera render-args)
        vbuf (persistent!
               (reduce (fn [vbuf ri]
                         (let [renderable (nth renderables ri)
                               [ocr ocg ocb] (light-rgb (:user-data renderable))
                               ^Vector3d world-translation (:world-translation renderable)
                               sf (scene-tools/scale-factor camera (:viewport render-args) world-translation)
                               h (* 2.0 (double sf) (double origin-marker-pixels))]
                           (if-some [axes (billboard-axes world-translation camera)]
                             (let [[^Vector3d right ^Vector3d up _] axes]
                               (fill-light-icon-quad! vbuf world-translation right up h ocr ocg ocb))
                             vbuf)))
                       (->tex-color-vtx (* n 6))
                       (range n)))]
    (when (pos? (count vbuf))
      (let [vb (vtx/use-with ::light-origin-icon vbuf light-icon-shader)
            gpu-tex @light-icon-gpu-texture]
        (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)
        (gl/gl-enable gl GL/GL_BLEND)
        (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
        (try
          (gl/with-gl-bindings gl render-args [light-icon-shader vb gpu-tex]
            (shader/set-samplers-by-index light-icon-shader gl 0 (:texture-units gpu-tex))
            (.glDrawArrays gl GL/GL_TRIANGLES 0 (count vbuf)))
          (finally
            (gl/gl-disable gl GL/GL_BLEND)
            (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_LINE)))))))

(defn- render-origin-markers-selection [^GL2 gl render-args renderables n]
  (assert (= pass/selection (:pass render-args)))
  (let [n (long n)
        camera (:camera render-args)
        vbuf (persistent!
               (reduce (fn [vbuf ri]
                         (let [renderable (nth renderables ri)
                               ^floats pc (scene-picking/picking-id->float-array (long (:picking-id renderable)))
                               cr (aget pc 0)
                               cg (aget pc 1)
                               cb (aget pc 2)
                               ca (aget pc 3)
                               ^Vector3d world-translation (:world-translation renderable)
                               sf (scene-tools/scale-factor camera (:viewport render-args) world-translation)
                               h (* 2.0 (double sf) (double origin-marker-pixels))]
                           (if-some [axes (billboard-axes world-translation camera)]
                             (let [[^Vector3d right ^Vector3d up _] axes]
                               (fill-solid-billboard-quad! vbuf world-translation right up h cr cg cb ca))
                             vbuf)))
                       (->color-vtx (* n 6))
                       (range n)))]
    (when (pos? (count vbuf))
      (let [vb (vtx/use-with ::light-origin-selection-quad vbuf outline-shader)]
        (gl/with-gl-bindings gl render-args [outline-shader vb]
          (.glDrawArrays gl GL/GL_TRIANGLES 0 (count vbuf)))))))

(defn- render-point-outline [^GL2 gl render-args renderables n]
  (assert (= pass/outline (:pass render-args)))
  (let [n (long n)
        camera (:camera render-args)
        vbuf (persistent!
               (reduce (fn [vbuf ri]
                         (let [renderable (nth renderables ri)]
                          (if (light-gizmo-selected? renderable)
                             (let [{:keys [color range]} (:user-data renderable)
                                   [cr cg cb] (outline-rgb-for-light renderable color)
                                   ^Vector3d world-translation (:world-translation renderable)
                                   r (* (double (or range 1.0)) (double (renderable-min-scale renderable)))
                                   r (max r 0.01)]
                               (if-some [axes (billboard-axes world-translation camera)]
                                 (let [[^Vector3d right ^Vector3d up _] axes]
                                   (fill-point-billboard-circle! vbuf world-translation right up r cr cg cb))
                                 vbuf))
                             vbuf)))
                       (->color-vtx (* n billboard-circle-segments 2))
                       (range n)))]
    (when (pos? (count vbuf))
      (gl/gl-enable gl GL/GL_DEPTH_TEST)
      (.glDepthMask gl false)
      (try
        (let [vb (vtx/use-with ::light-point-gizmo-lines vbuf outline-shader)]
          (gl/with-gl-bindings gl render-args [outline-shader vb]
            (.glDrawArrays gl GL/GL_LINES 0 (count vbuf))))
        (finally
          (gl/gl-disable gl GL/GL_DEPTH_TEST))))
    (render-origin-markers-billboard gl render-args renderables n)))

(defn- render-point-volume [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)
        show-pick (= pass/selection pass)]
    (when show-pick
      (render-origin-markers-selection gl render-args renderables n))))

(defn- fill-directional-move-arrow-tris-only!
  ([vbuf-tris ^Vector3d p ^Vector3d d-world cr cg cb total-len]
   (fill-directional-move-arrow-tris-only! vbuf-tris p d-world cr cg cb total-len 1.0))
  ([vbuf-tris ^Vector3d p ^Vector3d d-world cr cg cb total-len ca]
   (let [^Matrix3d R (mat3-x-axis-to-dir d-world)
         s (/ (double total-len) 100.0)
         cr (double cr) cg (double cg) cb (double cb)]
     (doseq [[mode vs] directional-arrow-vertex-groups
             :when (= mode GL/GL_TRIANGLES)]
       (doseq [v vs]
         (let [^Vector3d w (transform-local-arrow-point R p s (double (nth v 0)) (double (nth v 1)) (double (nth v 2)))]
           (conj-line-vertex! vbuf-tris (.x w) (.y w) (.z w) cr cg cb ca)))))))

(defn- fill-directional-move-arrow-lines-only! [vbuf-lines ^Vector3d p ^Vector3d d-world cr cg cb total-len]
  (let [^Matrix3d R (mat3-x-axis-to-dir d-world)
        s (/ (double total-len) 100.0)
        cr (double cr) cg (double cg) cb (double cb)]
    (doseq [[mode vs] directional-arrow-vertex-groups
            :when (= mode GL/GL_LINES)]
      (doseq [v vs]
        (let [^Vector3d w (transform-local-arrow-point R p s (double (nth v 0)) (double (nth v 1)) (double (nth v 2)))]
          (conj-line-vertex! vbuf-lines (.x w) (.y w) (.z w) cr cg cb))))))

(defn- render-directional-outline [^GL2 gl render-args renderables n]
  (assert (= pass/outline (:pass render-args)))
  (let [n (long n)
        camera (:camera render-args)
        vbuf-tris (persistent!
                    (reduce (fn [vbuf ri]
                              (let [renderable (nth renderables ri)]
                                (if (light-gizmo-selected? renderable)
                                  (let [{:keys [color direction]} (:user-data renderable)
                                        [cr cg cb] (outline-rgb-for-light renderable color)
                                        ^Vector3d p (:world-translation renderable)
                                        d (world-dir-from-light renderable direction)
                                        sf (scene-tools/scale-factor camera (:viewport render-args) p)
                                        total-len (* (double sf) gizmo-target-pixels)]
                                    (fill-directional-move-arrow-tris-only! vbuf p d cr cg cb total-len)
                                    vbuf)
                                  vbuf)))
                            (->color-vtx (* (long n) (long directional-arrow-tri-vert-count)))
                            (range n)))
        vbuf-lines (persistent!
                      (reduce (fn [vbuf ri]
                                (let [renderable (nth renderables ri)]
                                  (if (light-gizmo-selected? renderable)
                                    (let [{:keys [color direction]} (:user-data renderable)
                                          [cr cg cb] (outline-rgb-for-light renderable color)
                                          ^Vector3d p (:world-translation renderable)
                                          d (world-dir-from-light renderable direction)
                                          sf (scene-tools/scale-factor camera (:viewport render-args) p)
                                          total-len (* (double sf) gizmo-target-pixels)]
                                      (fill-directional-move-arrow-lines-only! vbuf p d cr cg cb total-len)
                                      vbuf)
                                    vbuf)))
                              (->color-vtx (* (long n) (long directional-arrow-line-vert-count)))
                              (range n)))]
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
          (gl/gl-disable gl GL/GL_DEPTH_TEST))))
    (render-origin-markers-billboard gl render-args renderables n)))

(defn- render-directional-volume [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)
        show-pick (= pass/selection pass)]
    (when show-pick
      (render-origin-markers-selection gl render-args renderables n))))

(defn- render-spot-outline [^GL2 gl render-args renderables n]
  (assert (= pass/outline (:pass render-args)))
  (when (light-gizmo-selected? (first renderables))
    (render-light-gizmo-lines gl render-args renderables n))
  (render-origin-markers-billboard gl render-args renderables n))

(defn- render-spot-volume [^GL2 gl render-args renderables n]
  (let [pass (:pass render-args)
        show-pick (= pass/selection pass)]
    (when show-pick
      (render-origin-markers-selection gl render-args renderables n))))

(def ^:private render-point-outline-scaled (wrap-uniform-scale render-point-outline))
(def ^:private render-point-volume-scaled (wrap-uniform-scale render-point-volume))
(def ^:private render-directional-outline-scaled (wrap-uniform-scale render-directional-outline))
(def ^:private render-directional-volume-scaled (wrap-uniform-scale render-directional-volume))
(def ^:private render-spot-outline-scaled (wrap-uniform-scale render-spot-outline))
(def ^:private render-spot-volume-scaled (wrap-uniform-scale render-spot-volume))

(defn- preview-light-user-data
  [light-type color intensity range direction inner-cone-angle outer-cone-angle]
  {:editor-preview-light {:light-type light-type
                         :color color
                         :intensity intensity
                         :range range
                         :direction direction
                         :inner-cone-angle inner-cone-angle
                         :outer-cone-angle outer-cone-angle}})

(g/defnk produce-light-scene
  [_node-id light-type color intensity direction range outer-cone-angle inner-cone-angle]
  (let [preview (preview-light-user-data light-type color intensity range direction inner-cone-angle outer-cone-angle)
        rgb-base {:color color
                  :light-rgb [(double (nth color 0 1.0))
                              (double (nth color 1 1.0))
                              (double (nth color 2 1.0))]}
        base-user-data (merge rgb-base preview)]
    (case light-type
      :point
      (let [r (max (double range) 0.01)
            ps (float-array [(float r) (float r) (float r) 1.0])
            aabb (geom/mirrored-point->aabb (Point3d. r r r))
            vol-ud (assoc base-user-data
                     :range range
                     :point-scale ps
                     :double-sided true
                     :geometry scene-shapes/capsule-triangles)
            out-ud (assoc base-user-data :range range)]
        {:node-id _node-id
         :aabb aabb
         :renderable {:render-fn render-point-volume-scaled
                      :batch-key [scene-shapes/shader]
                      :passes [pass/transparent pass/selection]
                      :user-data vol-ud}
         :children [{:node-id _node-id
                     :aabb aabb
                     :renderable {:render-fn render-point-outline-scaled
                                  :batch-key [outline-shader]
                                  :tags #{:outline}
                                  :passes [pass/outline]
                                  :user-data out-ud}}]})

      :directional
      (let [aabb (geom/mirrored-point->aabb (Point3d. 1.5 1.5 1.5))
            dir-ud (assoc base-user-data :direction direction)]
        {:node-id _node-id
         :aabb aabb
         :renderable {:render-fn render-directional-volume-scaled
                      :batch-key [outline-shader]
                      :passes [pass/transparent pass/selection]
                      :user-data dir-ud}
         :children [{:node-id _node-id
                     :aabb aabb
                     :renderable {:render-fn render-directional-outline-scaled
                                  :batch-key [outline-shader]
                                  :tags #{:outline}
                                  :passes [pass/outline]
                                  :user-data dir-ud}}]})

      :spot
      (let [h (max (double range) 0.01)
            half-outer (* 0.5 (Math/toRadians (double outer-cone-angle)))
            half-inner (* 0.5 (Math/toRadians (double inner-cone-angle)))
            inner-radius-ratio (if (> half-outer 1e-8)
                                 (min 1.0 (max 0.0 (/ (Math/tan half-inner) (Math/tan half-outer))))
                                 0.0)
            base-r (max (* h (Math/tan half-outer)) 0.02)
            ps (float-array [(float base-r) (float base-r) (float h) 1.0])
            max-ext (max base-r h)
            aabb (geom/mirrored-point->aabb (Point3d. max-ext max-ext max-ext))
            vol-ud (assoc base-user-data
                     :point-scale ps
                     :double-sided true
                     :geometry (scene-shapes/light-cone-triangles))
            out-ud (assoc base-user-data
                     :geometry (scene-shapes/light-cone-lines inner-radius-ratio)
                     :point-scale ps)]
        {:node-id _node-id
         :aabb aabb
         :renderable {:render-fn render-spot-volume-scaled
                      :batch-key [scene-shapes/shader]
                      :passes [pass/transparent pass/selection]
                      :user-data vol-ud}
         :children [{:node-id _node-id
                     :aabb aabb
                     :renderable {:render-fn render-spot-outline-scaled
                                  :batch-key [outline-shader]
                                  :tags #{:outline}
                                  :passes [pass/outline]
                                  :user-data out-ud}}]})

      {:node-id _node-id
       :aabb geom/empty-bounding-box})))

(defn- light-set-form-op [{:keys [node-id]} [prop] value]
  (case prop
    :direction (g/set-property node-id :direction (vec (take 3 value)))
    (protobuf-forms-util/set-form-op {:node-id node-id} [prop] value)))

(g/defnk produce-form-data
  [_node-id light-type color intensity range direction inner-cone-angle outer-cone-angle]
  (let [direction-vec4 (vec (take 4 (concat direction [0.0 0.0 -1.0 0.0])))
        hidden-range (= :directional light-type)
        hidden-direction (not= :directional light-type)
        hidden-cones (not= :spot light-type)]
    {:navigation false
     :form-ops {:user-data {:node-id _node-id}
                :set light-set-form-op
                :clear protobuf-forms-util/clear-form-op}
     :sections [{:localization-key "light"
                 :fields [{:path [:light-type]
                           :localization-key "light.type"
                           :type :choicebox
                           :options light-type-options
                           :default :point}
                          {:path [:color]
                           :localization-key "light.color"
                           :type :vec4
                           :default [1.0 1.0 1.0 1.0]}
                          {:path [:intensity]
                           :localization-key "light.intensity"
                           :type :number
                           :default 1.0}
                          {:path [:range]
                           :localization-key "light.range"
                           :type :number
                           :default 10.0
                           :hidden hidden-range}
                          {:path [:direction]
                           :localization-key "light.direction"
                           :type :vec4
                           :default [0.0 0.0 -1.0 0.0]
                           :hidden hidden-direction}
                          {:path [:inner-cone-angle]
                           :localization-key "light.inner-cone-angle"
                           :type :number
                           :default 0.0
                           :hidden hidden-cones}
                          {:path [:outer-cone-angle]
                           :localization-key "light.outer-cone-angle"
                           :type :number
                           :default 45.0
                           :hidden hidden-cones}]}]
     :values {[:light-type] light-type
              [:color] color
              [:intensity] intensity
              [:range] range
              [:direction] direction-vec4
              [:inner-cone-angle] inner-cone-angle
              [:outer-cone-angle] outer-cone-angle}}))

(defn- canonicalize-light-read [light-desc]
  {:pre [(map? light-desc)]}
  (let [m (parse-data-desc light-desc)]
    (build-data-desc (:light-type m) (:color m) (:intensity m) (:range m)
                     (:direction m) (:inner-cone-angle m) (:outer-cone-angle m))))

(defn load-light [_project self _resource light-desc]
  {:pre [(map? light-desc)]} ; DataProto$Data in map format.
  (let [m (parse-data-desc light-desc)]
    (apply g/set-properties self (apply concat m))))

(g/defnode LightNode
  (inherits resource-node/ResourceNode)

  (property light-type g/Keyword (default :point)
            (dynamic label (properties/label-dynamic :light :type))
            (dynamic edit-type (g/constantly {:type :choicebox :options light-type-options})))
  (property color types/Color (default [1.0 1.0 1.0 1.0])
            (dynamic label (properties/label-dynamic :light :color))
            (dynamic tooltip (properties/tooltip-dynamic :light :color)))
  (property intensity g/Num (default 1.0)
            (dynamic label (properties/label-dynamic :light :intensity)))
  (property range g/Num (default 10.0)
            (dynamic label (properties/label-dynamic :light :range))
            (dynamic visible (g/fnk [light-type] (contains? #{:point :spot} light-type))))
  (property direction types/Vec3 (default [0.0 0.0 -1.0])
            (dynamic label (properties/label-dynamic :light :direction))
            (dynamic edit-type (g/constantly {:type types/Vec3 :labels ["X" "Y" "Z"]}))
            (dynamic visible (g/fnk [light-type] (= :directional light-type))))
  (property inner-cone-angle g/Num (default 0.0)
            (dynamic label (properties/label-dynamic :light :inner-cone-angle))
            (dynamic visible (g/fnk [light-type] (= :spot light-type))))
  (property outer-cone-angle g/Num (default 45.0)
            (dynamic label (properties/label-dynamic :light :outer-cone-angle))
            (dynamic visible (g/fnk [light-type] (= :spot light-type))))

  (display-order [:light-type :color :intensity :range :direction :inner-cone-angle :outer-cone-angle])

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-light-scene)
  (output node-outline outline/OutlineData :cached produce-outline-data))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "light"
    :node-type LightNode
    :ddf-type DataProto$Data
    :load-fn load-light
    ;; After :read-fn (read-map-without-defaults), normalize to the same map
    ;; produce-save-value emits. Empty/sparse files become {} in the first step;
    ;; without this, :source-value (disk) and :save-value (graph) disagree and
    ;; sparse-protobuf-test / dirty checks fail. Not for migrating legacy formats.
    :sanitize-fn canonicalize-light-read
    :icon outline-icon
    :icon-class :property
    :category (localization/message "resource.category.components")
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{}}}
    :label (localization/message "resource.type.light")))
