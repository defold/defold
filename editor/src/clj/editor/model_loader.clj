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

(ns editor.model-loader
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [com.dynamo.bob.pipeline ModelUtil]
           [com.dynamo.bob.pipeline GLTFValidator GLTFValidator$ValidateError GLTFValidator$ValidateResult]
           [com.dynamo.rig.proto Rig$MeshSet Rig$Skeleton]
           [java.io InputStream]))

(set! *warn-on-reflection* true)

(defn- morph-target-texture-limits [project-settings]
  (mapv #(int (get project-settings %))
        [["model" "max_morph_target_texture_width"]
         ["model" "max_morph_target_texture_height"]]))

(defn- load-model-scene
  [resource ^InputStream stream morph-tex-w morph-tex-h]
  (let [workspace (resource/workspace resource)
        project-directory (workspace/project-directory workspace)
        mesh-set-builder (Rig$MeshSet/newBuilder)
        skeleton-builder (Rig$Skeleton/newBuilder)
        path (resource/path resource)
        options nil
        data-resolver (ModelUtil/createFileDataResolver project-directory)
        scene (ModelUtil/loadScene stream ^String path options data-resolver)
        bones (ModelUtil/loadSkeleton scene)
        material-ids (ModelUtil/loadMaterialNames scene)
        animation-ids (ModelUtil/getAnimationNames scene)] ; sorted on duration (largest first)
    (when-not (empty? bones)
      (ModelUtil/skeletonToDDF bones skeleton-builder))
    (ModelUtil/loadModels scene mesh-set-builder morph-tex-w morph-tex-h)
    (let [mesh-set (protobuf/pb->map-with-defaults (.build mesh-set-builder))
          skeleton (protobuf/pb->map-with-defaults (.build skeleton-builder))]
      {:mesh-set mesh-set
       :skeleton skeleton
       :bones bones
       :buffers (.buffers scene)
       :animation-ids animation-ids
       :material-ids material-ids})))

(defn- format-gltf-validation-errors [^java.util.List validation-errors]
  (string/join "\n"
               (mapv (fn [^GLTFValidator$ValidateError error]
                       (format "  - %s (pointer=%s, code=%s)"
                               (.message error)
                               (.pointer error)
                               (.code error)))
                     validation-errors)))

(defn- handle-gltf-validation-result [resource ^GLTFValidator$ValidateResult gltf-validation-result]
  (when-not (.result gltf-validation-result)
    (let [gltf-validation-errors (.errors gltf-validation-result)
          formatted-gltf-validation-error (format-gltf-validation-errors gltf-validation-errors)]
      (throw (ex-info (str "glTF validation failed:\n" formatted-gltf-validation-error)
                      {:resource resource
                       :errors gltf-validation-errors})))))

(defn- load-scene-internal [resource project-settings]
  (let [[morph-tex-w morph-tex-h] (morph-target-texture-limits project-settings)
        ext (string/lower-case (resource/ext resource))
        is-zip-resource? (resource/zip-resource? resource)]
    ;; First, run glTF/glb files through the bob validator.
    ;; For zip resources we validate from a stream and avoid validating external
    ;; resources (buffers on disk). For regular filesystem resources we validate
    ;; by absolute path and allow external resource validation.
    (when (or (= ext "gltf") (= ext "glb"))
      (if is-zip-resource?
        (with-open [stream (io/input-stream resource)]
          (handle-gltf-validation-result resource (GLTFValidator/validateGltf stream ext false)))
        (handle-gltf-validation-result resource (GLTFValidator/validateGltf (resource/abs-path resource) true))))
    ;; Then, open a new stream for actually loading the scene.
    (with-open [stream (io/input-stream resource)]
      (load-model-scene resource stream morph-tex-w morph-tex-h))))

(defn load-scene [node-id resource project-settings]
  (try
    (load-scene-internal resource project-settings)
    (catch Exception e
      (let [path (resource/proj-path resource)
            message (.getMessage e)]
        (log/error :message (format "The file '%s' failed to load:\n%s" path message) :exception e)
        (g/->error node-id nil :fatal nil
                   (localization/message "error.model-load-failed" {"file" path "error" message})
                   {:type :invalid-content :resource resource})))))

(def ^:private root-ancestor-translation-epsilon 1e-4)
(def ^:private root-ancestor-scale-epsilon 1e-4)
(def ^:private root-ancestor-rotation-dot-epsilon 1e-6)

(defn- instance-field [object field-name]
  (clojure.lang.Reflector/getInstanceField object field-name))

(defn- identity-transform-data []
  {:translation {:x 0.0 :y 0.0 :z 0.0}
   :rotation {:x 0.0 :y 0.0 :z 0.0 :w 1.0}
   :scale {:x 1.0 :y 1.0 :z 1.0}})

(defn- transform->data [transform]
  (let [translation (instance-field transform "translation")
        rotation (instance-field transform "rotation")
        scale (instance-field transform "scale")]
    {:translation {:x (double (instance-field translation "x"))
                   :y (double (instance-field translation "y"))
                   :z (double (instance-field translation "z"))}
     :rotation {:x (double (instance-field rotation "x"))
                :y (double (instance-field rotation "y"))
                :z (double (instance-field rotation "z"))
                :w (double (instance-field rotation "w"))}
     :scale {:x (double (instance-field scale "x"))
             :y (double (instance-field scale "y"))
             :z (double (instance-field scale "z"))}}))

