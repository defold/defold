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

(ns editor.model-scene
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.attribute :as attribute]
            [editor.gl.light :as light]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.types :as gl.types]
            [editor.gltf :as gltf]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.model-loader :as model-loader]
            [editor.model-util :as model-util]
            [editor.outline :as outline]
            [editor.pose :as pose]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.shaders :as shaders]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.num :as num])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [java.nio ByteOrder FloatBuffer]
           [javax.vecmath Matrix4d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")
(def material-icon "icons/32/Icons_31-Material.png")
(def texture-icon "icons/32/Icons_25-AT-Image.png")
(def model-file-types ["gltf" "glb"])
(def animation-file-types ["animationset" "gltf" "glb"])

(defn- make-attribute-float-buffer
  ^FloatBuffer [input-floats input-component-count output-component-count output-component-fill]
  (let [input-float-count (count input-floats)
        input-component-count (int input-component-count)
        output-component-count (int output-component-count)]
    (assert (pos? input-float-count))
    (assert (zero? (rem input-float-count input-component-count)))
    (let [vector-count (quot input-float-count input-component-count)
          output-float-count (* vector-count output-component-count)]
      (if (= input-component-count output-component-count)
        (-> (float-array input-float-count input-floats)
            (buffers/wrap-float-array))
        (let [output-component-fill (float output-component-fill)
              output-float-buffer (buffers/new-float-buffer output-float-count :byte-order/native)]
          (loop [vector-index 0
                 input-component-index 0
                 output-component-index 0]
            (cond
              (= vector-count vector-index)
              (.flip output-float-buffer) ; We're done.

              (< output-component-index output-component-count)
              (let [output-float
                    (float
                      (if (< input-component-index input-component-count)
                        (let [input-float-index (+ input-component-index (* vector-index input-component-count))]
                          (nth input-floats input-float-index))
                        output-component-fill))]
                (.put output-float-buffer output-float)
                (recur vector-index
                       (inc input-component-index)
                       (inc output-component-index)))

              :else
              (recur (inc vector-index)
                     0
                     0))))))))

(defn- make-attribute-buffer
  ([mesh-request-id mesh input-floats-pb-field input-component-count]
   (make-attribute-buffer mesh-request-id mesh input-floats-pb-field input-component-count input-component-count 0.0))
  ([mesh-request-id mesh input-floats-pb-field input-component-count output-component-count output-component-fill]
   (let [input-floats (get mesh input-floats-pb-field)
         input-float-count (count input-floats)
         input-component-count (int input-component-count)]
     (cond
       (zero? input-float-count)
       nil ; We don't have any data for the attribute. Return nil.

       (zero? (rem input-float-count input-component-count))
       (let [float-buffer (make-attribute-float-buffer input-floats input-component-count output-component-count output-component-fill)
             buffer-data (buffers/make-buffer-data float-buffer)
             request-id (assoc mesh-request-id :pb-field input-floats-pb-field)
             vector-type (graphics.types/component-count-vector-type output-component-count false)]
         (attribute/make-attribute-buffer request-id buffer-data vector-type :static))

       :else
       (g/error-fatal
         "Attribute component count mismatch."
         {:input-component-count input-component-count
          :input-float-count input-float-count
          :input-floats-pb-field input-floats-pb-field
          :mesh-request-id mesh-request-id
          :mesh mesh})))))

(defn- make-index-buffer [mesh-request-id mesh indices-pb-field]
  (when-let [^ByteString indices-byte-string (get mesh indices-pb-field)]
    (let [source-byte-buffer (.order (.asReadOnlyByteBuffer indices-byte-string) ByteOrder/LITTLE_ENDIAN)
          indices-byte-size (buffers/item-count source-byte-buffer)
          indices-format (:indices-format mesh)]
      (when (pos? indices-byte-size)
        (if-let [indices-buffer
                 (case indices-format
                   :indexbuffer-format-16
                   (when (zero? (rem indices-byte-size Short/BYTES))
                     (-> (buffers/new-byte-buffer indices-byte-size :byte-order/native)
                         (.asShortBuffer)
                         (.put (.asShortBuffer source-byte-buffer))
                         (.flip)))

                   :indexbuffer-format-32
                   (when (zero? (rem indices-byte-size Integer/BYTES))
                     (-> (buffers/new-byte-buffer indices-byte-size :byte-order/native)
                         (.asIntBuffer)
                         (.put (.asIntBuffer source-byte-buffer))
                         (.flip))))]
          (let [request-id (assoc mesh-request-id :pb-field indices-pb-field)
                buffer-data (buffers/make-buffer-data indices-buffer)]
            (attribute/make-index-buffer request-id buffer-data :static))
          (g/error-fatal
            "Index byte size mismatch."
            {:indices-format indices-format
             :indices-byte-size indices-byte-size
             :indices-pb-field indices-pb-field
             :mesh-request-id mesh-request-id
             :mesh mesh}))))))

(defn- mesh->renderable-buffers [mesh mesh-request-id]
  (let [texcoord0-component-count (int (:num-texcoord0-components mesh 0))
        texcoord1-component-count (int (:num-texcoord1-components mesh 0))
        positions (make-attribute-buffer mesh-request-id mesh :positions 3 4 1.0)
        normals (make-attribute-buffer mesh-request-id mesh :normals 3 4 0.0)
        tangents (make-attribute-buffer mesh-request-id mesh :tangents 4)
        colors (make-attribute-buffer mesh-request-id mesh :colors 4)
        texcoord0s (make-attribute-buffer mesh-request-id mesh :texcoord0 texcoord0-component-count)
        texcoord1s (make-attribute-buffer mesh-request-id mesh :texcoord1 texcoord1-component-count)
        indices (make-index-buffer mesh-request-id mesh :indices)]
    (g/precluding-errors
      [positions normals tangents colors texcoord0s texcoord1s indices]
      (let [position-count (graphics.types/element-count positions)
            normal-count (graphics.types/element-count normals)
            tangent-count (graphics.types/element-count tangents)
            color-count (graphics.types/element-count colors)
            texcoord0-count (graphics.types/element-count texcoord0s)
            texcoord1-count (graphics.types/element-count texcoord1s)
            max-index (if (nil? indices)
                        -1
                        (->> indices
                             (graphics.types/buffer-data)
                             (transduce
                               (map (case (:indices-format mesh)
                                      :indexbuffer-format-16 num/ushort->long
                                      :indexbuffer-format-32 num/uint->long))
                               max
                               -1)
                             (long)))]
        (if (zero? position-count)
          (g/error-fatal
            "Position data missing."
            {:mesh-request-id mesh-request-id
             :mesh mesh})
          (g/precluding-errors
            (into (if (< max-index position-count)
                    []
                    [(g/error-fatal
                       "Index out of bounds."
                       {:position-count position-count
                        :max-index max-index
                        :mesh-request-id mesh-request-id
                        :mesh mesh})])
                  (keep (fn [[attribute-pb-field ^long attribute-count]]
                          (when (and (pos? attribute-count)
                                     (not= position-count attribute-count))
                            (g/error-fatal
                              (format "Attribute count mismatch: Expected %d %s but got %d."
                                      position-count (name attribute-pb-field) attribute-count)
                              {:position-count position-count
                               :attribute-count attribute-count
                               :attribute-pb-field attribute-pb-field
                               :mesh-request-id mesh-request-id
                               :mesh mesh}))))
                  [[:normals normal-count]
                   [:tangents tangent-count]
                   [:colors color-count]
                   [:texcoord0 texcoord0-count]
                   [:texcoord1 texcoord1-count]])
            (let [attribute-buffers
                  (cond-> {:semantic-type-position [positions]}

                          (pos? normal-count)
                          (assoc :semantic-type-normal [normals])

                          (pos? tangent-count)
                          (assoc :semantic-type-tangent [tangents])

                          (pos? color-count)
                          (assoc :semantic-type-color [colors])

                          (or (pos? texcoord0-count)
                              (pos? texcoord1-count))
                          (assoc :semantic-type-texcoord (cond-> [(when (pos? texcoord0-count) texcoord0s)]
                                                                 (pos? texcoord1-count) (conj texcoord1s))))]
              (cond-> {:attribute-buffers attribute-buffers}

                      (not (neg? max-index))
                      (assoc :index-buffer indices)))))))))

(defn- render-mesh-opaque [^GL2 gl render-args renderables]
  (let [renderable (first renderables)
        {:keys [attribute-bindings coordinate-space-info index-buffer material-data shader textures]} (:user-data renderable)
        render-args (math/rederive-render-transforms render-args coordinate-space-info)
        index-type (gl.types/element-buffer-gl-type index-buffer)
        index-count (graphics.types/element-count index-buffer)]
    (gl/with-gl-bindings gl render-args [shader attribute-bindings index-buffer]
      (doseq [[name t] textures]
        (gl/bind gl t render-args)
        (shader/set-samplers-by-name shader gl name (:texture-units t)))
      (doseq [[name v] material-data]
        (shader/set-uniform shader gl name v))
      (light/bind-preview-lights-for-shader! gl shader render-args)
      (gl/gl-disable gl GL/GL_BLEND)
      (gl/gl-enable gl GL/GL_CULL_FACE)
      (gl/gl-cull-face gl GL/GL_BACK)
      (gl/gl-draw-elements gl GL/GL_TRIANGLES index-type 0 index-count)
      (gl/gl-disable gl GL/GL_CULL_FACE)
      (gl/gl-enable gl GL/GL_BLEND)
      (doseq [[_name t] textures]
        (gl/unbind gl t render-args)))))

(defn- render-mesh-opaque-selection [^GL2 gl render-args renderables]
  ;; TODO(instancing): We should use instanced rendering and put the picking-id as a per-instance attribute.
  (let [{:keys [picking-id user-data]} (first renderables)
        {:keys [index-buffer textures]} user-data
        index-type (gl.types/element-buffer-gl-type index-buffer)
        index-count (graphics.types/element-count index-buffer)
        picking-id-float-array (scene-picking/picking-id->float-array picking-id)

        selection-attribute-bindings
        (-> (:selection-attribute-bindings user-data)
            (update :id-color graphics.types/with-value picking-id-float-array))]

    (gl/with-gl-bindings gl render-args [shaders/selection-instance-local-space selection-attribute-bindings index-buffer]
      (doseq [[name t] textures]
        (gl/bind gl t render-args)
        (shader/set-samplers-by-name shaders/selection-instance-local-space gl name (:texture-units t)))
      (gl/gl-disable gl GL/GL_BLEND)
      (gl/gl-enable gl GL/GL_CULL_FACE)
      (gl/gl-cull-face gl GL/GL_BACK)
      (gl/gl-draw-elements gl GL/GL_TRIANGLES index-type 0 index-count)
      (gl/gl-disable gl GL/GL_CULL_FACE)
      (gl/gl-enable gl GL/GL_BLEND)
      (doseq [[_name t] textures]
        (gl/unbind gl t render-args)))))

(defn- render-mesh [^GL2 gl render-args renderables rcount]
  ;; TODO(instancing): Batch instanced meshes together and populate an instance-buffer with the per-instance attributes.
  (assert (= 1 rcount) "Batching is disabled in the editor for simplicity.")
  (condp = (:pass render-args)
    pass/opaque
    (render-mesh-opaque gl render-args renderables)

    pass/opaque-selection
    (render-mesh-opaque-selection gl render-args renderables)))

(g/defnk produce-mesh-set-build-target [_node-id resource content]
  (rig/make-mesh-set-build-target (resource/workspace resource)
                                  _node-id
                                  (assoc (:mesh-set content) :raw-models [])
                                  (:morph-target-textures content)))

(g/defnk produce-skeleton [content]
  (:skeleton content))

(g/defnk produce-skeleton-build-target [_node-id resource skeleton]
  (rig/make-skeleton-build-target (resource/workspace resource) _node-id skeleton))

(g/defnk produce-bones [content]
  (:bones content))

(g/defnk produce-content [_node-id resource project-settings external-buffer-sha256s]
  (g/precluding-errors external-buffer-sha256s
    (model-loader/load-scene _node-id resource project-settings)))

(g/defnk produce-animation-info [resource]
  [{:path (resource/proj-path resource) :parent-id "" :resource resource}])

(g/defnk produce-animation-ids [content]
  (:animation-ids content))

(g/defnk produce-collision-meshes [content]
  (:collision-meshes content))

(g/defnk produce-collision-mesh-set [content]
  (:mesh-set content))

(g/defnk produce-collision-mesh-renderables [renderable-mesh-set]
  (mapv (fn [{:keys [aabb mesh-index renderable-meshes]}]
          {:aabb aabb
           :index mesh-index
           :primitives
           (mapv (fn [{:keys [aabb renderable-buffers]}]
                   {:aabb aabb
                    :index-buffer (:index-buffer renderable-buffers)
                    :position-buffer (nth (get-in renderable-buffers [:attribute-buffers :semantic-type-position]) 0)})
                 renderable-meshes)})
        (:renderable-raw-models renderable-mesh-set)))

(def ^:private default-material-ids ["default"])

(g/defnk produce-material-ids [content]
  (let [ret (:material-ids content)]
    (if (zero? (count ret))
      default-material-ids
      ret)))

(g/defnk produce-gpu-textures
  "Builds sampler texture lifecycles, supplying fallback textures for missing or failed bindings."
  [_node-id samplers texture-binding-infos]
  (let [sampler-name->gpu-texture-generator
        (into {}
              (keep (fn [{:keys [sampler gpu-texture-generator]}]
                      (when gpu-texture-generator
                        [sampler gpu-texture-generator])))
              texture-binding-infos)

        explicit-texture-work
        (into []
              (keep-indexed
                (fn [unit-index {:keys [name] :as sampler}]
                  (when-let [gpu-texture-generator (sampler-name->gpu-texture-generator name)]
                    [unit-index sampler gpu-texture-generator])))
              samplers)

        ;; This generates CPU-side texture request data. GL upload happens later when the texture lifecycle is bound.
        explicit-textures
        (into {}
              (coll/pmapv
                (fn [[unit-index {:keys [name] :as sampler} gpu-texture-generator]]
                  (let [gpu-texture (texture-util/generate-gpu-texture gpu-texture-generator)]
                    [name
                     (-> (if (g/error-value? gpu-texture)
                           @texture/placeholder
                           gpu-texture)
                         (texture/set-params (material/sampler->tex-params sampler))
                         (texture/set-base-unit unit-index))]))
                explicit-texture-work))

        fallback-texture (if (pos? (count explicit-textures))
                           (val (first explicit-textures))
                           @texture/black-pixel)]
    (reduce
      (fn [textures-by-sampler-name {:keys [name]}]
        (cond-> textures-by-sampler-name
          (not (textures-by-sampler-name name))
          (assoc name fallback-texture)))
      explicit-textures
      samplers)))

(defn- make-renderable-material-data [mesh-material-data]
  (when mesh-material-data
    (let [pbr-metallic-roughness (:pbr-metallic-roughness mesh-material-data)
          pbr-specular-glossiness (:pbr-specular-glossiness mesh-material-data)
          pbr-clear-coat (:clearcoat mesh-material-data)
          pbr-transmission (:transmission mesh-material-data)
          pbr-ior (:ior mesh-material-data)
          pbr-specular (:specular mesh-material-data)
          pbr-volume (:volume mesh-material-data)
          pbr-sheen (:sheen mesh-material-data)
          pbr-emissive-strength (:emissive-strength mesh-material-data)
          pbr-iridescence (:iridescence mesh-material-data)
          pbr-texture-index->value (fn ^double [^long ix]
                                     (if (>= ix 0) 1.0 0.0))]
      [;; Common properties
       ["pbrAlphaCutoffAndDoubleSidedAndIsUnlit"
        (Vector4d. (:alpha-cutoff mesh-material-data)
                   (if (:double-sided mesh-material-data) 1.0 0.0)
                   (if (:unlit mesh-material-data) 1.0 0.0)
                   0.0)]
       ["pbrCommonTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in mesh-material-data [:normal-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in mesh-material-data [:occlusion-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in mesh-material-data [:emissive-texture :texture :index]))
                   0)]

       ;; Metallic roughness
       ["pbrMetallicRoughness.baseColorFactor"
        (doto (Vector4d.)
          (math/clj->vecmath (:base-color-factor pbr-metallic-roughness)))]
       ["pbrMetallicRoughness.metallicAndRoughnessFactor"
        (Vector4d. (:metallic-factor pbr-metallic-roughness)
                   (:roughness-factor pbr-metallic-roughness)
                   0 0)]
       ["pbrMetallicRoughness.metallicRoughnessTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-metallic-roughness [:base-color-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-metallic-roughness [:metallic-roughness-texture :texture :index]))
                   0 0)]

       ;; Specular glossiness
       ["pbrSpecularGlossiness.diffuseFactor"
        (doto (Vector4d.)
          (math/clj->vecmath (:diffuse-factor pbr-specular-glossiness)))]
       ["pbrSpecularGlossiness.specularAndSpecularGlossinessFactor"
        (doto (Vector4d.)
          (math/clj->vecmath (conj
                               (:specular-factor pbr-specular-glossiness)
                               (:glossiness-factor pbr-specular-glossiness))))]
       ["pbrSpecularGlossiness.specularGlossinessTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-specular-glossiness [:diffuse-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-specular-glossiness [:specular-glossiness-texture :texture :index]))
                   0 0)]

       ;; Clearcoat
       ["pbrClearCoat.clearCoatAndClearCoatRoughnessFactor"
        (Vector4d. (:clearcoat-factor pbr-clear-coat)
                   (:clearcoat-roughness-factor pbr-clear-coat)
                   0 0)]
       ["pbrClearCoat.clearCoatTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-clear-coat [:clearcoat-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-clear-coat [:clearcoat-roughness-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-clear-coat [:clearcoat-normal-texture :texture :index]))
                   0)]

       ;; Transmission
       ["pbrTransmission.transmissionFactor"
        (Vector4d. (:transmission-factor pbr-transmission) 0 0 0)]
       ["pbrTransmission.transmissionTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-transmission [:transmission-texture :texture :index]))
                   0 0 0)]

       ;; Ior
       ["pbrIor.ior"
        (Vector4d. (:ior pbr-ior) 0 0 0)]

       ;; Specular
       ["pbrSpecular.specularColorAndSpecularFactor"
        (doto (Vector4d.)
          (math/clj->vecmath (conj
                               (:specular-color-factor pbr-specular)
                               (:specular-factor pbr-specular))))]
       ["pbrSpecular.specularTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-specular [:specular-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-specular [:specular-color-texture :texture :index]))
                   0 0)]

       ;; Volume
       ["pbrVolume.thicknessFactorAndAttenuationColor"
        (doto (Vector4d.)
          (math/clj->vecmath (into
                               [(:thickness-factor pbr-volume)]
                               (:attenuation-color pbr-volume))))]
       ["pbrVolume.attenuationDistance"
        (Vector4d. (:attenuation-distance pbr-volume) 0 0 0)]
       ["pbrVolume.volumeTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-volume [:thickness-texture :texture :index]))
                   0 0 0)]

       ;; Sheen
       ["sheenColorAndRoughnessFactor"
        (doto (Vector4d.)
          (math/clj->vecmath (conj
                               (:sheen-color-factor pbr-sheen)
                               (:sheen-roughness-factor pbr-sheen))))]
       ["sheenTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-sheen [:sheen-color-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-sheen [:sheen-roughness-texture :texture :index]))
                   0 0)]

       ;; Emissive strength
       ["pbrEmissiveStrength.emissiveStrength"
        (Vector4d. (:emissive-strength pbr-emissive-strength) 0 0 0)]

       ;; Iridescence
       ["iridescenceFactorAndIorAndThicknessMinMax"
        (Vector4d. (:iridescence-factor pbr-iridescence)
                   (:iridescence-ior pbr-iridescence)
                   (:iridescence-thickness-min pbr-iridescence)
                   (:iridescence-thickness-max pbr-iridescence))]
       ["iridescenceTextures"
        (Vector4d. (pbr-texture-index->value
                     (get-in pbr-iridescence [:iridescence-texture :texture :index]))
                   (pbr-texture-index->value
                     (get-in pbr-iridescence [:iridescence-thickness-texture :texture :index]))
                   0 0)]])))

