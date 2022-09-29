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

(ns editor.camera-editor
  (:require [clojure.string :as s]
            [plumbing.core :as pc]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-shapes :as scene-shapes]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Camera$CameraDesc]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d Quat4d]))

(set! *warn-on-reflection* true)

(def camera-icon "icons/32/Icons_20-Camera.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom]
  {:form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :navigation false
   :sections [{:title "Camera"
               :fields [{:path [:aspect-ratio]
                         :label "Aspect Ratio"
                         :type :number}
                        {:path [:fov]
                         :label "FOV"
                         :type :number}
                        {:path [:near-z]
                         :label "Near-Z"
                         :type :number}
                        {:path [:far-z]
                         :label "Far-Z"
                         :type :number}
                        {:path [:auto-aspect-ratio]
                         :label "Auto Aspect Ratio"
                         :type :boolean}
                        {:path [:orthographic-projection]
                         :label "Orthographic Projection"
                         :type :boolean}
                        {:path [:orthographic-zoom]
                         :label "Orthographic Zoom"
                         :type :number}]}]
   :values {[:aspect-ratio] aspect-ratio
            [:fov] fov
            [:near-z] near-z
            [:far-z] far-z
            [:auto-aspect-ratio] auto-aspect-ratio
            [:orthographic-projection] orthographic-projection
            [:orthographic-zoom] orthographic-zoom}})

(g/defnk produce-pb-msg
  [aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom]
  {:aspect-ratio aspect-ratio
   :fov fov
   :near-z near-z
   :far-z far-z
   :auto-aspect-ratio (if (true? auto-aspect-ratio) 1 0)
   :orthographic-projection (if (true? orthographic-projection) 1 0)
   :orthographic-zoom orthographic-zoom})

(defn build-camera
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Camera$CameraDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-camera
      :user-data {:pb-msg pb-msg}})])

(shader/defshader outline-vertex-shader
                  (attribute vec4 position)
                  (attribute vec4 color)
                  (varying vec4 var_color)
                  (defn void main []
                    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
                    (setq var_color color)))

(shader/defshader outline-fragment-shader
                  (varying vec4 var_color)
                  (defn void main []
                    (setq gl_FragColor var_color)))

; Copied from editor.sprite, comment left as-is
; TODO - macro of this
(def outline-shader (shader/make-shader ::outline-shader outline-vertex-shader outline-fragment-shader))

(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) cr cg cb 1])

(defn- gen-outline-vertex-raw [x y z w cr cg cb]
  [(/ x w) (/ y w) (/ z w) cr cg cb 1])

(defn- conj-outline-quad! [vbuf ^Matrix4d wt ^Point3d pt cr cg cb]
  (let [x1 (* 0.5 1.0)
        y1 (* 0.5 1.0)
        x0 (- x1)
        y0 (- y1)
        v0 (gen-outline-vertex wt pt x0 y0 cr cg cb)
        v1 (gen-outline-vertex wt pt x1 y0 cr cg cb)
        v2 (gen-outline-vertex wt pt x1 y1 cr cg cb)
        v3 (gen-outline-vertex wt pt x0 y1 cr cg cb)]
    (-> vbuf (conj! v0) (conj! v1) (conj! v1) (conj! v2) (conj! v2) (conj! v3) (conj! v3) (conj! v0))))

