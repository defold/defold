;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.render-program-utils
  (:require [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms])
  (:import [com.dynamo.render.proto Material$MaterialDesc$ConstantType Material$MaterialDesc$FilterModeMag Material$MaterialDesc$FilterModeMin Material$MaterialDesc$Sampler Material$MaterialDesc$WrapMode]))

(set! *warn-on-reflection* true)

(defn gen-form-data-constants [label path-key]
  {:path [path-key]
   :label label
   :type :table
   :columns (let [constant-values (protobuf/enum-values Material$MaterialDesc$ConstantType)]
              [{:path [:name] :label "Name" :type :string}
               {:path [:type]
                :label "Type"
                :type :choicebox
                :options (protobuf-forms/make-options constant-values)
                :default (ffirst constant-values)}
               {:path [:value] :label "Value" :type :vec4}])})

(defn gen-form-data-samplers [label path-key]
  {:path [path-key]
   :label label
   :type :table
   :columns (let [wrap-options (protobuf/enum-values Material$MaterialDesc$WrapMode)
                  min-options (protobuf/enum-values Material$MaterialDesc$FilterModeMin)
                  mag-options (protobuf/enum-values Material$MaterialDesc$FilterModeMag)]
              [{:path [:name] :label "Name" :type :string}
               {:path [:wrap-u]
                :label "Wrap U"
                :type :choicebox
                :options (protobuf-forms/make-options wrap-options)
                :default (ffirst wrap-options)}
               {:path [:wrap-v]
                :label "Wrap V"
                :type :choicebox
                :options (protobuf-forms/make-options wrap-options)
                :default (ffirst wrap-options)}
               {:path [:filter-min]
                :label "Filter Min"
                :type :choicebox
                :options (protobuf-forms/make-options min-options)
                :default (ffirst min-options)}
               {:path [:filter-mag]
                :label "Filter Mag"
                :type :choicebox
                :options (protobuf-forms/make-options mag-options)
                :default (ffirst mag-options)}
               {:path [:max-anisotropy]
                :label "Max Anisotropy"
                :type :number}])})

(defn- hack-downgrade-constant-value
  "HACK/FIXME: The value field in MaterialDesc$Constant was changed from
  `optional` to `repeated` in material. proto so that we can set uniform array
  values in the runtime. However, we do not yet support editing of array values
  in the material constant editing widget, and MaterialDesc$Constant is used for
  both the runtime binary format and the material file format. For the time
  being, we only read (and allow editing of) the first value from uniform
  arrays. Since there is no way to add more uniform array entries from the
  editor, it should be safe to do so until we can support uniform arrays fully."
  [upgraded-constant-value]
  (first upgraded-constant-value))

(defn- hack-upgrade-constant-value
  "HACK/FIXME: See above for the detailed background. We must convert the legacy
  `optional` value to a `repeated` value when writing the runtime binary format."
  [downgraded-constant-value]
  [downgraded-constant-value])

(defn- hack-downgrade-constant [constant]
  (protobuf/sanitize constant :value hack-downgrade-constant-value))

(defn- hack-upgrade-constant [constant]
  (protobuf/sanitize constant :value hack-upgrade-constant-value))

(def hack-downgrade-constants (partial mapv hack-downgrade-constant))

(def hack-upgrade-constants (partial mapv hack-upgrade-constant))

(def ^:private editable-sampler-optional-field-defaults
  (-> Material$MaterialDesc$Sampler
      (protobuf/default-message #{:optional})
      (dissoc :name-hash :texture))) ; TODO: Support assigning a default :texture for Samplers.

(defn sampler->editable-sampler [sampler]
  (merge editable-sampler-optional-field-defaults sampler))

(defn samplers->editable-samplers [samplers]
  (mapv sampler->editable-sampler samplers))

(defn editable-samplers->samplers [editable-samplers]
  (mapv #(protobuf/clear-defaults Material$MaterialDesc$Sampler %)
        editable-samplers))