(defn- make-renderable-mesh [mesh mesh-request-id mesh-set mesh-material-index->material-name]
  (let [renderable-buffers (mesh->renderable-buffers mesh mesh-request-id)]
    (if (g/error-value? renderable-buffers)
      renderable-buffers
      (let [{:keys [aabb-min aabb-max ^int material-index]} mesh
            aabb (geom/coords->aabb aabb-min aabb-max)
            material-name (mesh-material-index->material-name material-index)
            ;; TODO(instancing): These doesn't appear to actually be per-mesh? Replace model-loader :material-ids with list of Rig$Material in map format.
            mesh-material-data (nth (:materials mesh-set) material-index)
            material-data (make-renderable-material-data mesh-material-data)]
        {:aabb aabb
         :material-index material-index
         :material-name material-name
         :material-data material-data
         :renderable-buffers renderable-buffers}))))

(defn- make-bone-id->world-transform [skeleton]
  (into {}
        (map (fn [bone]
               [(:id bone)
                (let [{:keys [translation rotation scale]} (:world bone)]
                  (math/clj->mat4 translation rotation scale))]))
        (:bones skeleton)))

(defn- make-renderable-model [model model-request-id mesh-set mesh-material-index->material-name bone-id->world-transform]
  (let [{:keys [translation rotation scale]} (:local model)
        local-transform (math/clj->mat4 translation rotation scale)
        bone-transform (get bone-id->world-transform (:bone-id model))
        model-transform (if (some? bone-transform)
                          (doto (Matrix4d. ^Matrix4d bone-transform)
                            (.mul ^Matrix4d local-transform))
                          local-transform)
        pose-without-skeleton (pose/make translation rotation scale)
        pose-with-skeleton (if (some? bone-transform)
                             (pose/from-matrix model-transform)
                             pose-without-skeleton)

        renderable-meshes
        (coll/into-> (:meshes model) []
          (map-indexed
            (fn [mesh-index mesh]
              (let [mesh-request-id (assoc model-request-id :mesh-index mesh-index)]
                (make-renderable-mesh mesh mesh-request-id mesh-set mesh-material-index->material-name)))))]

    (g/precluding-errors renderable-meshes
      (let [model-aabb (transduce
                         (map :aabb)
                         geom/aabb-union
                         geom/null-aabb
                         renderable-meshes)]
        {:pose-with-skeleton pose-with-skeleton
         :pose-without-skeleton pose-without-skeleton
         :mesh-index (:mesh-index model)
         :aabb model-aabb
         :renderable-meshes renderable-meshes}))))