(defn- conj-outline-frustum! [vbuf ^Vector3d camera-pos ^Matrix4d inv-proj-view cr cg cb]
  (let [far0 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0 -1.0 1.0 1.0))
        far1 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0  1.0 1.0 1.0))
        far2 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0  1.0 1.0 1.0))
        far3 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0 -1.0 1.0 1.0))
        ; near positions
        near0 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0 -1.0 -1.0 1.0))
        near1 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0  1.0 -1.0 1.0))
        near2 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0  1.0 -1.0 1.0))
        near3 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0 -1.0 -1.0 1.0))
        far-v0 (gen-outline-vertex-raw (.x far0) (.y far0) (.z far0) (.w far0) cr cg cb)
        far-v1 (gen-outline-vertex-raw (.x far1) (.y far1) (.z far1) (.w far1) cr cg cb)
        far-v2 (gen-outline-vertex-raw (.x far2) (.y far2) (.z far2) (.w far2) cr cg cb)
        far-v3 (gen-outline-vertex-raw (.x far3) (.y far3) (.z far3) (.w far3) cr cg cb)
        near-v0 (gen-outline-vertex-raw (.x near0) (.y near0) (.z near0) (.w near0) cr cg cb)
        near-v1 (gen-outline-vertex-raw (.x near1) (.y near1) (.z near1) (.w near1) cr cg cb)
        near-v2 (gen-outline-vertex-raw (.x near2) (.y near2) (.z near2) (.w near2) cr cg cb)
        near-v3 (gen-outline-vertex-raw (.x near3) (.y near3) (.z near3) (.w near3) cr cg cb)
        ; camera center position
        camera-v0 (gen-outline-vertex-raw (.x camera-pos) (.y camera-pos) (.z camera-pos) 1 cr cg cb)
        ]
    ; Add square for near plane
    ; Add square for far plane
    ; Add lines for frustum from center to far plane corners
    (-> vbuf (conj! near-v0) (conj! near-v1) (conj! near-v1) (conj! near-v2) (conj! near-v2) (conj! near-v3) (conj! near-v3) (conj! near-v0)
        (conj! far-v0) (conj! far-v1) (conj! far-v1) (conj! far-v2) (conj! far-v2) (conj! far-v3) (conj! far-v3) (conj! far-v0)
        (conj! near-v0) (conj! far-v0)
        (conj! near-v1) (conj! far-v1)
        (conj! near-v2) (conj! far-v2)
        (conj! near-v3) (conj! far-v3))))

(vtx/defvertex color-vtx
               (vec3 position)
               (vec4 color))

(defn- camera-view-matrix [^Vector3d position ^Quat4d rotation]
        (let [m (Matrix4d.)]
          (.setIdentity m)
          (.set m rotation)
          (.transpose m)
          (.transform m position)
          (.negate position)
          (.setColumn m 3 (.x position) (.y position) (.z position) 1.0)
          m))

(defn- camera-perspective-projection-matrix [near far fov]
  (let [fov-deg (math/rad->deg fov)
        fov-x  fov-deg
        fov-y  fov-deg
        aspect (/ fov-x fov-y)

        ymax (* near (Math/tan (/ (* fov-y Math/PI) 360.0)))
        ymin (- ymax)
        xmin (* ymin aspect)
        xmax (* ymax aspect)

        x    (/ (* 2.0 near) (- xmax xmin))
        y    (/ (* 2.0 near) (- ymax ymin))
        a    (/ (+ xmin xmax) (- xmax xmin))
        b    (/ (+ ymin ymax) (- ymax ymin))
        c    (/ (- (+     near far)) (- far near))
        d    (/ (- (* 2.0 near far)) (- far near))
        m    (Matrix4d.)]
    (set! (. m m00) x)
    (set! (. m m01) 0.0)
    (set! (. m m02) a)
    (set! (. m m03) 0.0)

    (set! (. m m10) 0.0)
    (set! (. m m11) y)
    (set! (. m m12) b)
    (set! (. m m13) 0.0)

    (set! (. m m20) 0.0)
    (set! (. m m21) 0.0)
    (set! (. m m22) c)
    (set! (. m m23) d)

    (set! (. m m30) 0.0)
    (set! (. m m31) 0.0)
    (set! (. m m32) -1.0)
    (set! (. m m33) 0.0)
    m))

(defn- camera-orthographic-projection-matrix [near far fov]
        (let [fov-deg (math/rad->deg fov)
              fov-x  fov-deg
              fov-y  fov-deg
              right  (/ fov-x 2.0)
              left   (- right)
              top    (/ fov-y 2.0)
              bottom (- top)
              m      (Matrix4d.)]
          (set! (. m m00) (/ 2.0 (- right left)))
          (set! (. m m01) 0.0)
          (set! (. m m02) 0.0)
          (set! (. m m03) (/ (- (+ right left)) (- right left)))

          (set! (. m m10) 0.0)
          (set! (. m m11) (/ 2.0 (- top bottom)))
          (set! (. m m12) 0.0)
          (set! (. m m13) (/ (- (+ top bottom)) (- top bottom)))

          (set! (. m m20) 0.0)
          (set! (. m m21) 0.0)
          (set! (. m m22) (/ 2.0 (- near far)))
          (set! (. m m23) (/ (+ near far) (- near far)))

          (set! (. m m30) 0.0)
          (set! (. m m31) 0.0)
          (set! (. m m32) 0.0)
          (set! (. m m33) 1.0)
          m))

