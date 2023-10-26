;; Copyright 2020-2023 The Defold Foundation
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

(ns integration.save-data-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [util.coll :refer [pair]])
  (:import [com.google.protobuf DescriptorProtos$EnumDescriptorProto DescriptorProtos$EnumValueDescriptorProto Descriptors$Descriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType Message]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private project-path "test/resources/save_data_project")

;; Make it simple to re-run tests after adding content.
;; This project is not used by any other tests.
(test-util/evict-cached-system-and-project! project-path)

(def ^:private settings-ignored-fields
  ;; Deprecated settings in `game.project` are not automatically migrated, so
  ;; there are no tests for that. Instead, they appear as errors for the user to
  ;; address manually.
  {"project"
   {["bootstrap" "debug_init_script"] :deprecated
    ["display" "variable_dt"] :deprecated
    ["html5" "custom_heap_size"] :deprecated
    ["html5" "set_custom_heap_size"] :deprecated
    ["shader" "output_spirv"] :deprecated}})

(def ^:private pb-ignored-fields
  {'dmGameObjectDDF.CollectionDesc
   {:default
    {"component_types" :runtime-only
     "property_resources" :runtime-only}}

   'dmGameObjectDDF.CollectionInstanceDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in silent-migrations-test.

   'dmGameObjectDDF.ComponentDesc
   {:default
    {"property_decls" :runtime-only}}

   'dmGameObjectDDF.ComponentPropertyDesc
   {:default
    {"property_decls" :runtime-only}}

   'dmGameObjectDDF.EmbeddedInstanceDesc
   {:default
    {"component_properties" :unimplemented ; Edits are embedded in "data" field.
     "scale" :deprecated}} ; Migration tested in silent-migrations-test.

   'dmGameObjectDDF.InstanceDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in silent-migrations-test.

   'dmGameObjectDDF.PrototypeDesc
   {:default
    {"property_resources" :runtime-only}}

   'dmGameSystemDDF.LabelDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in integration.label-test/label-migration-test.

   'dmGameSystemDDF.SpineSceneDesc
   {:default
    {"sample_rate" :deprecated}} ; This was a legacy setting in our own Spine implementation. There is no equivalent in the official Spine runtime.

   'dmGameSystemDDF.TileLayer
   {:default
    {"id_hash" :runtime-only}}

   'dmGameSystemDDF.TileSet
   {:default
    {"convex_hull_points" :runtime-only}}

   'dmGraphics.VertexAttribute
   {:default
    {"binary_values" :runtime-only
     "coordinate_space" :unused
     "data_type" :unused
     "element_count" :unused
     "name_hash" :runtime-only
     "normalize" :unused
     "semantic_type" :unused}

    ["material" "attributes"]
    {"binary_values" :runtime-only
     "name_hash" :runtime-only}}

   'dmGuiDDF.SceneDesc
   {:default
    {"spine_scenes" :deprecated}} ; Migration tested in integration.extension-spine-test/legacy-spine-project-user-migration-test.

   'dmInputDDF.GamepadMapEntry
   {:default
    {"hat_mask" :runtime-only}}

   'dmMath.Point3
   {:default
    {"d" :padding}}

   'dmMath.Vector3
   {:default
    {"d" :padding}}

   'dmMath.Vector4
   (into {}
         (map #(pair % {"w" :padding}))
         #{["label" "size"]
           ["sprite" "size"]})

   'dmModelDDF.ModelDesc
   {:default
    {"materials" :unimplemented}} ; Multiple materials not supported yet.

   'dmRenderDDF.MaterialDesc
   {:default
    {"textures" :deprecated}} ; Migration tested in silent-migrations-test.

   'dmRenderDDF.MaterialDesc.Sampler
   {:default
    {"name_hash" :runtime-only
     "name_indirections" :runtime-only
     "texture" :unimplemented}}}) ; Default texture resources not supported yet.