(defn- make-renderable-models
  [models request-id-key model-key mesh-set-request-id mesh-set mesh-material-index->material-name bone-id->world-transform]
  (mapv (fn [model]
          (let [model-request-id (assoc mesh-set-request-id request-id-key (model-key model))]
            (make-renderable-model model model-request-id mesh-set mesh-material-index->material-name bone-id->world-transform)))
        models))

(defn- make-renderable-mesh-set [mesh-set skeleton mesh-set-request-id mesh-material-index->material-name]
  (let [bone-id->world-transform (make-bone-id->world-transform skeleton)
        renderable-models
        (make-renderable-models (:models mesh-set) :model-id :id mesh-set-request-id mesh-set mesh-material-index->material-name bone-id->world-transform)

        initial-renderable-raw-models
        (make-renderable-models (:raw-models mesh-set) :raw-mesh-index :mesh-index mesh-set-request-id mesh-set mesh-material-index->material-name bone-id->world-transform)]

    (g/precluding-errors (into renderable-models initial-renderable-raw-models)
      (let [renderable-model-by-mesh-index (coll/pair-map-by :mesh-index renderable-models)
            renderable-raw-models
            (mapv (fn [renderable-raw-model]
                    (if (coll/not-empty (:renderable-meshes renderable-raw-model))
                      renderable-raw-model
                      (if-let [renderable-model (renderable-model-by-mesh-index (:mesh-index renderable-raw-model))]
                        (assoc renderable-raw-model
                          :aabb (:aabb renderable-model)
                          :renderable-meshes (:renderable-meshes renderable-model))
                        renderable-raw-model)))
                  initial-renderable-raw-models)
            mesh-set-aabb (transduce
                            (map (fn [{:keys [aabb pose-with-skeleton]}]
                                   (geom/aabb-transform aabb (pose/matrix pose-with-skeleton))))
                            geom/aabb-union
                            geom/null-aabb
                            renderable-models)]
        {:aabb mesh-set-aabb
         :renderable-models renderable-models
         :renderable-raw-models renderable-raw-models}))))

