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

(ns integration.model-scene-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.gl.vertex2 :as vtx]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [java.nio ByteBuffer ByteOrder]
           [java.nio.charset StandardCharsets]
           [java.util Base64]
           [javax.vecmath Point3d Vector4d]))

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(def ^:private preview-geometry-buffer-base64
  "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA")

(def ^:private preview-image-base64
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==")

(def ^:private preview-material-specs
  [{:base-color-factor [0.25 0.5 0.75 1.0]
    :index 0
    :name "Shared"}
   {:base-color-factor [0.75 0.5 0.25 1.0]
    :index 1
    :name "Shared"}])

(defn- preview-scene-json [buffer-json]
  (str "{"
       "\"asset\":{\"version\":\"2.0\"},"
       "\"scene\":0,"
       "\"scenes\":[{\"nodes\":[0]}],"
       "\"nodes\":[{\"mesh\":0,\"name\":\"PreviewNode\"}],"
       "\"meshes\":[{\"primitives\":["
       "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":0},"
       "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2,\"material\":1}]}],"
       "\"buffers\":[" buffer-json "],"
       "\"bufferViews\":["
       "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
       "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24},"
       "{\"buffer\":0,\"byteOffset\":60,\"byteLength\":6}],"
       "\"accessors\":["
       "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
       "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
       "{\"bufferView\":2,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
       "\"samplers\":["
       "{\"wrapS\":10497,\"wrapT\":10497,\"minFilter\":9729,\"magFilter\":9729},"
       "{\"wrapS\":33071,\"wrapT\":33648,\"minFilter\":9728,\"magFilter\":9728}],"
       "\"images\":["
       "{\"name\":\"PaintAlbedo\",\"uri\":\"data:image/png;base64," preview-image-base64 "\",\"mimeType\":\"image/png\"},"
       "{\"name\":\"ChromeAlbedo\",\"uri\":\"data:image/png;base64," preview-image-base64 "\",\"mimeType\":\"image/png\"}],"
       "\"textures\":["
       "{\"name\":\"PaintAlbedoTexture\",\"sampler\":0,\"source\":0},"
       "{\"name\":\"ChromeAlbedoTexture\",\"sampler\":1,\"source\":1}],"
       "\"materials\":["
       "{\"name\":\"Shared\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.25,0.5,0.75,1],\"baseColorTexture\":{\"index\":0}}},"
       "{\"name\":\"Shared\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.75,0.5,0.25,1],\"baseColorTexture\":{\"index\":1}}}]}"))

(defn- preview-gltf-content []
  (preview-scene-json
    (str "{\"uri\":\"data:application/octet-stream;base64,"
         preview-geometry-buffer-base64
         "\",\"byteLength\":66}")))

(defn- preview-glb-content
  ^bytes []
  (let [^bytes geometry-bytes (.decode (Base64/getDecoder) preview-geometry-buffer-base64)
        ^bytes json-bytes (.getBytes ^String (preview-scene-json "{\"byteLength\":66}")
                                     StandardCharsets/UTF_8)
        padded-json-length (bit-and (+ (alength json-bytes) 3) (bit-not 3))
        padded-geometry-length (bit-and (+ (alength geometry-bytes) 3) (bit-not 3))
        glb-length (+ 12 8 padded-json-length 8 padded-geometry-length)
        ^ByteBuffer glb (doto (ByteBuffer/allocate glb-length)
                          (.order ByteOrder/LITTLE_ENDIAN))]
    (.putInt glb 0x46546c67)
    (.putInt glb 2)
    (.putInt glb glb-length)
    (.putInt glb padded-json-length)
    (.putInt glb 0x4e4f534a)
    (.put glb json-bytes)
    (while (< (.position glb) (+ 20 padded-json-length))
      (.put glb (byte 32)))
    (.putInt glb padded-geometry-length)
    (.putInt glb 0x004e4942)
    (.put glb geometry-bytes)
    (.array glb)))

(defn- scene-mesh-user-data-by-material-index [scene]
  (into {}
        (comp
          (drop 1)
          (mapcat :children)
          (map (fn [mesh-scene]
                 (let [user-data (get-in mesh-scene [:renderable :user-data])]
                   [(:material-index user-data) user-data]))))
        (:children scene)))

(defn- vector4d->vector [^Vector4d value]
  [(.-x value) (.-y value) (.-z value) (.-w value)])

(defn- assert-read-only-property [node-id property-key expected-value]
  (let [node-properties (g/node-value node-id :_properties)]
    (is (= node-id (:node-id node-properties)))
    (is (= expected-value (get-in node-properties [:properties property-key :value])))
    (is (true? (get-in node-properties [:properties property-key :read-only?])))))

(defn- assert-selected-property [app-view node-id property-key expected-value]
  (app-view/select! app-view [node-id])
  (is (= [node-id] (g/node-value app-view :selected-node-ids)))
  (let [selected-properties (g/node-value app-view :selected-node-properties)
        coalesced-properties (properties/coalesce selected-properties)]
    (is (= [node-id] (:original-node-ids coalesced-properties)))
    (is (= [expected-value]
           (get-in coalesced-properties [:properties property-key :values])))
    (is (true? (get-in coalesced-properties [:properties property-key :read-only?])))))

