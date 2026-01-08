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

(ns editor.camera-editor
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource-node :as resource-node]
            [editor.scene-tools :as scene-tools]
            [editor.shaders :as shaders]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto Camera$CameraDesc Camera$OrthoZoomMode]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix4d Quat4d Vector3d Vector4d]))

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

;; Precomputed extents of the authored camera mesh (in view-space units).
;; Used to calibrate pixel-stable scaling so the icon keeps a reasonable size.
(def ^:private camera-mesh-height-units
  (if-let [s (seq camera-mesh-lines)]
    (let [[^double miny ^double maxy]
          (reduce (fn [[mn mx] ^Vector4d v]
                    (let [y (.y v)]
                      [(Math/min (double mn) y)
                       (Math/max (double mx) y)]))
                  [Double/POSITIVE_INFINITY Double/NEGATIVE_INFINITY]
                  s)]
      (- maxy miny))
    0.0))

(def ^:private ^:const camera-mesh-target-pixels 32.0)

(g/defnk produce-form-data
  [_node-id aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom orthographic-mode]
  {:form-ops {:user-data {:node-id _node-id}
              :set protobuf-forms-util/set-form-op
              :clear protobuf-forms-util/clear-form-op}
   :navigation false
   :sections [{:localization-key "camera"
               :fields [{:path [:aspect-ratio]
                         :localization-key "camera.aspect-ratio"
                         :type :number}
                        {:path [:fov]
                         :localization-key "camera.fov"
                         :type :number}
                        {:path [:near-z]
                         :localization-key "camera.near-z"
                         :type :number}
                        {:path [:far-z]
                         :localization-key "camera.far-z"
                         :type :number}
                        {:path [:auto-aspect-ratio]
                         :localization-key "camera.auto-aspect-ratio"
                         :type :boolean}
                        {:path [:orthographic-projection]
                         :localization-key "camera.orthographic-projection"
                         :type :boolean}
                        {:path [:orthographic-mode]
                         :localization-key "camera.orthographic-mode"
                         :type :choicebox
                         :options (sort-by first (protobuf-forms/make-enum-options Camera$OrthoZoomMode))}
                        {:path [:orthographic-zoom]
                         :localization-key "camera.orthographic-zoom"
                         :type :number}]}]
   :values {[:aspect-ratio] aspect-ratio
            [:fov] fov
            [:near-z] near-z
            [:far-z] far-z
            [:auto-aspect-ratio] auto-aspect-ratio
            [:orthographic-projection] orthographic-projection
            [:orthographic-zoom] orthographic-zoom
            [:orthographic-mode] orthographic-mode}})

(g/defnk produce-save-value
  [aspect-ratio fov near-z far-z auto-aspect-ratio orthographic-projection orthographic-zoom orthographic-mode]
  (protobuf/make-map-without-defaults Camera$CameraDesc
    :aspect-ratio aspect-ratio
    :fov fov
    :near-z near-z
    :far-z far-z
    :auto-aspect-ratio (protobuf/boolean->int auto-aspect-ratio)
    :orthographic-projection (protobuf/boolean->int orthographic-projection)
    :orthographic-zoom orthographic-zoom
    :orthographic-mode orthographic-mode))

(defn build-camera
  [resource _dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Camera$CameraDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource save-value]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-camera
      :user-data {:pb-msg save-value}})])

(def ^:private outline-shader shaders/basic-color-world-space)

