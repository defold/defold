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

(ns editor.data
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.gamesys.proto DataProto$Data]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn data-desc->data-desc-pb-map
  [data-desc]
  {:pre [(map? data-desc)]} ; DataProto$Data in JSON map format.
  (protobuf/sanitize data-desc :data protobuf/clj-value->ddf-struct-value))

(defn data-desc-pb-map->data-desc
  [data-desc-pb-map]
  {:pre [(map? data-desc-pb-map)]} ; DataProto$Data in Protobuf map format.
  (protobuf/sanitize data-desc-pb-map :data protobuf/ddf-struct-value->clj-value))

(defn- build-data [build-resource _dep-resources user-data]
  (let [{:keys [rt-data rt-tags]} user-data
        rt-pb-map (data-desc->data-desc-pb-map
                    (protobuf/assign-repeated {:data rt-data} :tags rt-tags))]
    {:resource build-resource
     :content (protobuf/map->bytes DataProto$Data rt-pb-map)}))

(g/defnode DataResourceNode
  (inherits resource-node/ResourceNode)

  (output save-value g/Any :cached
          (g/fnk [data]
            {:data data}))

  (output build-targets g/Any :cached
          (g/fnk [_node-id own-build-errors resource rt-data rt-tags]
            (g/precluding-errors own-build-errors
              [(bt/with-content-hash
                 {:node-id _node-id
                  :resource (workspace/make-build-resource resource)
                  :build-fn build-data
                  :user-data {:rt-tags rt-tags
                              :rt-data rt-data}})])))

  ;; Implemented by inheritors.
  (output data g/Any :abstract)
  (output rt-data g/Any (gu/passthrough data))
  (output rt-tags g/Any :abstract))

(defn- read-default-data-desc-pb-map
  [readable type-ext]
  {:pre [(string? (not-empty type-ext))]}
  (try
    (with-open [reader (io/reader readable)]
      (protobuf/read-map-without-defaults DataProto$Data reader))
    (catch Throwable cause
      (let [template-proj-path
            (when (resource/resource? readable)
              (resource/proj-path readable))

            message
            (if template-proj-path
              (str "Failed to read data resource template from " template-proj-path)
              (str "Failed to read JAR-embedded data resource template for ext ." type-ext))]
        (throw
          (ex-info
            message
            {:type-ext type-ext
             :template-proj-path template-proj-path
             :readable-type (.getName (class readable))}
            cause))))))

(defn register-data-resource-type
  [workspace & {:keys [sanitize-fn pb-encode-fn] :as args}]
  {:pre [(not (contains? args :ddf-type))
         (string? (not-empty (:ext args)))]}
  (let [default-data-desc-delay-fn
        (memoize
          (fn default-data-desc-delay-fn [template-resource]
            (delay
              ;; Cache both the default data and read failures per template.
              ;; A failed template read must mark resource nodes defective to
              ;; prevent unsanitized data from corrupting files when saved.
              (some-> template-resource
                      (read-default-data-desc-pb-map (:ext args))
                      (data-desc-pb-map->data-desc)))))

        ddf-sanitize-fn
        (fn ddf-sanitize-fn [read-opts owner-resource data-desc-pb-map]
          (let [template-resource-fn (:template-resource-fn read-opts)
                template-resource (template-resource-fn args false)
                default-data-desc (force (default-data-desc-delay-fn template-resource))
                data-desc (coll/deep-merge default-data-desc
                                           (data-desc-pb-map->data-desc data-desc-pb-map))]
            (if-not sanitize-fn
              data-desc
              (sanitize-fn read-opts owner-resource data-desc))))

        ddf-pb-encode-fn
        (if-not pb-encode-fn
          data-desc->data-desc-pb-map
          (fn ddf-pb-encode-fn [data-desc]
            (data-desc->data-desc-pb-map (pb-encode-fn data-desc))))

        args
        (-> args
            (assoc :ddf-type DataProto$Data
                   :sanitize-fn ddf-sanitize-fn
                   :pb-encode-fn ddf-pb-encode-fn))]

    (apply resource-node/register-ddf-resource-type workspace (mapcat identity args))))
