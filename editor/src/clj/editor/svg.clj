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

(ns editor.svg
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.code.resource :as code-resource]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.scene-picking :as scene-picking]
            [util.fn :as fn])
  (:import [com.dynamo.bob.pipeline MaterialBuilder SvgImporter SvgImporter$Image]
           [com.dynamo.bob.slug SlugCompiler SlugCompiler$Image]
           [com.dynamo.render.proto Material$MaterialDesc$Sampler]
           [com.github.benmanes.caffeine.cache Caffeine]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL3]
           [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [javax.vecmath Matrix4d Point3d Vector4d]))

(set! *warn-on-reflection* true)

(defn svg-resource? [resource]
  (and resource (= "svg" (resource/ext resource))))

(defn vector-texture? [texture]
  (true? (:vector texture)))

(defn validate-raster-atlas [node-id property texture-set]
  (when (= :texture-set-mode-vector (:mode texture-set))
    (g/->error node-id property :fatal nil (localization/message "error.atlas.vector-unsupported"))))

(defn raster-atlas-resource? [atlas-resource]
  ;; Called by resource dialogs, outside graph evaluation. Read current state
  ;; here so edits made while a picker is open are included.
  (or (not= "atlas" (resource/ext atlas-resource))
      (g/with-auto-evaluation-context evaluation-context
        (let [project (project/get-project (:basis evaluation-context))
              node (project/get-resource-node project atlas-resource evaluation-context)]
          (not= :vector (g/node-value node :atlas-mode evaluation-context))))))

(defn vector-material? [samplers]
  (MaterialBuilder/isVectorMaterial (mapv :name samplers)))

(defn prop-vector-material-contract? [_material samplers sampler-types]
  (when (vector-material? samplers)
    (when-let [problem (MaterialBuilder/vectorMaterialProblem (mapv #(protobuf/map->pb Material$MaterialDesc$Sampler %) samplers) sampler-types)]
      (localization/message (if (.samplerType problem) "error.sprite.vector-sampler-type" "error.sprite.vector-sampler-filter")
                            {"sampler" (.sampler problem)}))))

(defn image-samplers [samplers]
  (if-not (vector-material? samplers)
    samplers
    (filterv #(= "curve_texture" (:name %)) samplers)))

(defn- make-preview-shader [vertex-source fragment-source selection]
  (let [fragment-source (cond-> fragment-source
                          selection (string/replace-first #"(?m)^([ \t]*#version[^\r\n]*)" "$1\n#define SPRITE_VECTOR_PICKING"))
        {:keys [shader-type+source-pairs location+attribute-name-pairs
                array-sampler-name->slice-sampler-names strip-resource-binding-namespace-regex-str
                attribute-reflection-infos]}
        (shader/read-combined-shader-info ["vector.vp" "vector.fp"] {}
                                          (fn [path]
                                            (if (= "vector.vp" path)
                                              vertex-source
                                              fragment-source)))]
    (shader/make-shader-lifecycle [::vector-preview selection vertex-source fragment-source]
                                  (shader/make-shader-request-data shader-type+source-pairs location+attribute-name-pairs
                                                                   array-sampler-name->slice-sampler-names strip-resource-binding-namespace-regex-str)
                                  attribute-reflection-infos
                                  {"view_proj" :view-proj
                                   "tint" (Vector4d. 1.0 1.0 1.0 1.0)
                                   "id" :id-color
                                   "curve_texture" 0
                                   "band_texture" 1
                                   "paint_texture" 2})))

(fn/defn-cached ^:private make-preview-shaders
  {:cache (-> (Caffeine/newBuilder) (.maximumSize 8) (.build))}
  [vertex-source fragment-source]
  {:preview-shader (make-preview-shader vertex-source fragment-source false)
   :selection-shader (make-preview-shader vertex-source fragment-source true)})

(g/defnode VectorPreviewNode
  (property vector-vertex-shader resource/Resource
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:shader-source-info :vector-vertex-info]))))
  (property vector-fragment-shader resource/Resource
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:shader-source-info :vector-fragment-info]))))
  (input vector-vertex-info g/Any)
  (input vector-fragment-info g/Any)
  (input vector-preview-enabled g/Bool)
  (output vector-preview-shaders g/Any :cached
          (g/fnk [vector-preview-enabled ^:try vector-vertex-info ^:try vector-fragment-info]
            (when vector-preview-enabled
              (g/precluding-errors [vector-vertex-info vector-fragment-info]
                (resource-io/with-error-translation (:resource vector-fragment-info) (:node-id vector-fragment-info) :lines
                  (make-preview-shaders (:shader-source vector-vertex-info) (:shader-source vector-fragment-info))))))))

(defn load-preview-shaders [{:keys [resolve-resource-fn]} {:keys [node-id owner-resource]}]
  (into
    (g/set-property node-id :vector-vertex-shader (resolve-resource-fn owner-resource "/builtins/materials/sprite-vector.vp"))
    (g/set-property node-id :vector-fragment-shader (resolve-resource-fn owner-resource "/builtins/materials/sprite-vector.fp"))))