(defmacro ^:private gen-outline-vertex [x y z w cr cg cb]
  `[(/ ~x ~w) (/ ~y ~w) (/ ~z ~w) ~cr ~cg ~cb 1.0])

(defn- conj-camera-mesh-vertices!
  "Append the small camera mesh to the vertex buffer.
  The mesh is authored in camera view space. We scale it uniformly in view space
  by `mesh-scale` before transforming to world space with `inv-view` to achieve
  a pixel-stable on-screen size."
  [vbuf ^Matrix4d inv-view mesh-scale cr cg cb]
  (mapv (fn [^Vector4d p]
          (let [sx (* (.x p) (double mesh-scale))
                sy (* (.y p) (double mesh-scale))
                sz (* (.z p) (double mesh-scale))
                sp (Vector4d. sx sy sz 1.0)
                tp (math/transform-vector-v4 inv-view sp)
                camera-mesh-vx (gen-outline-vertex (.x tp) (.y tp) (.z tp) (.w tp) cr cg cb)]
            (conj! vbuf camera-mesh-vx)))
        camera-mesh-lines))

(defn- conj-camera-outline!
  "Append the camera preview (small mesh + frustum) for one camera component.
  `mesh-scale` only affects the small camera mesh. Frustum geometry remains
  world-sized to reflect the camera's true near/far and FOV."
  [vbuf ^Vector3d camera-pos ^Matrix4d inv-view ^Matrix4d inv-proj-view mesh-scale cr cg cb]
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
    ;; Add camera mesh vertices (pixel-stable size)
    (conj-camera-mesh-vertices! vbuf inv-view mesh-scale cr cg cb)
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

(defn- camera-projection-matrix
  [is-orthographic near far fov-deg aspect-ratio display-width display-height orthographic-zoom orthographic-mode]
  (let [fov-y (double fov-deg)
        fov-x (* fov-y (double aspect-ratio))
        dw (double display-width)
        dh (double display-height)]
    (if (true? is-orthographic)
      (let [zoom (double (or orthographic-zoom 1.0))
            zoom (if (= orthographic-mode :ortho-mode-fixed)
                   zoom
                   1.0)
            ow (/ dw zoom)
            oh (/ dh zoom)]
        (camera/simple-orthographic-projection-matrix near far ow oh))
      (camera/simple-perspective-projection-matrix near far fov-x fov-y))))

(defn- gen-outline-vertex-buffer
  [render-args renderables ^long renderable-count]
  (loop [renderables renderables
         vbuf (->color-vtx (* renderable-count camera-preview-mesh-vertices-count))]
    (if-let [renderable (first renderables)]
      (let [color (colors/renderable-outline-color renderable)
            cr (get color 0)
            cg (get color 1)
            cb (get color 2)
            {:keys [is-orthographic near-z far-z fov aspect-ratio display-width display-height orthographic-zoom orthographic-mode]} (:user-data renderable)
            world-translation (:world-translation renderable)
            world-rotation (:world-rotation renderable)]
        ;; Hide frustum preview when orthographic zoom is invalid (<= 0) in fixed mode
        (if (and is-orthographic (= orthographic-mode :ortho-mode-fixed)
                 (not (> (double (or orthographic-zoom 0.0)) 0.0)))
          (recur (rest renderables) vbuf)
          (let [;; Pixel-stable scale for the small camera mesh at the camera position.
                ;; Similar to manipulators: 1 world unit at the reference depth -> Δpx.
                ;; We target a fixed on-screen height (camera-mesh-target-pixels).
                sf (scene-tools/scale-factor (:camera render-args) (:viewport render-args) world-translation)
                mh (double camera-mesh-height-units)
                tp (double camera-mesh-target-pixels)
                ratio (if (> mh 0.0) (/ tp mh) 1.0)
                mesh-scale (* (double sf) ratio)
                ^Matrix4d proj-matrix (camera-projection-matrix is-orthographic near-z far-z fov aspect-ratio display-width display-height orthographic-zoom orthographic-mode)
                ^Matrix4d view-matrix (camera-view-matrix world-translation world-rotation)
                ^Matrix4d inv-view-matrix (math/inverse view-matrix)
                ^Matrix4d proj-view-matrix (doto (Matrix4d. proj-matrix) (.mul view-matrix))
                ^Matrix4d inv-view-proj-matrix (math/inverse proj-view-matrix)]
            (recur (rest renderables) (conj-camera-outline! vbuf world-translation inv-view-matrix inv-view-proj-matrix mesh-scale cr cg cb)))))
      (persistent! vbuf))))

(defn- render-frustum-outlines [^GL2 gl render-args renderables ^long renderable-count]
  (assert (= pass/outline (:pass render-args)))
  (let [vertex-buffer (gen-outline-vertex-buffer render-args renderables renderable-count)
        outline-vertex-binding (vtx/use-with ::frustum-outline vertex-buffer outline-shader)]
    (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (* renderable-count camera-preview-mesh-vertices-count)))))

(g/defnk produce-camera-scene
  [_node-id fov aspect-ratio near-z far-z orthographic-projection orthographic-zoom orthographic-mode project-display-width project-display-height]
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
                                          :is-orthographic orthographic-projection
                                          :orthographic-zoom orthographic-zoom
                                          :orthographic-mode orthographic-mode
                                          :display-width project-display-width
                                          :display-height project-display-height}
                              :passes [pass/outline]}}]}))

(defn load-camera [project self _resource camera-desc]
  {:pre [(map? camera-desc)]} ; Camera$CameraDesc in map format.
  (concat
    (g/connect project :display-width self :project-display-width)
    (g/connect project :display-height self :project-display-height)
    (gu/set-properties-from-pb-map self Camera$CameraDesc camera-desc
      aspect-ratio :aspect-ratio
      fov :fov
      near-z :near-z
      far-z :far-z
      auto-aspect-ratio (protobuf/int->boolean :auto-aspect-ratio)
      orthographic-projection (protobuf/int->boolean :orthographic-projection)
      orthographic-zoom :orthographic-zoom
      orthographic-mode :orthographic-mode)))

(g/defnode CameraNode
  (inherits resource-node/ResourceNode)

  (property aspect-ratio g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :camera :aspect-ratio))
            (dynamic tooltip (properties/tooltip-dynamic :camera :aspect-ratio))
            (dynamic read-only? (g/fnk [orthographic-projection] orthographic-projection)))
  (property fov g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :camera :fov))
            (dynamic tooltip (properties/tooltip-dynamic :camera :fov))
            (dynamic read-only? (g/fnk [orthographic-projection] orthographic-projection)))
  (property near-z g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :camera :near-z))
            (dynamic tooltip (properties/tooltip-dynamic :camera :near-z)))
  (property far-z g/Num ; Required protobuf field.
            (dynamic label (properties/label-dynamic :camera :far-z))
            (dynamic tooltip (properties/tooltip-dynamic :camera :far-z)))
  (property auto-aspect-ratio g/Bool (default (protobuf/int->boolean (protobuf/default Camera$CameraDesc :auto-aspect-ratio)))
            (dynamic label (properties/label-dynamic :camera :auto-aspect-ratio))
            (dynamic tooltip (properties/tooltip-dynamic :camera :auto-aspect-ratio))
            (dynamic read-only? (g/fnk [orthographic-projection] orthographic-projection)))
  (property orthographic-projection g/Bool (default (protobuf/int->boolean (protobuf/default Camera$CameraDesc :orthographic-projection)))
            (dynamic label (properties/label-dynamic :camera :orthographic-projection))
            (dynamic tooltip (properties/tooltip-dynamic :camera :orthographic-projection)))
  (property orthographic-mode g/Keyword (default (protobuf/default Camera$CameraDesc :orthographic-mode))
            (dynamic label (properties/label-dynamic :camera :orthographic-mode))
            (dynamic tooltip (properties/tooltip-dynamic :camera :orthographic-mode))
            (dynamic read-only? (g/fnk [orthographic-projection] (not orthographic-projection)))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Camera$OrthoZoomMode))))
  (property orthographic-zoom g/Num (default (protobuf/default Camera$CameraDesc :orthographic-zoom))
            (dynamic label (properties/label-dynamic :camera :orthographic-zoom))
            (dynamic tooltip (properties/tooltip-dynamic :camera :orthographic-zoom))
            (dynamic read-only? (g/fnk [orthographic-projection orthographic-mode] (not (and orthographic-projection (= orthographic-mode :ortho-mode-fixed)))))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? orthographic-zoom)))

  (input project-display-width g/Num)
  (input project-display-height g/Num)

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :node-outline-key "Camera"
                                                      :label (localization/message "outline.camera")
                                                      :icon camera-icon}))

  (output save-value g/Any :cached produce-save-value)
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
    :label (localization/message "resource.type.camera")))