(g/defnk produce-renderable-mesh-set [_node-id content]
  (let [mesh-set-request-id
        {:request-type :ModelSceneNode/mesh-set
         :scene-node-id _node-id}

        mesh-material-index->material-name
        (or (some-> content :material-ids not-empty vec)
            default-material-ids)

        renderable-mesh-set-or-error-value
        (make-renderable-mesh-set (:mesh-set content) (:skeleton content) mesh-set-request-id mesh-material-index->material-name)]

    (if (g/error-value? renderable-mesh-set-or-error-value)
      (assoc renderable-mesh-set-or-error-value :_node-id _node-id :_label :renderable-mesh-set)
      renderable-mesh-set-or-error-value)))

(def ^:private model-aabb-outline-renderable
  (render-util/make-aabb-outline-renderable #{:model}))

(defn- render-selected-model-aabb-outline
  "Draws mesh bounding boxes only when their own outline items are selected."
  [gl render-args renderables _renderable-count]
  (let [selected-renderables (coll/filterv-> renderables #(= :self-selected (:selected %)))]
    (when (coll/not-empty selected-renderables)
      ((:render-fn model-aabb-outline-renderable)
       gl render-args selected-renderables (count selected-renderables)))))

(def ^:private selected-model-aabb-outline-renderable
  (assoc model-aabb-outline-renderable
    :batch-key ::selected-model-aabb-outline
    :render-fn render-selected-model-aabb-outline))

(defn- make-mesh-scene [scene-node-id renderable-mesh]
  {:pre [(g/node-id? scene-node-id)]}
  (let [{:keys [aabb material-data material-index material-name renderable-buffers]} renderable-mesh
        index-buffer (:index-buffer renderable-buffers)
        semantic-type->attribute-buffers (:attribute-buffers renderable-buffers)
        attribute-reflection-infos (shader/attribute-reflection-infos shaders/mesh-preview-local-space nil)
        coordinate-space-info (graphics/coordinate-space-info attribute-reflection-infos)
        attribute-bindings (model-util/make-attribute-bindings scene-node-id attribute-reflection-infos semantic-type->attribute-buffers {})
        selection-attribute-reflection-infos (shader/attribute-reflection-infos shaders/selection-instance-local-space nil)
        selection-attribute-bindings (model-util/make-attribute-bindings scene-node-id selection-attribute-reflection-infos semantic-type->attribute-buffers {})

        user-data
        {:attribute-bindings attribute-bindings
         :coordinate-space-info coordinate-space-info
         :index-buffer index-buffer
         :material-data material-data
         :material-index material-index
         :material-name material-name
         :mesh-renderable-buffers renderable-buffers
         :selection-attribute-bindings selection-attribute-bindings
         :shader shaders/mesh-preview-local-space}

        renderable
        {:render-fn render-mesh
         :tags #{:model}
         :batch-key nil ; Batching is disabled in the editor for simplicity.
         :select-batch-key nil
         :passes [pass/opaque pass/opaque-selection]
         :user-data user-data}]

    {:node-id scene-node-id
     :aabb aabb
     :renderable renderable}))

(defn- make-model-scene [renderable-model mesh-scene-info-by-index]
  (let [{:keys [pose-with-skeleton pose-without-skeleton mesh-index aabb renderable-meshes]} renderable-model
        {:keys [node-id node-outline-key]} (mesh-scene-info-by-index mesh-index)
        mesh-scenes (mapv #(make-mesh-scene node-id %)
                          renderable-meshes)]
    {:node-id node-id
     :node-outline-key node-outline-key
     :pose pose-with-skeleton
     :pose-with-skeleton pose-with-skeleton
     :pose-without-skeleton pose-without-skeleton
     :mesh-index mesh-index
     :aabb aabb
     :renderable selected-model-aabb-outline-renderable
     :children mesh-scenes}))

(defn- make-scene [scene-node-id renderable-mesh-set mesh-scene-infos]
  (let [{:keys [aabb renderable-models renderable-raw-models]} renderable-mesh-set
        mesh-scene-info-by-index (coll/pair-map-by :mesh-index mesh-scene-infos)

        child-scenes
        (into [{:node-id scene-node-id
                :aabb aabb
                :renderable model-aabb-outline-renderable}]
              (map #(make-model-scene % mesh-scene-info-by-index))
              renderable-models)]

    {:node-id scene-node-id
     :aabb aabb
     :raw-model-scenes (mapv #(make-model-scene % mesh-scene-info-by-index)
                             renderable-raw-models)
     :renderable {:tags #{:model}
                  :batch-key nil ; Batching is disabled in the editor for simplicity.
                  :passes [pass/opaque-selection]} ; A selection pass to ensure it can be selected and manipulated.
     :children child-scenes}))

(g/defnk produce-source-scene
  "Builds the scene used by model resources before glTF preview materials are applied."
  [_node-id mesh-scene-infos renderable-mesh-set]
  (make-scene _node-id renderable-mesh-set mesh-scene-infos))

(defn- finalize-claim-scene [scene _old-node-id new-node-id]
  (update scene :children coll/mapv->
          update :children coll/mapv->
          update-in [:renderable :user-data :attribute-bindings]
          attribute/claim-transformed-attribute-buffer-bindings
          assoc :scene-node-id new-node-id))

(defn- apply-material-scene-info
  "Applies material render data to a mesh scene, preserving it when no material info is supplied."
  [mesh-scene scene-node-id material-scene-info]
  (if (nil? material-scene-info)
    mesh-scene
    (let [{:keys [gpu-textures material-attribute-infos shader vertex-attribute-bytes vertex-space]} material-scene-info
          material-data (get-in mesh-scene [:renderable :user-data :material-data])
          shader-attribute-reflection-infos (shader/attribute-reflection-infos shader nil)
          default-coordinate-space (case vertex-space
                                     :vertex-space-local :coordinate-space-local
                                     :vertex-space-world :coordinate-space-world)]
      (assert (map? gpu-textures))
      (assert (coll/every? (fn [[sampler-name _]]
                             (string? sampler-name))
                           gpu-textures))
      (assert (coll/every? (fn [[_ gpu-texture]]
                             (texture/texture-lifecycle? gpu-texture))
                           gpu-textures))
      (assert (coll/every? map? material-attribute-infos))
      (assert (coll/every? (comp keyword? :name-key) material-attribute-infos))
      (assert (shader/shader-lifecycle? shader))
      (assert (coll/every? (fn [[attribute-key _]]
                             (keyword? attribute-key))
                           vertex-attribute-bytes))
      (assert (coll/every? (fn [[_ attribute-bytes]]
                             (bytes? attribute-bytes))
                           vertex-attribute-bytes))
      (update mesh-scene :renderable
              update :user-data
              (fn [user-data]
                (let [mesh-renderable-buffers (:mesh-renderable-buffers user-data)
                      semantic-type->attribute-buffers (:attribute-buffers mesh-renderable-buffers)
                      combined-attribute-infos (graphics/combined-attribute-infos shader-attribute-reflection-infos material-attribute-infos default-coordinate-space)
                      coordinate-space-info (graphics/coordinate-space-info combined-attribute-infos)
                      attribute-bindings (model-util/make-attribute-bindings scene-node-id combined-attribute-infos semantic-type->attribute-buffers vertex-attribute-bytes)]
                  (assoc user-data
                    :attribute-bindings attribute-bindings
                    :coordinate-space-info coordinate-space-info
                    :material-attribute-infos material-attribute-infos
                    :material-data material-data
                    :shader shader
                    :textures gpu-textures)))))))

(defn- apply-preview-materials-to-model-scene
  "Applies preview materials to a model's mesh scenes using material indices."
  [model-scene scene-node-id material-index->material-scene-info]
  (if-let [mesh-scenes (:children model-scene)]
    (assoc model-scene
      :children
      (mapv
        (fn [mesh-scene]
          (let [material-index (get-in mesh-scene [:renderable :user-data :material-index])]
            (apply-material-scene-info mesh-scene scene-node-id (material-index->material-scene-info material-index))))
        mesh-scenes))
    model-scene))

(g/defnk produce-scene
  "Applies glTF preview materials to both instantiated and raw model scenes."
  [_node-id source-scene material-scene-infos]
  (let [material-index->material-scene-info
        (into {}
              (comp (keep identity)
                    (coll/pair-map-by :material-index))
              material-scene-infos)

        apply-preview-materials
        (fn [model-scenes]
          (mapv #(apply-preview-materials-to-model-scene % _node-id material-index->material-scene-info)
                model-scenes))]
    (-> source-scene
        (update :children apply-preview-materials)
        (update :raw-model-scenes apply-preview-materials))))

(defn- augment-mesh-scene [mesh-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info]
  (let [material-name (get-in mesh-scene [:renderable :user-data :material-name])
        material-scene-info (material-name->material-scene-info material-name)
        claimed-scene (scene/claim-child-scene mesh-scene old-node-id new-node-id new-node-outline-key)]
    (apply-material-scene-info claimed-scene new-node-id material-scene-info)))

(defn- augment-model-scene [model-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info use-skeleton-transforms]
  (let [model-scene (assoc model-scene
                      :pose (if use-skeleton-transforms
                              (:pose-with-skeleton model-scene)
                              (:pose-without-skeleton model-scene)))
        mesh-scenes (:children model-scene)]
    (assoc (scene/claim-child-scene model-scene old-node-id new-node-id new-node-outline-key)
      :children (mapv #(augment-mesh-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info)
                      mesh-scenes))))

(defn- model-scenes-aabb [model-scenes]
  (transduce
    (keep (fn [{:keys [aabb pose]}]
            (when pose
              (geom/aabb-transform aabb (pose/matrix pose)))))
    geom/aabb-union
    geom/null-aabb
    model-scenes))

(defn augment-scene [scene new-node-id new-node-outline-key material-name->material-scene-info use-skeleton-transforms selected-mesh-index]
  (if (g/error-value? scene)
    scene
    (let [old-node-id (:node-id scene)
          model-scenes (cond
                         (= -1 selected-mesh-index)
                         (:children scene)

                         selected-mesh-index
                         (into [(nth (:children scene) 0)]
                               (filter #(= selected-mesh-index (:mesh-index %)))
                               (:raw-model-scenes scene))

                         :else
                         [(nth (:children scene) 0)])
          augmented-model-scenes (mapv #(augment-model-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info use-skeleton-transforms)
                                       model-scenes)
          scene-aabb (when (or (not use-skeleton-transforms)
                               (not= -1 selected-mesh-index))
                       (model-scenes-aabb augmented-model-scenes))
          augmented-model-scenes (cond-> augmented-model-scenes
                                   (and scene-aabb (seq augmented-model-scenes))
                                   (assoc-in [0 :aabb] scene-aabb))]
      (cond-> (assoc scene
        :node-id new-node-id
        :node-outline-key new-node-outline-key
        :finalize-claim-fn finalize-claim-scene ; We may have one or more TransformedAttributeBufferLifecycles after this, so we must assign them unique request-ids per instance.
        :children augmented-model-scenes)
        scene-aabb (assoc :aabb scene-aabb)))))

(defn make-material-name->material-scene-info
  "Given some material-scene-infos, return a material-name->material-scene-info
  fn suitable for use with the augment-scene function."
  [material-scene-infos]
  (let [;; When augmenting the scene, we only want to use material-scene-infos
        ;; that are fully formed, and ignore the others.
        usable-material-scene-infos
        (filterv
          (fn [material-scene-info]
            (and (some? (:shader material-scene-info))
                 (some? (:vertex-space material-scene-info))))
          material-scene-infos)

        ;; If we have no material associated with the index, we mirror the
        ;; engine behavior by picking the first one:
        ;; https://github.com/defold/defold/blob/a265a1714dc892eea285d54eae61d0846b48899d/engine/gamesys/src/gamesys/resources/res_model.cpp#L234-L238
        fallback-material-scene-info
        (first usable-material-scene-infos)

        usable-material-scene-infos-by-material-name
        (->> usable-material-scene-infos
             (coll/pair-map-by :name)
             (coll/not-empty))]

    (fn material-name->material-scene-info [^String material-name]
      (get usable-material-scene-infos-by-material-name material-name fallback-material-scene-info))))

(defn- gltf-metadata-group-presentation
  "Returns the localized label, icon and ordering for a glTF metadata group."
  [kind]
  (case kind
    :materials {:icon material-icon
                :label (localization/message "outline.gltf.materials")
                :order 1}
    :meshes {:icon mesh-icon
             :label (localization/message "outline.gltf.meshes")
             :order 0}
    :textures {:icon texture-icon
               :label (localization/message "outline.gltf.textures")
               :order 2}))

(g/defnode GltfMetadataGroupNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)

  (property kind g/Keyword
            (dynamic visible (g/constantly false)))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id child-outlines kind]
            (let [{:keys [icon label order]} (gltf-metadata-group-presentation kind)]
              {:node-id _node-id
               :node-outline-key (str "gltf-" (name kind))
               :label label
               :icon icon
               :order order
               :read-only true
               :children (vec (sort-by :order child-outlines))}))))

(g/defnode GltfMeshInfoNode
  (inherits outline/OutlineNode)

  (property outline-label g/Str
            (dynamic visible (g/constantly false)))
  (property index g/Int
            (dynamic read-only? (g/constantly true)))
  (property name g/Str
            (dynamic read-only? (g/constantly true)))
  (property name-generated g/Bool
            (dynamic read-only? (g/constantly true)))
  (property primitive-count g/Int
            (dynamic read-only? (g/constantly true)))
  (property vertex-count g/Int
            (dynamic read-only? (g/constantly true)))

  (display-order [:index :name :name-generated :primitive-count :vertex-count])

  (output mesh-scene-info g/Any
          (g/fnk [_node-id index]
            {:mesh-index index
             :node-id _node-id
             :node-outline-key (format "gltf-mesh-%d" index)}))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id index outline-label]
            {:node-id _node-id
             :node-outline-key (format "gltf-mesh-%d" index)
             :label outline-label
             :icon mesh-icon
             :order index
             :read-only true})))