(defn- root-bone [bones]
  (or (some #(when (nil? (instance-field % "parent")) %) bones)
      (first bones)))

(defn- root-bone-ancestor-info [scene]
  (let [bones (seq (ModelUtil/loadSkeleton scene))]
    (when-let [root-bone (and bones (root-bone bones))]
      (let [bone-node (instance-field root-bone "node")
            ancestor-node (some-> bone-node (instance-field "parent"))]
        {:bone-name (instance-field root-bone "name")
         :ancestor-node-name (or (some-> ancestor-node (instance-field "name")) "")
         :ancestor-transform (if ancestor-node
                               (transform->data (instance-field ancestor-node "world"))
                               (identity-transform-data))}))))

(defn- nearly-equal [a b epsilon]
  (<= (Math/abs ^double (- (double a) (double b))) epsilon))

(defn- equivalent-rotation [a b]
  (let [dot (+ (* (:x a) (:x b))
               (* (:y a) (:y b))
               (* (:z a) (:z b))
               (* (:w a) (:w b)))]
    (<= (- 1.0 (Math/abs ^double dot)) root-ancestor-rotation-dot-epsilon)))

(defn- equivalent-transform [a b]
  (and (nearly-equal (get-in a [:translation :x]) (get-in b [:translation :x]) root-ancestor-translation-epsilon)
       (nearly-equal (get-in a [:translation :y]) (get-in b [:translation :y]) root-ancestor-translation-epsilon)
       (nearly-equal (get-in a [:translation :z]) (get-in b [:translation :z]) root-ancestor-translation-epsilon)
       (nearly-equal (get-in a [:scale :x]) (get-in b [:scale :x]) root-ancestor-scale-epsilon)
       (nearly-equal (get-in a [:scale :y]) (get-in b [:scale :y]) root-ancestor-scale-epsilon)
       (nearly-equal (get-in a [:scale :z]) (get-in b [:scale :z]) root-ancestor-scale-epsilon)
       (equivalent-rotation (:rotation a) (:rotation b))))

(defn- format-root-ancestor-transform [transform]
  (format "T=(%.4f, %.4f, %.4f) R=(%.4f, %.4f, %.4f, %.4f) S=(%.4f, %.4f, %.4f)"
          (get-in transform [:translation :x]) (get-in transform [:translation :y]) (get-in transform [:translation :z])
          (get-in transform [:rotation :x]) (get-in transform [:rotation :y]) (get-in transform [:rotation :z]) (get-in transform [:rotation :w])
          (get-in transform [:scale :x]) (get-in transform [:scale :y]) (get-in transform [:scale :z])))

(defn- make-root-ancestor-transform-warning [skeleton-scene skeleton-path animations-scene animations-path]
  (let [skeleton-info (root-bone-ancestor-info skeleton-scene)
        animations-info (root-bone-ancestor-info animations-scene)]
    (when (and skeleton-info
               animations-info
               (not (equivalent-transform (:ancestor-transform skeleton-info)
                                          (:ancestor-transform animations-info))))
      (let [skeleton-ancestor-name (if (string/blank? (:ancestor-node-name skeleton-info)) "<identity>" (:ancestor-node-name skeleton-info))
            animations-ancestor-name (if (string/blank? (:ancestor-node-name animations-info)) "<identity>" (:ancestor-node-name animations-info))]
        (format "The skeleton '%s' and animations '%s' use different root ancestor transforms above the root bone. Skeleton root bone '%s' inherits '%s' with %s, while animation root bone '%s' inherits '%s' with %s. Combining these files may cause incorrect root rotation, scale, or translation at runtime."
                skeleton-path animations-path
                (:bone-name skeleton-info) skeleton-ancestor-name (format-root-ancestor-transform (:ancestor-transform skeleton-info))
                (:bone-name animations-info) animations-ancestor-name (format-root-ancestor-transform (:ancestor-transform animations-info)))))))

(defn root-ancestor-transform-warning [skeleton-resource animations-resource]
  (when (and skeleton-resource
             animations-resource
             (not= (resource/proj-path skeleton-resource) (resource/proj-path animations-resource))
             (#{"gltf" "glb"} (string/lower-case (resource/ext skeleton-resource)))
             (#{"gltf" "glb"} (string/lower-case (resource/ext animations-resource))))
    (try
      (let [workspace (resource/workspace skeleton-resource)
            project-directory (workspace/project-directory workspace)
            data-resolver (ModelUtil/createFileDataResolver project-directory)
            skeleton-path (resource/proj-path skeleton-resource)
            animations-path (resource/proj-path animations-resource)]
        (with-open [skeleton-stream (io/input-stream skeleton-resource)
                    animations-stream (io/input-stream animations-resource)]
          (let [skeleton-scene (ModelUtil/loadScene skeleton-stream skeleton-path nil data-resolver)
                animations-scene (ModelUtil/loadScene animations-stream animations-path nil data-resolver)]
            (make-root-ancestor-transform-warning skeleton-scene skeleton-path animations-scene animations-path))))
      (catch Exception error
        (log/warn :message "Failed to compare model skeleton and animation root ancestor transforms"
                  :skeleton (resource/proj-path skeleton-resource)
                  :animations (resource/proj-path animations-resource)
                  :exception error)
        nil))))