(deftest aabb
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/builtins/assets/gltf/cube.gltf")
          scene (g/node-value node-id :scene)
          aabb (:aabb scene)
          min ^Point3d (types/min-p aabb)
          max ^Point3d (types/max-p aabb)
          dist (.distance max min)]
      (is (< 1 dist 2)))))

(deftest gltf-valid-scene
  (test-util/with-loaded-project
    ;; Valid .gltf should load successfully via model_loader and glTF validation.
    (let [node-id (test-util/resource-node project "/mesh/triangle/gltf/Triangle.gltf")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (not (g/error? scene))))
    ;; Valid .glb should load successfully via model_loader and glTF validation.
    (let [node-id (test-util/resource-node project "/mesh/triangle/glb/valid.glb")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (not (g/error? scene))))))

(deftest gltf-contained-materials-and-textures-are-used-in-preview
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        models-directory (io/file project-path "models")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! (io/file models-directory "preview.gltf") (preview-gltf-content))
      (fs/create-file! (io/file models-directory "preview.glb") (preview-glb-content))

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)]
          (doseq [source-proj-path ["/models/preview.gltf"
                                    "/models/preview.glb"]]
            (testing source-proj-path
              (let [source-node-id (test-util/resource-node project source-proj-path)
                    scene (g/node-value source-node-id :scene)
                    user-data-by-material-index (scene-mesh-user-data-by-material-index scene)]
                (is (not (g/error? scene)))
                (is (= #{0 1} (into #{} (map key) user-data-by-material-index)))

                (doseq [{:keys [base-color-factor index name]} preview-material-specs]
                  (testing name
                    (let [material-node-id (test-util/resource-node project (str source-proj-path "/materials/" index ".material"))
                          image-node-id (test-util/resource-node project (str source-proj-path "/images/" index ".png"))
                          expected-shader (g/node-value material-node-id :shader)
                          expected-gpu-texture (g/node-value image-node-id :gpu-texture)
                          expected-sampler (first (g/node-value material-node-id :samplers))
                          expected-sampler-name (:name expected-sampler)
                          user-data (user-data-by-material-index index)
                          actual-gpu-texture (get-in user-data [:textures expected-sampler-name])
                          material-data (into {} (:material-data user-data))]
                      (is (= index (:material-index user-data)))
                      (is (= name (:material-name user-data)))
                      (is (= (dissoc expected-shader :uniforms)
                             (dissoc (:shader user-data) :uniforms)))
                      (is (= #{expected-sampler-name}
                             (into #{} (map key) (:textures user-data))))
                      (is (= base-color-factor
                             (vector4d->vector
                               (material-data "pbrMetallicRoughness.baseColorFactor"))))
                      (is (= [1.0 0.0 0.0 0.0]
                             (vector4d->vector
                               (material-data "pbrMetallicRoughness.metallicRoughnessTextures"))))
                      (is (= image-node-id (:request-id actual-gpu-texture)))
                      (is (= (:texture-request-datas expected-gpu-texture)
                             (:texture-request-datas actual-gpu-texture)))
                      (is (= (dissoc (material/sampler->tex-params expected-sampler)
                                     :default-tex-params)
                             (dissoc (:params actual-gpu-texture)
                                     :default-tex-params)))
                      (is (= [0] (vec (:texture-units actual-gpu-texture)))))))))))))))