(defn- camera-projection-matrix [is-orthographic near far fov]
  (if (true? is-orthographic)
    (camera-orthographic-projection-matrix near far fov)
    (camera-perspective-projection-matrix near far fov)))

(defn- gen-outline-vertex-buffer
  [renderables count]
  (let [tmp-point (Point3d.)]
    (loop [renderables renderables
           vbuf (->color-vtx (* count 24))]
      (if-let [renderable (first renderables)]
        (let [color (if (:selected renderable) colors/selected-outline-color colors/outline-color)
              cr (get color 0)
              cg (get color 1)
              cb (get color 2)
              user-data (:user-data renderable)
              world-transform (:world-transform renderable)
              world-translation (:world-translation renderable)
              world-rotation (:world-rotation renderable)
              ^Matrix4d proj-matrix (camera-projection-matrix (:is-orthographic user-data) (:near-z user-data) (:far-z user-data) (:fov user-data))
              ^Matrix4d view-matrix (camera-view-matrix world-translation world-rotation)
              ^Matrix4d proj-view-matrix (doto (Matrix4d. proj-matrix) (.mul view-matrix))
              ^Matrix4d inv-view-proj-matrix (math/inverse proj-view-matrix)]
          (recur (rest renderables) (conj-outline-frustum! vbuf world-translation inv-view-proj-matrix cr cg cb)))
        (persistent! vbuf)))))

(defn- render-frustum-outlines [^GL2 gl render-args renderables count]
  (assert (= pass/outline (:pass render-args)))
  (let [outline-vertex-binding (vtx/use-with ::frustum-outline (gen-outline-vertex-buffer renderables count) outline-shader)]
    (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
                         (gl/gl-draw-arrays gl GL/GL_LINES 0 (* count 24)))))

(g/defnk produce-camera-scene
  [_node-id fov near-z far-z orthographic-projection]
  (let [ext-x (* 0.5 2.0)
        ext-y (* 0.5 2.0)
        ext-z (* 0.5 2.0)
        ext [ext-x ext-y ext-z]
        neg-ext [(- ext-x) (- ext-y) (- ext-z)]
        aabb (geom/coords->aabb ext neg-ext)]
    {:node-id _node-id
     :aabb aabb
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-frustum-outlines
                              :batch-key [outline-shader]
                              :tags #{:sprite :outline}
                              :select-batch-key _node-id
                              :user-data {:fov fov
                                          :near-z near-z
                                          :far-z far-z
                                          :is-orthographic orthographic-projection}
                              :passes [pass/outline]}}]}))

(defn load-camera [project self resource camera]
  (g/set-property self
    :aspect-ratio (:aspect-ratio camera)
    :fov (:fov camera)
    :near-z (:near-z camera)
    :far-z (:far-z camera)
    :auto-aspect-ratio (not (zero? (:auto-aspect-ratio camera)))
    :orthographic-projection (not (zero? (:orthographic-projection camera)))
    :orthographic-zoom (:orthographic-zoom camera)))

(g/defnode CameraNode
  (inherits resource-node/ResourceNode)

  (property aspect-ratio g/Num)
  (property fov g/Num)
  (property near-z g/Num)
  (property far-z g/Num)
  (property auto-aspect-ratio g/Bool)
  (property orthographic-projection g/Bool)
  (property orthographic-zoom g/Num)

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :node-outline-key "Camera"
                                                      :label "Camera"
                                                      :icon camera-icon}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output scene g/Any produce-camera-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "camera"
                                    :node-type CameraNode
                                    :ddf-type Camera$CameraDesc
                                    :load-fn load-camera
                                    :icon camera-icon
                                    :view-types [:cljfx-form-view :text]
                                    :view-opts {}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Camera"))