(defn make-gpu-texture [node-id payload preview-shaders]
  (assoc preview-shaders
    :vector true
    :textures
    (mapv (fn [index field data-format stride]
            (let [^ByteString bytes (payload field)
                  data (doto (ByteBuffer/allocateDirect (.size bytes))
                         (.order ByteOrder/LITTLE_ENDIAN)
                         (.put (.toByteArray bytes))
                         (.flip))
                  request (texture/make-texture-request-data data (hash bytes) data-format 4096 (quot (.size bytes) (* 4096 stride)) false)]
              (texture/make-gpu-texture [node-id field] [request] (vector-of :int index)
                                        {:min-filter GL3/GL_NEAREST :mag-filter GL3/GL_NEAREST
                                         :wrap-s GL3/GL_CLAMP_TO_EDGE :wrap-t GL3/GL_CLAMP_TO_EDGE})))
          [0 1 2] [:curves :bands :paints] [:rgba16f :r32ui :rgba32f] [8 4 16])))

(defn bind-textures! [gl render-args shader gpu-texture]
  (doseq [[sampler texture] (mapv vector ["curve_texture" "band_texture" "paint_texture"] (:textures gpu-texture))]
    (gl/bind gl texture render-args)
    (shader/set-samplers-by-name shader gl sampler (:texture-units texture))))

(defn unbind-textures! [gl render-args gpu-texture]
  (run! #(gl/unbind gl % render-args) (:textures gpu-texture)))

(vtx/defvertex vector-vtx
  (vec4 position)
  (vec2 texcoord0)
  (vec1 page_index)
  (vec1 svg_time))

(defn render-vectors [^GL3 gl render-args renderables _count]
  (doseq [renderable renderables]
    (let [{:keys [gpu-texture x y width height viewport-width viewport-height paint]} (:user-data renderable)
          selection (= pass/selection (:pass render-args))
          shader ((if selection :selection-shader :preview-shader) gpu-texture)
          ^Matrix4d transform (:world-transform renderable)
          point (Point3d.)
          vertex-buffer (->vector-vtx 6)
          ^ByteBuffer buffer (vtx/buf vertex-buffer)]
      (doseq [[cx cy] [[0 0] [1 0] [0 1] [1 0] [1 1] [0 1]]]
        (.set point (+ (double x) (* cx (double width))) (+ (double y) (* cy (double height))) 0.0)
        (.transform transform point)
        (vtx/buf-push-floats! buffer [(.x point) (.y point) (.z point) 1.0
                                      (* cx (double viewport-width)) (* (- 1 cy) (double viewport-height)) paint 0.0]))
      (vtx/flip! vertex-buffer)
      (let [binding (vtx/use-with ::vector vertex-buffer shader)
            render-args (cond-> render-args
                          selection (assoc :id-color (scene-picking/renderable-picking-id-uniform renderable)))]
        (gl/with-gl-bindings gl render-args [shader binding]
          (bind-textures! gl render-args shader gpu-texture)
          (gl/set-blend-mode gl :blend-mode-alpha)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
          (unbind-textures! gl render-args gpu-texture))))))

(defn make-scene [node-id gpu-texture x y width height viewport-width viewport-height paint]
  {:node-id node-id
   :aabb (geom/make-aabb (Point3d. x y 0.0) (Point3d. (+ x width) (+ y height) 0.0))
   :renderable {:render-fn render-vectors
                :tags #{:atlas}
                :passes [pass/transparent pass/selection]
                :user-data {:gpu-texture gpu-texture :x x :y y :width width :height height
                            :viewport-width viewport-width :viewport-height viewport-height :paint paint}}})

(g/defnode SvgNode
  (inherits code-resource/CodeEditorResourceNode)
  (inherits VectorPreviewNode)
  (output vector-preview-enabled g/Bool (g/constantly true))

  (output svg-content g/Any :cached
          (g/fnk [save-value]
            (.getBytes ^String (code-data/lines->string save-value) StandardCharsets/UTF_8)))
  (output svg-image g/Any :cached
          (g/fnk [_node-id resource svg-content]
            (resource-io/with-error-translation resource _node-id :svg-image
              (SvgImporter/parse svg-content))))
  (output size g/Any
          (g/fnk [svg-image]
            {:width (int (Math/ceil (.width ^SvgImporter$Image svg-image)))
             :height (int (Math/ceil (.height ^SvgImporter$Image svg-image)))}))
  (output vector-payload g/Any :cached
          (g/fnk [_node-id resource svg-image]
            (resource-io/with-error-translation resource _node-id :vector-payload
              (let [compiled (SlugCompiler/compileAtlas (into-array SlugCompiler$Image [(SvgImporter/toVectorImage svg-image)]))]
                {:curves (ByteString/copyFrom (.-pixels (.curves compiled)))
                 :bands (ByteString/copyFrom (.-pixels (.bands compiled)))
                 :paints (ByteString/copyFrom (.-pixels (.paints compiled)))
                 :paint (aget (.imagePaints compiled) 0)}))))
  (output gpu-texture g/Any :cached
          (g/fnk [_node-id vector-payload vector-preview-shaders]
            (make-gpu-texture _node-id vector-payload vector-preview-shaders)))
  (output scene g/Any :cached
          (g/fnk [_node-id svg-image gpu-texture vector-payload]
            (let [width (.width ^SvgImporter$Image svg-image)
                  height (.height ^SvgImporter$Image svg-image)]
              (make-scene _node-id gpu-texture 0.0 0.0 width height width height (:paint vector-payload))))))

(defn register-resource-types [workspace]
  (code-resource/register-code-resource-type workspace
    :ext "svg"
    :label (localization/message "resource.type.svg")
    :icon "icons/32/Icons_25-AT-Image.png"
    :node-type SvgNode
    :view-types [:scene :code]
    :lazy-loaded false
    :editor-dependencies ["/builtins/materials/sprite-vector.vp" "/builtins/materials/sprite-vector.fp"]
    :additional-load-fn load-preview-shaders))