(g/defnode GltfMaterialInfoNode
  (inherits outline/OutlineNode)

  (property outline-label g/Str
            (dynamic visible (g/constantly false)))
  (property index g/Int
            (dynamic read-only? (g/constantly true)))
  (property name g/Str
            (dynamic read-only? (g/constantly true)))
  (property material resource/Resource
            (dynamic read-only? (g/constantly true)))
  (property samplers g/Str
            (dynamic read-only? (g/constantly true))
            (dynamic visible (g/fnk [samplers] (not (string/blank? samplers)))))

  (display-order [:index :name :material :samplers])

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id index material outline-label]
            {:node-id _node-id
             :node-outline-key (format "gltf-material-%d" index)
             :label outline-label
             :icon material-icon
             :order index
             :read-only true
             :link material
             :outline-show-link? true})))

(g/defnode GltfTextureInfoNode
  (inherits outline/OutlineNode)

  (property outline-label g/Str
            (dynamic visible (g/constantly false)))
  (property index g/Int
            (dynamic read-only? (g/constantly true)))
  (property name g/Str
            (dynamic read-only? (g/constantly true)))
  (property image resource/Resource
            (dynamic read-only? (g/constantly true)))
  (property image-index g/Int
            (dynamic read-only? (g/constantly true)))
  (property image-name g/Str
            (dynamic read-only? (g/constantly true)))
  (property uri g/Str
            (dynamic read-only? (g/constantly true))
            (dynamic visible (g/fnk [uri] (not (string/blank? uri)))))
  (property mime-type g/Str
            (dynamic read-only? (g/constantly true)))
  (property source-kind g/Str
            (dynamic read-only? (g/constantly true)))
  (property sampler-index g/Int
            (dynamic read-only? (g/constantly true)))
  (property min-filter g/Int
            (dynamic read-only? (g/constantly true)))
  (property mag-filter g/Int
            (dynamic read-only? (g/constantly true)))
  (property wrap-s g/Int
            (dynamic read-only? (g/constantly true)))
  (property wrap-t g/Int
            (dynamic read-only? (g/constantly true)))
  (property basisu g/Bool
            (dynamic read-only? (g/constantly true)))

  (display-order [:index :name :image :image-index :image-name :uri :mime-type :source-kind
                  :sampler-index :min-filter :mag-filter :wrap-s :wrap-t :basisu])

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id image index outline-label]
            {:node-id _node-id
             :node-outline-key (format "gltf-texture-%d" index)
             :label outline-label
             :icon texture-icon
             :order index
             :read-only true
             :link image
             :outline-show-link? true})))

