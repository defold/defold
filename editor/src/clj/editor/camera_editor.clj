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

(ns editor.camera-editor
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Camera$CameraDesc]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix4d Vector3d Vector4d Quat4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def camera-icon "icons/32/Icons_20-Camera.png")

(def camera-mesh-json "meshes/camera-preview-edge-list.json")

(defn- read-json-resource [path]
  (with-open [reader (io/reader (io/resource path))]
    (json/read reader)))

(def ^:private camera-mesh-lines
  (let [camera-name+edge-list (first (read-json-resource camera-mesh-json)) ; First mesh name + edge-list.
        camera-edge-list (second camera-name+edge-list)
        camera-mesh-lines-v4 (mapv (fn [p]
                                     (Vector4d. (p 0) (p 1) (p 2) 1.0))
                                   camera-edge-list)]
    camera-mesh-lines-v4))

(g/defnk produce-form-data
  [_node-id aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom]
  {:form-ops {:user-data {:node-id _node-id}
              :set protobuf-forms-util/set-form-op
              :clear protobuf-forms-util/clear-form-op}
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

(def outline-shader (shader/make-shader ::outline-shader outline-vertex-shader outline-fragment-shader))

(defmacro ^:private gen-outline-vertex [x y z w cr cg cb]
  `[(/ ~x ~w) (/ ~y ~w) (/ ~z ~w) ~cr ~cg ~cb 1.0])

(defn- conj-camera-mesh-vertices! [vbuf ^Matrix4d inv-view cr cg cb]
  (mapv (fn [^Vector4d p]
          (let [tp (math/transform-vector-v4 inv-view p)
                camera-mesh-vx (gen-outline-vertex (.x tp) (.y tp) (.z tp) (.w tp) cr cg cb)]
            (conj! vbuf camera-mesh-vx)))
        camera-mesh-lines))

(defn- conj-camera-outline! [vbuf ^Vector3d camera-pos ^Matrix4d inv-view ^Matrix4d inv-proj-view cr cg cb]
  (let [far-p0 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0 -1.0 1.0 1.0))
        far-p1 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0  1.0 1.0 1.0))
        far-p2 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0  1.0 1.0 1.0))
        far-p3 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0 -1.0 1.0 1.0))
        ;; near positions
        near-p0 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0 -1.0 -1.0 1.0))
        near-p1 (math/transform-vector-v4 inv-proj-view (Vector4d. -1.0  1.0 -1.0 1.0))
        near-p2 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0  1.0 -1.0 1.0))
        near-p3 (math/transform-vector-v4 inv-proj-view (Vector4d.  1.0 -1.0 -1.0 1.0))
        ;; far vertices
        far-v0 (gen-outline-vertex (.x far-p0) (.y far-p0) (.z far-p0) (.w far-p0) cr cg cb)
        far-v1 (gen-outline-vertex (.x far-p1) (.y far-p1) (.z far-p1) (.w far-p1) cr cg cb)
        far-v2 (gen-outline-vertex (.x far-p2) (.y far-p2) (.z far-p2) (.w far-p2) cr cg cb)
        far-v3 (gen-outline-vertex (.x far-p3) (.y far-p3) (.z far-p3) (.w far-p3) cr cg cb)
        ;; near vertices
        near-v0 (gen-outline-vertex (.x near-p0) (.y near-p0) (.z near-p0) (.w near-p0) cr cg cb)
        near-v1 (gen-outline-vertex (.x near-p1) (.y near-p1) (.z near-p1) (.w near-p1) cr cg cb)
        near-v2 (gen-outline-vertex (.x near-p2) (.y near-p2) (.z near-p2) (.w near-p2) cr cg cb)
        near-v3 (gen-outline-vertex (.x near-p3) (.y near-p3) (.z near-p3) (.w near-p3) cr cg cb)

        cr-less (* ^double cr 0.55)
        cg-less (* ^double cg 0.55)
        cb-less (* ^double cb 0.55)

        ;; camera vertices from center to near
        camera-near-v0 (gen-outline-vertex (.x near-p0) (.y near-p0) (.z near-p0) (.w near-p0) cr-less cg-less cb-less)
        camera-near-v1 (gen-outline-vertex (.x near-p1) (.y near-p1) (.z near-p1) (.w near-p1) cr-less cg-less cb-less)
        camera-near-v2 (gen-outline-vertex (.x near-p2) (.y near-p2) (.z near-p2) (.w near-p2) cr-less cg-less cb-less)
        camera-near-v3 (gen-outline-vertex (.x near-p3) (.y near-p3) (.z near-p3) (.w near-p3) cr-less cg-less cb-less)

        ;; camera vertex for center
        camera-v (gen-outline-vertex (.x camera-pos) (.y camera-pos) (.z camera-pos) 1 cr-less cg-less cb-less)]
    ;; Add camera mesh vertices
    (conj-camera-mesh-vertices! vbuf inv-view cr cg cb)
    (-> vbuf
        ;; Add square for near plane
        (conj! near-v0) (conj! near-v1) (conj! near-v1) (conj! near-v2) (conj! near-v2) (conj! near-v3) (conj! near-v3) (conj! near-v0)
        ;; Add square for far plane
        (conj! far-v0) (conj! far-v1) (conj! far-v1) (conj! far-v2) (conj! far-v2) (conj! far-v3) (conj! far-v3) (conj! far-v0)
        ;; Add lines for frustum from near to far plane corners
        (conj! near-v0) (conj! far-v0)
        (conj! near-v1) (conj! far-v1)
        (conj! near-v2) (conj! far-v2)
        (conj! near-v3) (conj! far-v3)
        ;; Add lines from camera position to near plane
        (conj! camera-v) (conj! camera-near-v0)
        (conj! camera-v) (conj! camera-near-v1)
        (conj! camera-v) (conj! camera-near-v2)
        (conj! camera-v) (conj! camera-near-v3))))

(def ^:private ^:const camera-frustum-vertices-count 32) ; This should correspond to the number of conj! in the last expression above.

(def ^:private ^:const camera-preview-mesh-vertices-count (+ camera-frustum-vertices-count (count camera-mesh-lines)))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(defn- camera-view-matrix [^Vector3d position ^Quat4d rotation]
  (let [m (Matrix4d.)
        p (Vector3d. position)]
    (.setIdentity m)
    (.set m rotation)
    (.transpose m)
    (.transform m p)
    (.negate p)
    (.setColumn m 3 (.x p) (.y p) (.z p) 1.0)
    m))

(defn- camera-projection-matrix [is-orthographic near far fov-deg aspect-ratio]
  ;; TODO: Derive aspect-ratio from display setting in game.project when auto-aspect-ratio is enabled.
  (let [fov-y (double fov-deg)
        fov-x (* fov-y (double aspect-ratio))]
    (if (true? is-orthographic)
      (camera/simple-orthographic-projection-matrix near far fov-x fov-y)
      (camera/simple-perspective-projection-matrix near far fov-x fov-y))))

(defn- gen-outline-vertex-buffer
  [renderables ^long renderable-count]
  (loop [renderables renderables
         vbuf (->color-vtx (* renderable-count camera-preview-mesh-vertices-count))]
    (if-let [renderable (first renderables)]
      (let [color (colors/renderable-outline-color renderable)
            cr (get color 0)
            cg (get color 1)
            cb (get color 2)
            {:keys [is-orthographic near-z far-z fov aspect-ratio]} (:user-data renderable)
            world-translation (:world-translation renderable)
            world-rotation (:world-rotation renderable)
            ^Matrix4d proj-matrix (camera-projection-matrix is-orthographic near-z far-z fov aspect-ratio)
            ^Matrix4d view-matrix (camera-view-matrix world-translation world-rotation)
            ^Matrix4d inv-view-matrix (math/inverse view-matrix)
            ^Matrix4d proj-view-matrix (doto (Matrix4d. proj-matrix) (.mul view-matrix))
            ^Matrix4d inv-view-proj-matrix (math/inverse proj-view-matrix)]
        (recur (rest renderables) (conj-camera-outline! vbuf world-translation inv-view-matrix inv-view-proj-matrix cr cg cb)))
      (persistent! vbuf))))

(defn- render-frustum-outlines [^GL2 gl render-args renderables ^long renderable-count]
  (assert (= pass/outline (:pass render-args)))
  (let [outline-vertex-binding (vtx/use-with ::frustum-outline (gen-outline-vertex-buffer renderables renderable-count) outline-shader)]
    (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (* renderable-count camera-preview-mesh-vertices-count)))))

(g/defnk produce-camera-scene
  [_node-id fov aspect-ratio near-z far-z orthographic-projection]
  ;; TODO: Better AABB calculation
  (let [^double ext-x far-z
        ^double ext-y far-z
        ^double ext-z far-z
        ext [ext-x ext-y ext-z]
        neg-ext [(- ext-x) (- ext-y) (- ext-z)]
        aabb (geom/coords->aabb neg-ext ext)]
    {:node-id _node-id
     :aabb aabb
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-frustum-outlines
                              :batch-key [outline-shader]
                              :tags #{:camera :outline}
                              :select-batch-key _node-id
                              :user-data {:fov (math/rad->deg fov) ; TODO: FOV should be edited as degrees, not radians.
                                          :aspect-ratio aspect-ratio
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
    :auto-aspect-ratio (not= 0 (:auto-aspect-ratio camera))
    :orthographic-projection (not= 0 (:orthographic-projection camera))
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
  (output scene g/Any :cached produce-camera-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "camera"
    :node-type CameraNode
    :ddf-type Camera$CameraDesc
    :load-fn load-camera
    :icon camera-icon
    :icon-class :property
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{}}}
    :label "Camera"))