(defn- valid-resource-ext? [str]
  (re-matches #"^[a-z0-9_]+$" str))

(defn- setting-valid-path-token? [str]
  (re-matches #"^[a-z][a-z0-9_]*$" str))

(defn- pb-valid-field-name? [str]
  (re-matches #"^[A-Za-z][A-Za-z0-9_]*$" str))

(s/def ::class-java-symbol symbol?)
(s/def ::resource-type-ext (s/and string? valid-resource-ext?))
(s/def ::ignore-reason #{:deprecated :padding :runtime-only :unimplemented :unused})

(s/def ::setting-path-token (s/and string? setting-valid-path-token?))
(s/def ::setting-path (s/coll-of ::setting-path-token :kind vector?))
(s/def ::setting->ignore-reason (s/map-of ::setting-path ::ignore-reason))
(s/def ::ext->setting->ignore-reason (s/map-of ::resource-type-ext ::setting->ignore-reason))

(s/def ::pb-field-name (s/and string? pb-valid-field-name?))
(s/def ::pb-field->ignore-reason (s/map-of ::pb-field-name ::ignore-reason))
(s/def ::pb-path (s/cat :ext ::resource-type-ext :field-path (s/* ::pb-field-name)))
(s/def ::pb-filter (s/or :default #{:default} :path ::pb-path))
(s/def ::pb-filter->pb-field->ignore-reason (s/map-of ::pb-filter ::pb-field->ignore-reason))
(s/def ::class->pb-filter->pb-field->ignore-reason (s/map-of ::class-java-symbol ::pb-filter->pb-field->ignore-reason))

(deftest settings-ignored-paths-declaration-test
  (is (s/valid? ::ext->setting->ignore-reason settings-ignored-fields)
      (s/explain-str ::ext->setting->ignore-reason settings-ignored-fields)))

(deftest pb-ignored-fields-declaration-test
  (is (s/valid? ::class->pb-filter->pb-field->ignore-reason pb-ignored-fields)
      (s/explain-str ::class->pb-filter->pb-field->ignore-reason pb-ignored-fields)))

(defn- coll-value-comparator
  "The standard comparison will order shorter vectors above longer ones.
  Here, we compare the values before length is taken into account."
  ^long [a b]
  (let [^long value-comparison
        (reduce (fn [^long _ ^long result]
                  (if (zero? result)
                    0
                    (reduced result)))
                0
                (map compare a b))]
    (if (zero? value-comparison)
      (compare (count a) (count b))
      value-comparison)))

(def ^:private sorted-coll-set (sorted-set-by coll-value-comparator))

(defn- editable-file-resource? [resource]
  (and (resource/file-resource? resource)
       (resource/editable? resource)
       (resource/openable? resource)
       (:write-fn (resource/resource-type resource))))

(defn- editable-resource-types-by-ext [workspace]
  (into (sorted-map)
        (filter #(:write-fn (val %)))
        (workspace/get-resource-type-map workspace :editable)))

(defn- checked-resources [workspace]
  (->> (workspace/find-resource workspace "/")
       (resource/children)
       (filter editable-file-resource?)
       (sort-by (juxt resource/type-ext resource/proj-path))
       (vec)))

(defn- list-message [message items]
  (string/join "\n" (cons message (map #(str "  " %) items))))

(defn- resource-ext-message [message resource-exts]
  (list-message message (map #(str \. %) resource-exts)))

(deftest all-resource-types-covered-test
  (test-util/with-loaded-project project-path
    (let [editable-resource-exts
          (into (sorted-set)
                (map key)
                (editable-resource-types-by-ext workspace))

          checked-resource-exts
          (into (sorted-set)
                (map #(:ext (resource/resource-type %)))
                (checked-resources workspace))

          non-covered-resource-exts
          (set/difference editable-resource-exts checked-resource-exts)]

      (is (= #{} non-covered-resource-exts)
          (resource-ext-message
            (str "The following editable resource types do not have files under `editor/" project-path "`:")
            non-covered-resource-exts)))))

(deftest editable-resource-types-have-valid-test-info
  (test-util/with-loaded-project project-path
    (let [problematic-resource-exts-by-issue-message
          (-> (util/group-into
                {} (sorted-set)
                (fn key-fn [[_ext resource-type]]
                  (cond
                    (nil? (:test-info resource-type))
                    "The following editable resource types did not specify :test-info when registered:"

                    (not (contains? (:test-info resource-type) :type))
                    "The following editable resource types did not specify :type in their :test-info when registered:"

                    (not (keyword? (:type (:test-info resource-type))))
                    "The following editable resource types specified an invalid :type in their :test-info when registered:"))
                (fn value-fn [[ext _resource-type]]
                  ext)
                (editable-resource-types-by-ext workspace))
              (dissoc nil))]

      (doseq [[issue-message problematic-resource-exts] problematic-resource-exts-by-issue-message]
        (is (= #{} problematic-resource-exts)
            (resource-ext-message issue-message problematic-resource-exts))))))

(defn- merge-nested-frequencies
  ([] 0)
  ([a] a)
  ([a b]
   (cond
     (and (integer? a) (integer? b))
     (+ (long a) (long b))

     (and (map? a) (map? b))
     (merge-with merge-nested-frequencies a b)

     (and (integer? a) (zero? (long a)))
     b

     (and (integer? b) (zero? (long b)))
     a

     :else
     (assert false))))

(def ^:private xform-nested-frequencies->paths
  (letfn [(path-entries [path [key value]]
            (let [path (conj path key)]
              (if (coll? value)
                (eduction
                  (mapcat #(path-entries path %))
                  value)
                [(pair path value)])))]
    (mapcat #(path-entries [] %))))

(defn- flatten-nested-frequencies [nested-frequencies]
  {:pre [(map? nested-frequencies)]}
  (into (empty nested-frequencies)
        xform-nested-frequencies->paths
        nested-frequencies))

(definline pb-enum-field? [^Descriptors$FieldDescriptor field-desc]
  `(= Descriptors$FieldDescriptor$JavaType/ENUM (.getJavaType ~field-desc)))

(definline pb-message-field? [^Descriptors$FieldDescriptor field-desc]
  `(= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType ~field-desc)))

(defn- pb-enum-desc-usable-value-count
  ^long [^DescriptorProtos$EnumDescriptorProto enum-desc-proto]
  (let [values (.getValueList enum-desc-proto)
        value-count (.size values)
        last-index (dec value-count)
        last-value ^DescriptorProtos$EnumValueDescriptorProto (.get values last-index)]
    (if (string/ends-with? (.getName last-value) "_COUNT")
      last-index
      value-count)))

(defn- pb-field-has-single-valid-value? [^Descriptors$FieldDescriptor field-desc]
  (and (pb-enum-field? field-desc)
       (-> field-desc
           (.getEnumType)
           (.toProto)
           (pb-enum-desc-usable-value-count)
           (== 1))))

(defn- pb-field-value-count
  ^long [^Message pb ^Descriptors$FieldDescriptor field-desc]
  (cond
    (.isRepeated field-desc)
    (if (pb-field-has-single-valid-value? field-desc)
      (.getRepeatedFieldCount pb field-desc)
      (util/count-where (partial not= (.getDefaultValue field-desc))
                        (.getField pb field-desc)))

    (.hasField pb field-desc)
    (if (or (not= (.getDefaultValue field-desc)
                  (.getField pb field-desc))
            (pb-field-has-single-valid-value? field-desc))
      1
      0)

    :else
    0))

(defn- pb-descriptor-expected-fields-raw [^Descriptors$Descriptor pb-desc pb-path]
  {:pre [(s/valid? ::pb-path pb-path)]}
  (let [pb-filter->pb-field->ignore-reason (get pb-ignored-fields (symbol (.getFullName pb-desc)))
        pb-field->ignore-reason (or (get pb-filter->pb-field->ignore-reason pb-path)
                                    (get pb-filter->pb-field->ignore-reason :default)
                                    {})
        ignored-field? (fn [^Descriptors$FieldDescriptor field-desc]
                         (contains? pb-field->ignore-reason (.getName field-desc)))]
    (into []
          (remove ignored-field?)
          (.getFields pb-desc))))

(def ^:private pb-descriptor-expected-fields (memoize pb-descriptor-expected-fields-raw))

(defn- pb-nested-field-frequencies [^Message pb pb-path]
  {:pre [(s/valid? ::pb-path pb-path)]}
  (into (sorted-map)
        (map (fn [^Descriptors$FieldDescriptor field-desc]
               (let [field-name (.getName field-desc)]
                 (pair field-name
                       (if (pb-message-field? field-desc)
                         (let [pb-path (conj pb-path field-name)]
                           (if (.isRepeated field-desc)
                             (transduce (map #(pb-nested-field-frequencies % pb-path))
                                        merge-nested-frequencies
                                        (.getField pb field-desc))
                             (pb-nested-field-frequencies (.getField pb field-desc) pb-path)))
                         (pb-field-value-count pb field-desc))))))
        (pb-descriptor-expected-fields (.getDescriptorForType pb) pb-path)))

(defmulti value-path-frequencies (fn [resource] (-> resource resource/resource-type :test-info :type)))

(defmethod value-path-frequencies :code [resource]
  {["lines"] (if (string/blank? (slurp resource)) 0 1)})

(defmethod value-path-frequencies :ddf [resource]
  ;; We do not use the :read-fn here since we want the rawest possible file contents.
  (let [resource-type (resource/resource-type resource)
        ext (:ext resource-type)
        pb-class (-> resource-type :test-info :ddf-type)
        pb (protobuf/read-pb pb-class resource)]
    (-> pb
        (pb-nested-field-frequencies [ext])
        (flatten-nested-frequencies))))

(defmethod value-path-frequencies :settings [resource]
  (let [resource-type (resource/resource-type resource)
        resource-type-ext (:ext resource-type)
        ignore-reason-by-setting-path (get settings-ignored-fields resource-type-ext {})
        ignored-setting-path? #(contains? ignore-reason-by-setting-path %)
        meta-settings (-> resource-type :test-info :meta-settings)
        settings (with-open [reader (io/reader resource)]
                   (settings-core/parse-settings reader))
        setting-paths (into #{}
                            (comp cat
                                  (map :path)
                                  (remove ignored-setting-path?))
                            [meta-settings settings])]
    (into (sorted-map)
          (map (fn [setting-path]
                 (let [value (settings-core/get-setting settings setting-path)]
                   (pair setting-path
                         (if (some? value) 1 0)))))
          setting-paths)))

(defn- uncovered-value-paths [resources]
  (->> resources
       (map value-path-frequencies)
       (apply merge-with +)
       (into sorted-coll-set
             (keep (fn [[value-path ^long value-count]]
                     (when (zero? value-count)
                       value-path))))))

(deftest all-fields-covered-test
  (test-util/with-loaded-project project-path
    (let [uncovered-value-paths-by-ext
          (->> (checked-resources workspace)
               (group-by (comp :ext resource/resource-type))
               (into (sorted-map)
                     (keep (fn [[ext resources]]
                             (some->> resources
                                      (uncovered-value-paths)
                                      (not-empty)
                                      (into (sorted-set)
                                            (map #(string/join " -> " %)))
                                      (pair ext))))))]
      (doseq [[ext uncovered-value-paths] uncovered-value-paths-by-ext]
        (is (= #{} uncovered-value-paths)
            (list-message
              (str "The following fields are not covered by any ." ext " files under `editor/" project-path "`:")
              uncovered-value-paths))))))

(deftest silent-migrations-test
  (test-util/with-loaded-project project-path
    (testing "collection"
      (let [uniform-scale-collection (test-util/resource-node project "/silently_migrated/uniform_scale.collection")
            referenced-collection (:node-id (test-util/outline uniform-scale-collection [0]))
            embedded-go (:node-id (test-util/outline uniform-scale-collection [1]))
            referenced-go (:node-id (test-util/outline uniform-scale-collection [2]))]
        (is (= collection/CollectionInstanceNode (g/node-type* referenced-collection)))
        (is (= collection/EmbeddedGOInstanceNode (g/node-type* embedded-go)))
        (is (= collection/ReferencedGOInstanceNode (g/node-type* referenced-go)))
        (is (= [2.0 2.0 2.0] (g/node-value referenced-collection :scale)))
        (is (= [2.0 2.0 2.0] (g/node-value embedded-go :scale)))
        (is (= [2.0 2.0 2.0] (g/node-value referenced-go :scale)))))

    (testing "material"
      (let [legacy-textures-material (test-util/resource-node project "/silently_migrated/legacy_textures.material")]
        (is (= [{:filter-mag :filter-mode-mag-linear
                 :filter-min :filter-mode-min-linear
                 :max-anisotropy 1.0
                 :name "albedo"
                 :wrap-u :wrap-mode-clamp-to-edge
                 :wrap-v :wrap-mode-clamp-to-edge}
                {:filter-mag :filter-mode-mag-linear
                 :filter-min :filter-mode-min-linear
                 :max-anisotropy 1.0
                 :name "normal"
                 :wrap-u :wrap-mode-clamp-to-edge
                 :wrap-v :wrap-mode-clamp-to-edge}]
               (g/node-value legacy-textures-material :samplers)))))))