(defn- add-gltf-outline-labels
  "Adds outline labels, appending asset indices to disambiguate duplicate names."
  [descriptors]
  (let [base-labels (mapv :name descriptors)
        label-counts (frequencies base-labels)]
    (mapv
      (fn [{:keys [index] :as descriptor} base-label]
        (assoc descriptor
          :outline-label (if (= 1 (get label-counts base-label))
                           base-label
                           (format "%s [%d]" base-label index))))
      descriptors
      base-labels)))

(defn- create-gltf-metadata-item-tx
  "Creates a metadata item and optionally connects its mesh selection information."
  [group-node model-scene-node node-type properties scene-info-output-label]
  (g/make-nodes (g/node-id->graph-id group-node)
    [item-node [node-type properties]]
    (g/connect item-node :_node-id group-node :nodes)
    (g/connect item-node :node-outline group-node :child-outlines)
    (if-not scene-info-output-label
      []
      (g/connect item-node scene-info-output-label model-scene-node :mesh-scene-infos))))

(defn- create-gltf-metadata-group-tx
  "Creates a glTF outline group for non-empty descriptors."
  [model-scene-node kind node-type descriptors property-keys scene-info-output-label]
  (when-not (coll/empty? descriptors)
    (g/make-nodes (g/node-id->graph-id model-scene-node)
      [group-node [GltfMetadataGroupNode :kind kind]]
      (g/connect group-node :_node-id model-scene-node :nodes)
      (g/connect group-node :node-outline model-scene-node :child-outlines)
      (into []
            (map
              (fn [descriptor]
                (create-gltf-metadata-item-tx
                  group-node model-scene-node node-type
                  (select-keys descriptor property-keys)
                  scene-info-output-label)))
            (add-gltf-outline-labels descriptors)))))