(deftest gltf-metadata-is-shown-in-outline-and-properties
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        models-directory (io/file project-path "models")]
    (with-open [_project-directory-deleter (test-util/make-directory-deleter project-path)]
      (fs/create-file! (io/file models-directory "preview.gltf") (preview-gltf-content))
      (fs/create-file! (io/file models-directory "preview.glb") (preview-glb-content))

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              app-view (test-util/setup-app-view! project)]
          (doseq [source-proj-path ["/models/preview.gltf"
                                    "/models/preview.glb"]]
            (testing source-proj-path
              (let [source-node-id (test-util/open-tab! project app-view source-proj-path)
                    source-outline (g/node-value source-node-id :node-outline)
                    groups (:children source-outline)
                    meshes-group (nth groups 0)
                    materials-group (nth groups 1)
                    textures-group (nth groups 2)
                    mesh-outline (get-in meshes-group [:children 0])
                    material-outlines (:children materials-group)
                    texture-outlines (:children textures-group)]
                (is (= ["Meshes" "Materials" "Textures"]
                       (mapv (comp test-util/localization :label) groups)))
                (is (= ["Mesh 0"] (mapv :label (:children meshes-group))))
                (is (= ["Shared [0]" "Shared [1]"] (mapv :label material-outlines)))
                (is (= ["PaintAlbedoTexture" "ChromeAlbedoTexture"]
                       (mapv :label texture-outlines)))

                (doseq [{:keys [node-id read-only]}
                        (into groups (mapcat :children) groups)]
                  (is (g/node-id? node-id))
                  (is (true? read-only)))

                (let [mesh-node-id (:node-id mesh-outline)]
                  (assert-read-only-property mesh-node-id :index 0)
                  (assert-read-only-property mesh-node-id :name "Mesh 0")
                  (assert-read-only-property mesh-node-id :name-generated true)
                  (assert-read-only-property mesh-node-id :primitive-count 2)
                  (assert-read-only-property mesh-node-id :vertex-count 6)
                  (assert-selected-property app-view mesh-node-id :primitive-count 2))

                (doseq [[material-index material-outline]
                        (into [] (map-indexed vector) material-outlines)]
                  (let [material-node-id (:node-id material-outline)]
                    (assert-read-only-property material-node-id :index material-index)
                    (assert-read-only-property material-node-id :name "Shared")
                    (is (resource/gltf-resource?
                          (get-in (g/node-value material-node-id :_properties)
                                  [:properties :material :value])))
                    (assert-selected-property app-view material-node-id :name "Shared")))

                (doseq [[texture-index texture-outline]
                        (into [] (map-indexed vector) texture-outlines)]
                  (let [texture-node-id (:node-id texture-outline)
                        expected-values
                        (nth [{:basisu false
                               :image-index 0
                               :mag-filter 9729
                               :min-filter 9729
                               :name "PaintAlbedoTexture"
                               :sampler-index 0
                               :wrap-s 10497
                               :wrap-t 10497}
                              {:basisu false
                               :image-index 1
                               :mag-filter 9728
                               :min-filter 9728
                               :name "ChromeAlbedoTexture"
                               :sampler-index 1
                               :wrap-s 33071
                               :wrap-t 33648}]
                             texture-index)]
                    (doseq [[property-key expected-value] expected-values]
                      (assert-read-only-property texture-node-id property-key expected-value))))

                (let [texture-node-id (:node-id (nth texture-outlines 1))]
                  (assert-selected-property app-view texture-node-id :name "ChromeAlbedoTexture"))))))))))

(deftest gltf-invalid-scene
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/mesh/accessor_element_out_of_max_bound.gltf")
          scene (log/without-logging
                  (g/node-value node-id :scene))]
      (is (g/error? scene))
      (let [errors (g/flatten-errors scene)
            msg    (some-> errors g/error-message test-util/localization)]
        (is (re-find #"glTF validation failed" msg))
        (is (re-find #"ACCESSOR_MAX_MISMATCH" msg))
        (is (re-find #"ACCESSOR_ELEMENT_OUT_OF_MAX_BOUND" msg))))))

(deftest external-buffer-change-invalidates-content
  (test-util/with-scratch-project "test/resources/test_project"
    (let [model-scene (test-util/resource-node project "/mesh/triangle/gltf/Triangle.gltf")
          collision-object (test-util/resource-node project "/collision_object/mesh_shape.collisionobject")
          collision-shape (:node-id (test-util/outline collision-object [0]))
          model-scene-resource (workspace/find-resource workspace "/mesh/triangle/gltf/Triangle.gltf")
          buffer-resource (workspace/find-resource workspace "/mesh/triangle/gltf/simpleTriangle.bin")
          buffer-node (project/get-resource-node project buffer-resource)
          first-x (fn []
                    (-> (g/node-value model-scene :content)
                        (get-in [:mesh-set :models 0 :meshes 0 :positions 0])
                        double))
          collision-preview-position-buffer
          (fn []
            (get-in (g/node-value collision-shape :selected-collision-mesh-renderable)
                    [:primitives 0 :position-buffer]))]
      (g/set-property! collision-shape :mesh-scene model-scene-resource)
      (g/set-properties! collision-shape :mesh-name "Triangle" :mesh-index 0)

      (is (= ["simpleTriangle.bin"] (g/node-value model-scene :source-value)))
      (is (= [[buffer-node :sha256]]
             (g/sources-of model-scene :external-buffer-sha256s)))
      (is (coll/empty? (get-in (g/node-value model-scene :content)
                               [:mesh-set :raw-models 0 :meshes])))
      (is (= 0.0 (first-x)))

      (let [initial-mesh-set-content-hash (:content-hash (g/node-value model-scene :mesh-set-build-target))
            initial-collision-content-hash (-> (g/node-value collision-object :build-targets)
                                               first
                                               :content-hash)
            initial-collision-preview-position-buffer (collision-preview-position-buffer)
            bytes (resource/resource->bytes buffer-resource)]
        (-> (ByteBuffer/wrap bytes)
            (.order ByteOrder/LITTLE_ENDIAN)
            (.putFloat 8 0.5))
        (test-support/write-until-new-mtime (io/as-file buffer-resource) bytes)

        (workspace/resource-sync! workspace)

        (is (= 0.5 (first-x)))
        (is (not= initial-mesh-set-content-hash
                  (:content-hash (g/node-value model-scene :mesh-set-build-target))))
        (is (not= initial-collision-content-hash
                  (-> (g/node-value collision-object :build-targets)
                      first
                      :content-hash)))
        (is (not (identical? initial-collision-preview-position-buffer
                             (collision-preview-position-buffer))))))))
