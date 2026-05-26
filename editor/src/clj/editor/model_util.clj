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

(ns editor.model-util
  (:require [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.gl.attribute :as attribute]
            [editor.graphics.types :as graphics.types]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair]]
            [util.eduction :as e]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-transformed-attribute-buffer
  [scene-node-id untransformed-attribute-buffer-lifecycle attribute-transform]
  {:pre [(gt/node-id? scene-node-id)]}
  (let [transform-render-arg-key (graphics.types/attribute-transform-render-arg-key attribute-transform)
        w-component (graphics.types/attribute-transform-w-component attribute-transform)
        untransformed-buffer-data (graphics.types/buffer-data untransformed-attribute-buffer-lifecycle)
        untransformed-buffer-request-id (:request-id untransformed-attribute-buffer-lifecycle)
        _ (assert (gt/node-id? (:scene-node-id untransformed-buffer-request-id)))
        _ (assert (not (contains? untransformed-buffer-request-id :attribute-transform)))
        request-id (assoc untransformed-buffer-request-id
                     :scene-node-id scene-node-id
                     :attribute-transform attribute-transform)]
    (attribute/make-transformed-attribute-buffer request-id untransformed-buffer-data transform-render-arg-key w-component)))

(defn- make-attribute-buffer-binding
  [scene-node-id attribute-buffer-lifecycle attribute-info]
  (let [{:keys [attribute-transform ^long location step-function]} attribute-info]
    (case attribute-transform
      (:attribute-transform-none)
      (attribute/make-attribute-buffer-binding attribute-buffer-lifecycle location step-function)

      (:attribute-transform-normal :attribute-transform-world)
      (attribute/make-attribute-buffer-binding
        (make-transformed-attribute-buffer scene-node-id attribute-buffer-lifecycle attribute-transform)
        location
        step-function))))

(defn- make-attribute-value-binding
  [attribute-bytes attribute-info]
  (let [{:keys [attribute-transform data-type location normalize semantic-type vector-type]} attribute-info
        element-type (graphics.types/make-element-type vector-type data-type normalize)

        value-array
        (or (when attribute-bytes
              (try
                (let [byte-buffer (buffers/wrap-byte-array attribute-bytes :byte-order/native)
                      buffer-data-type (graphics.types/data-type-buffer-data-type data-type)]
                  (buffers/as-primitive-array byte-buffer buffer-data-type))
                (catch Exception exception
                  (let [message (format "Vertex attribute '%s' - %s"
                                        (:name attribute-info)
                                        (ex-message exception))]
                    (log/warn :message message
                              :exception exception
                              :ex-data (ex-data exception)))
                  nil)))
            (graphics.types/default-attribute-value-array semantic-type element-type))]

    (case attribute-transform
      (:attribute-transform-none)
      (attribute/make-attribute-value-binding value-array element-type location)

      (:attribute-transform-normal :attribute-transform-world)
      (let [transform-render-arg-key (graphics.types/attribute-transform-render-arg-key attribute-transform)
            w-component (graphics.types/attribute-transform-w-component attribute-transform)]
        (attribute/make-transformed-attribute-value-binding value-array element-type transform-render-arg-key w-component location)))))

(defn- make-attribute-render-arg-binding
  [render-arg-key attribute-info]
  (let [{:keys [data-type location normalize vector-type]} attribute-info
        element-type (graphics.types/make-element-type vector-type data-type normalize)]
    (attribute/make-attribute-render-arg-binding render-arg-key element-type location)))

(defn- make-render-arg-attribute-binding-entries
  [attribute-infos render-arg-key]
  (->> attribute-infos
       (e/map
         (fn [{:keys [name-key] :as attribute-info}]
           {:pre [(keyword? name-key)]}
           (pair name-key
                 (make-attribute-render-arg-binding render-arg-key attribute-info))))))