(g/defnode GltfPreviewTextureBinding
  (property sampler g/Str)
  (property texture resource/Resource
            (value (gu/passthrough texture-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :texture-resource]
                                            [:gpu-texture-generator :gpu-texture-generator]))))

  (input texture-resource resource/Resource)
  (input gpu-texture-generator g/Any)

  (output texture-binding-info g/Any
          (g/fnk [sampler ^:try gpu-texture-generator :as info]
            (cond-> info
              (g/error-value? gpu-texture-generator)
              (dissoc :gpu-texture-generator)))))

(g/defnode GltfPreviewMaterialBinding
  (inherits core/Scope)

  (property name g/Str)
  (property material-index g/Num)
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :samplers]
                                            [:shader :shader]
                                            [:attribute-infos :material-attribute-infos]
                                            [:vertex-space :vertex-space]))))

  (input material-resource resource/Resource)
  (input material-attribute-infos g/Any)
  (input samplers g/Any)
  (input shader g/Any)
  (input texture-binding-infos g/Any :array)
  (input vertex-space g/Any)

  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output material-scene-info g/Any
          (g/fnk [material-index
                  name
                  ^:try gpu-textures
                  ^:try material-attribute-infos
                  ^:try shader
                  ^:try vertex-space]
            (when (coll/every? #(and % (not (g/error-value? %)))
                              [material-index gpu-textures material-attribute-infos shader vertex-space])
              {:gpu-textures gpu-textures
               :material-attribute-infos material-attribute-infos
               :material-index material-index
               :name name
               :shader shader
               :vertex-attribute-bytes {}
               :vertex-space vertex-space}))))

