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
  (:require [editor.protobuf :as protobuf]
            [editor.resource-node :as resource-node])
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

(defn pb-encode-data-desc
  [_workspace data-desc]
  {:pre [(map? data-desc)]} ; DataProto$Data in JSON map format.
  (data-desc->data-desc-pb-map data-desc))

(defn data-resource-type?
  [resource-type]
  (= DataProto$Data (:ddf-type (:test-info resource-type))))

(defn register-data-resource-type
  [workspace & {:keys [sanitize-fn pb-encode-fn] :as args}]
  {:pre [(not (contains? args :ddf-type))]}
  (let [ddf-sanitize-fn
        (if-not sanitize-fn
          data-desc-pb-map->data-desc
          (fn ddf-sanitize-fn [data-desc-pb-map]
            (sanitize-fn (data-desc-pb-map->data-desc data-desc-pb-map))))

        ddf-pb-encode-fn
        (if-not pb-encode-fn
          pb-encode-data-desc
          (fn ddf-pb-encode-fn [workspace data-desc]
            (pb-encode-data-desc workspace (pb-encode-fn workspace data-desc))))

        args
        (-> args
            (assoc :ddf-type DataProto$Data
                   :sanitize-fn ddf-sanitize-fn
                   :pb-encode-fn ddf-pb-encode-fn))]

    (apply resource-node/register-ddf-resource-type workspace (mapcat identity args))))