(defn- make-attribute-binding-entries
  [scene-node-id attribute-infos attribute-buffers name-key->attribute-bytes]
  {:pre [(or (nil? attribute-buffers)
             (vector? attribute-buffers))]}
  (->> attribute-infos
       (e/map-indexed
         (fn [^long channel {:keys [name-key] :as attribute-info}]
           {:pre [(keyword? name-key)]}
           ;; Use attribute buffers from the mesh when available. Otherwise,
           ;; use the overridden attribute value from the referencing
           ;; component. If not overridden by the component, use the value
           ;; specified for the attribute in the Material. Finally, if none of
           ;; the above are available, use a suitable default value inferred
           ;; from the semantic-type.
           (pair name-key
                 (if-some [attribute-buffer (get attribute-buffers channel)] ; The vector may contain nil if, for example, we only have UVs for texcoord1, but not for texcoord0.
                   (make-attribute-buffer-binding scene-node-id attribute-buffer attribute-info)
                   (let [attribute-bytes (or (name-key->attribute-bytes name-key) ; From the referencing component.
                                             (:bytes attribute-info))] ; From the Material.
                     (make-attribute-value-binding attribute-bytes attribute-info))))))))

(defn make-attribute-bindings
  [scene-node-id combined-attribute-infos semantic-type->attribute-buffers name-key->attribute-bytes]
  ;; Note: The combined-attribute-infos should be in the order they are declared
  ;; in the Material. This becomes important when channels are assigned later in
  ;; this function.
  {:pre [(g/node-id? scene-node-id)]}
  (->> combined-attribute-infos
       (util/group-into {} [] :semantic-type)
       (into {}
             (mapcat
               (fn [[semantic-type attribute-infos]]
                 {:pre [(graphics.types/semantic-type? semantic-type)]}
                 (case semantic-type
                   :semantic-type-world-matrix
                   (make-render-arg-attribute-binding-entries attribute-infos :world)

                   :semantic-type-normal-matrix
                   (make-render-arg-attribute-binding-entries attribute-infos :normal)

                  :semantic-type-texture-transform-2d
                  ;; We currently don't support texture transforms for models.
                  (make-attribute-binding-entries scene-node-id attribute-infos nil (constantly nil))

                  ;; else
                  (let [attribute-buffers (semantic-type->attribute-buffers semantic-type)]
                    (make-attribute-binding-entries scene-node-id attribute-infos attribute-buffers name-key->attribute-bytes))))))))

(defn- model-instance-step-semantic-type? [semantic-type]
  (case semantic-type
    (:semantic-type-world-matrix :semantic-type-normal-matrix) true
    false))

(defn- auto-instance-step-model-attribute [attribute-info]
  (cond-> attribute-info
          (model-instance-step-semantic-type? (:semantic-type attribute-info))
          (assoc :step-function :vertex-step-function-instance)))

(defn make-model-attribute-bindings
  "Returns both the legacy merged attribute bindings used for single-renderable
  model draws and a vertex/instance split for the instanced model path.

  Model world and normal matrix attributes are promoted to instance-step here
  to mirror the engine's automatic instancing behavior for the mtx_world and
  mtx_normal shader attribute names.

  Instance-step attributes deliberately do not use mesh attribute buffers even if
  they share a semantic with mesh data. They are supplied from render args,
  component/material attribute values, or semantic defaults until the caller
  replaces them with an instance buffer."
  [scene-node-id combined-attribute-infos semantic-type->attribute-buffers name-key->attribute-bytes]
  {:pre [(g/node-id? scene-node-id)]}
  (let [combined-attribute-infos (mapv auto-instance-step-model-attribute combined-attribute-infos)
        vertex-attribute-infos (filterv #(= :vertex-step-function-vertex (:step-function %)) combined-attribute-infos)
        instance-attribute-infos (filterv #(= :vertex-step-function-instance (:step-function %)) combined-attribute-infos)
        vertex-attribute-bindings (make-attribute-bindings scene-node-id vertex-attribute-infos semantic-type->attribute-buffers name-key->attribute-bytes)
        instance-attribute-bindings (make-attribute-bindings scene-node-id instance-attribute-infos {} name-key->attribute-bytes)]
    {:attribute-bindings (merge vertex-attribute-bindings instance-attribute-bindings)
     :vertex-attribute-bindings vertex-attribute-bindings
     :instance-attribute-bindings instance-attribute-bindings
     :instance-attribute-infos instance-attribute-infos
     :has-instance-attributes? (boolean (seq instance-attribute-infos))}))
