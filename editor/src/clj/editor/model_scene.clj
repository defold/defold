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
  (:require [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.attribute :as attribute]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.types :as gl.types]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.model-loader :as model-loader]
            [editor.model-util :as model-util]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.shaders :as shaders]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.num :as num])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [java.nio ByteOrder FloatBuffer]
           [javax.vecmath Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")
(def model-file-types ["dae" "gltf" "glb"])
(def animation-file-types ["animationset" "dae" "gltf" "glb"])

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
  (rig/make-mesh-set-build-target (resource/workspace resource) _node-id (:mesh-set content)))

(g/defnk produce-skeleton [content]
  (:skeleton content))

(g/defnk produce-skeleton-build-target [_node-id resource skeleton]
  (rig/make-skeleton-build-target (resource/workspace resource) _node-id skeleton))

(g/defnk produce-bones [content]
  (:bones content))

(g/defnk produce-content [_node-id resource]
  (model-loader/load-scene _node-id resource))

(g/defnk produce-animation-info [resource]
  [{:path (resource/proj-path resource) :parent-id "" :resource resource}])

(g/defnk produce-animation-ids [content]
  (:animation-ids content))

(def ^:private default-material-ids ["default"])

(g/defnk produce-material-ids [content]
  (let [ret (:material-ids content)]
    (if (zero? (count ret))
      default-material-ids
      ret)))

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
       ["pbrMetallicRoughness.metallicRoughnessFactor"
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
            ;; TODO(instancing): Do we even have Rig$Materials in the :mesh-set for Collada scenes?
            mesh-material-data (nth (:materials mesh-set) material-index)
            material-data (make-renderable-material-data mesh-material-data)]
        {:aabb aabb
         :material-name material-name
         :material-data material-data
         :renderable-buffers renderable-buffers}))))

(defn- make-renderable-model [model model-request-id mesh-set mesh-material-index->material-name]
  (let [{:keys [translation rotation scale]} (:local model)
        model-transform (math/clj->mat4 translation rotation scale)

        renderable-meshes
        (coll/transfer (:meshes model) []
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
        {:transform model-transform
         :aabb model-aabb
         :renderable-meshes renderable-meshes}))))

(defn- make-renderable-mesh-set [mesh-set mesh-set-request-id mesh-material-index->material-name]
  (let [renderable-models
        (mapv (fn [model]
                (let [model-request-id (assoc mesh-set-request-id :model-id (:id model))]
                  (make-renderable-model model model-request-id mesh-set mesh-material-index->material-name)))
              (:models mesh-set))]

    (g/precluding-errors renderable-models
      (let [mesh-set-aabb (transduce
                            (map (fn [{:keys [aabb transform]}]
                                   (geom/aabb-transform aabb transform)))
                            geom/aabb-union
                            geom/null-aabb
                            renderable-models)]
        {:aabb mesh-set-aabb
         :renderable-models renderable-models}))))

(g/defnk produce-renderable-mesh-set [_node-id content]
  (let [mesh-set-request-id
        {:request-type :ModelSceneNode/mesh-set
         :scene-node-id _node-id}

        mesh-material-index->material-name
        (or (some-> content :material-ids not-empty vec)
            default-material-ids)

        renderable-mesh-set-or-error-value
        (make-renderable-mesh-set (:mesh-set content) mesh-set-request-id mesh-material-index->material-name)]

    (if (g/error-value? renderable-mesh-set-or-error-value)
      (assoc renderable-mesh-set-or-error-value :_node-id _node-id :_label :renderable-mesh-set)
      renderable-mesh-set-or-error-value)))

