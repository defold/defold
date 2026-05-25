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

(defn data-resource-type?
  [resource-type]
  (= DataProto$Data (:ddf-type (:test-info resource-type))))

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
  (let [default-data-desc-delay
        (delay
          ;; Read the default DataProto$Data in Protobuf map format from the
          ;; non-user template associated with the file extension, if there is
          ;; one. If there is no template, return nil when forced. If there is a
          ;; template but the read fails, throw a decorated exception whenever
          ;; the delay is forced. This ensures any resource-nodes using this
          ;; resource-type will be marked defective. We want this since
          ;; unsanitized data can corrupt project files when saved.
          (some-> (workspace/template-resource (g/unsafe-basis) workspace args false)
                  (read-default-data-desc-pb-map (:ext args))
                  (data-desc-pb-map->data-desc)))

        data-desc-pb-map->data-desc-with-defaults
        (fn data-desc-pb-map->data-desc-with-defaults [data-desc-pb-map]
          (coll/deep-merge
            (force default-data-desc-delay)
            (data-desc-pb-map->data-desc data-desc-pb-map)))

        ddf-sanitize-fn
        (if-not sanitize-fn
          data-desc-pb-map->data-desc-with-defaults
          (fn ddf-sanitize-fn [data-desc-pb-map]
            (sanitize-fn (data-desc-pb-map->data-desc-with-defaults data-desc-pb-map))))

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