(defn- create-gltf-preview-texture-binding-tx
  "Connects a texture resource to a glTF preview material sampler."
  [material-binding {:keys [sampler texture]}]
  (g/make-nodes (g/node-id->graph-id material-binding)
    [texture-binding [GltfPreviewTextureBinding
                      :sampler sampler
                      :texture texture]]
    (g/connect texture-binding :_node-id material-binding :nodes)
    (g/connect texture-binding :texture-binding-info material-binding :texture-binding-infos)))

(defn- create-gltf-preview-material-binding-tx
  "Creates a material binding and its texture bindings for the glTF scene preview."
  [model-scene-node {:keys [material material-index name textures]}]
  (g/make-nodes (g/node-id->graph-id model-scene-node)
    [material-binding [GltfPreviewMaterialBinding
                       :material material
                       :material-index material-index
                       :name name]]
    (g/connect material-binding :_node-id model-scene-node :nodes)
    (g/connect material-binding :material-scene-info model-scene-node :material-scene-infos)
    (into []
          (map #(create-gltf-preview-texture-binding-tx material-binding %))
          textures)))

(defn- set-external-buffer-resources [evaluation-context self old-value new-value]
  (let [disconnect-tx-data
        (into []
              (mapcat #(project/resource-setter evaluation-context self % nil
                                                [:sha256 :external-buffer-sha256s]))
              old-value)]
    (into disconnect-tx-data
          (mapcat #(project/resource-setter evaluation-context self nil %
                                            [:sha256 :external-buffer-sha256s]))
          new-value)))

(defn load-model-scene-node [project self resource external-buffer-uris]
  (let [basis (g/now)
        workspace (resource/workspace resource)
        source-path (resource/path resource)
        external-buffer-resources
        (into []
              (keep (fn [uri]
                      (when-let [proj-path (gltf/uri->proj-path source-path uri)]
                        (workspace/resolve-workspace-resource basis workspace proj-path))))
              external-buffer-uris)
        initial-tx-data
        (into (g/connect project :settings self :project-settings)
              (g/set-property self :external-buffer-resources external-buffer-resources))
        preview-tx-data
        (into initial-tx-data
              (mapcat #(create-gltf-preview-material-binding-tx self %))
              (gltf/material-binding-descriptors resource nil))
        {:keys [materials meshes textures]} (gltf/metadata-descriptors resource)]
    (into preview-tx-data
          (comp (keep identity) cat)
          [(create-gltf-metadata-group-tx
             self :meshes GltfMeshInfoNode meshes
             [:index :name :name-generated :outline-label :primitive-count :vertex-count]
             :mesh-scene-info)
           (create-gltf-metadata-group-tx
             self :materials GltfMaterialInfoNode materials
             [:index :material :name :outline-label :samplers]
             nil)
           (create-gltf-metadata-group-tx
             self :textures GltfTextureInfoNode textures
             [:basisu :image :image-index :image-name :index :mag-filter :mime-type :min-filter
              :name :outline-label :sampler-index :source-kind :uri :wrap-s :wrap-t]
             nil)])))

(g/defnode ModelSceneNode
  (inherits resource-node/ResourceNode)

  (property external-buffer-resources g/Any
            (default [])
            (set set-external-buffer-resources)
            (dynamic visible (g/constantly false)))

  (input external-buffer-sha256s g/Str :array)
  (input material-scene-infos g/Any :array)
  (input mesh-scene-infos g/Any :array)
  (input project-settings g/Any)

  (output content g/Any :cached produce-content)
  (output bones g/Any produce-bones)
  (output animation-info g/Any produce-animation-info)
  (output animation-ids g/Any produce-animation-ids)
  (output collision-mesh-renderables g/Any produce-collision-mesh-renderables)
  (output collision-mesh-set g/Any produce-collision-mesh-set)
  (output collision-meshes g/Any produce-collision-meshes)
  (output material-ids g/Any produce-material-ids)
  (output mesh-set-build-target g/Any :cached produce-mesh-set-build-target)
  (output skeleton g/Any produce-skeleton)
  (output skeleton-build-target g/Any :cached produce-skeleton-build-target)
  (output renderable-mesh-set g/Any :cached produce-renderable-mesh-set)
  (output source-scene g/Any :cached produce-source-scene)
  (output scene g/Any :cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
    :ext model-file-types
    :label (localization/message "resource.type.model-scene")
    :node-type ModelSceneNode
    :load-fn load-model-scene-node
    :read-fn model-loader/read-external-buffer-uris
    :icon mesh-icon
    :icon-class :design
    :view-types [:scene :text]))