(defn- make-mesh-scene [scene-node-id renderable-mesh]
  {:pre [(g/node-id? scene-node-id)]}
  (let [{:keys [aabb material-data material-name renderable-buffers]} renderable-mesh
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

    {:aabb aabb
     :renderable renderable}))

(defn- make-model-scene [scene-node-id renderable-model]
  (let [{:keys [transform aabb renderable-meshes]} renderable-model
        mesh-scenes (mapv #(make-mesh-scene scene-node-id %)
                          renderable-meshes)]
    {:transform transform
     :aabb aabb
     :children mesh-scenes}))

(defn- make-scene [scene-node-id renderable-mesh-set]
  (let [{:keys [aabb renderable-models]} renderable-mesh-set

        child-scenes
        (into [{:node-id scene-node-id
                :aabb aabb
                :renderable (render-util/make-aabb-outline-renderable #{:model})}]
              (map #(make-model-scene scene-node-id %))
              renderable-models)]

    {:node-id scene-node-id
     :aabb aabb
     :renderable {:tags #{:model}
                  :batch-key nil ; Batching is disabled in the editor for simplicity.
                  :passes [pass/opaque-selection]} ; A selection pass to ensure it can be selected and manipulated.
     :children child-scenes}))

(g/defnk produce-scene [_node-id renderable-mesh-set]
  (make-scene _node-id renderable-mesh-set))

(defn- finalize-claim-scene [scene _old-node-id new-node-id]
  (update scene :children coll/mapv>
          update :children coll/mapv>
          update-in [:renderable :user-data :attribute-bindings]
          attribute/claim-transformed-attribute-buffer-bindings
          assoc :scene-node-id new-node-id))

(defn- augment-mesh-scene [mesh-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info]
  (let [{:keys [user-data]} (:renderable mesh-scene)
        {:keys [material-data material-name]} user-data
        material-scene-info (material-name->material-scene-info material-name)
        claimed-scene (scene/claim-child-scene mesh-scene old-node-id new-node-id new-node-outline-key)]
    (if (nil? material-scene-info)
      claimed-scene
      (let [{:keys [gpu-textures material-attribute-infos shader vertex-attribute-bytes vertex-space]} material-scene-info
            shader-attribute-reflection-infos (shader/attribute-reflection-infos shader nil)
            default-coordinate-space (case vertex-space
                                       :vertex-space-local :coordinate-space-local
                                       :vertex-space-world :coordinate-space-world)]
        (assert (map? gpu-textures))
        (assert (every? string? (keys gpu-textures)))
        (assert (every? texture/texture-lifecycle? (vals gpu-textures)))
        (assert (every? map? material-attribute-infos))
        (assert (every? keyword? (map :name-key material-attribute-infos)))
        (assert (shader/shader-lifecycle? shader))
        (assert (every? keyword? (keys vertex-attribute-bytes)))
        (assert (every? bytes? (vals vertex-attribute-bytes)))
        (update claimed-scene :renderable
                update :user-data
                (fn [user-data]
                  (let [mesh-renderable-buffers (:mesh-renderable-buffers user-data)
                        semantic-type->attribute-buffers (:attribute-buffers mesh-renderable-buffers)
                        combined-attribute-infos (graphics/combined-attribute-infos shader-attribute-reflection-infos material-attribute-infos default-coordinate-space)
                        coordinate-space-info (graphics/coordinate-space-info combined-attribute-infos)
                        attribute-bindings (model-util/make-attribute-bindings new-node-id combined-attribute-infos semantic-type->attribute-buffers vertex-attribute-bytes)]
                    (assoc user-data
                      :attribute-bindings attribute-bindings
                      :coordinate-space-info coordinate-space-info
                      :material-attribute-infos material-attribute-infos
                      :material-data material-data
                      :shader shader
                      :textures gpu-textures))))))))

(defn- augment-model-scene [model-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info]
  (let [mesh-scenes (:children model-scene)]
    (assoc (scene/claim-child-scene model-scene old-node-id new-node-id new-node-outline-key)
      :children (mapv #(augment-mesh-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info)
                      mesh-scenes))))

(defn augment-scene [scene new-node-id new-node-outline-key material-name->material-scene-info]
  (if (g/error-value? scene)
    scene
    (let [old-node-id (:node-id scene)
          model-scenes (:children scene)]
      (assoc scene
        :node-id new-node-id
        :node-outline-key new-node-outline-key
        :finalize-claim-fn finalize-claim-scene ; We may have one or more TransformedAttributeBufferLifecycles after this, so we must assign them unique request-ids per instance.
        :children (mapv #(augment-model-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info)
                        model-scenes)))))

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

(g/defnode ModelSceneNode
  (inherits resource-node/ResourceNode)

  (output content g/Any :cached produce-content)
  (output bones g/Any produce-bones)
  (output animation-info g/Any produce-animation-info)
  (output animation-ids g/Any produce-animation-ids)
  (output material-ids g/Any produce-material-ids)
  (output mesh-set-build-target g/Any :cached produce-mesh-set-build-target)
  (output skeleton g/Any produce-skeleton)
  (output skeleton-build-target g/Any :cached produce-skeleton-build-target)
  (output renderable-mesh-set g/Any :cached produce-renderable-mesh-set)
  (output scene g/Any :cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
    :ext model-file-types
    :label (localization/message "resource.type.model-scene")
    :node-type ModelSceneNode
    :icon mesh-icon
    :icon-class :design
    :view-types [:scene :text]))
