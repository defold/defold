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
            [util.coll :as coll :refer [pair]]
            [util.text-util :as text-util])
  (:import [com.dynamo.gamesys.proto Gui$NodeDesc Gui$NodeDesc$Type Gui$SceneDesc Gui$SceneDesc$LayoutDesc]
           [com.google.protobuf Descriptors$Descriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType Message]))

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

(def ^:private pb-type-field-names
  {'dmGuiDDF.NodeDesc "type"})

(def ^:private pb-enum-ignored-values
  {'dmGuiDDF.NodeDesc.Type
   {"[TYPE_SPINE]" :deprecated}}) ; Migration tested in integration.extension-spine-test/legacy-spine-project-user-migration-test.

;; TODO: Add type fields from all union protobuf types.

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
     "name_hash" :runtime-only}

    [["particlefx" "emitters" "attributes"]
     ["sprite" "attributes"]]
    {"coordinate_space" :unused
     "data_type" :unused
     "element_count" :unused
     "normalize" :unused
     "semantic_type" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_BOX]"]
   {:default
    {"custom_type" :unused
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "overridden_fields" :non-editable
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "type" :allowed-default}

    [["gui" "layouts" "nodes"]]
    {"id" :non-overridable
     "parent" :non-overridable
     "template_node_child" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_CUSTOM]"]
   {:default
    {"custom_type" :non-overridable
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "overridden_fields" :non-editable
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :non-editable
     "slice9" :unused
     "spine_node_child" :deprecated ;; TODO: What was this?
     "template" :unused
     "template_node_child" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused
     "type" :non-overridable}

    [["gui" "layouts" "nodes"]]
    {"clipping_inverted" :allowed-default
     "clipping_mode" :allowed-default
     "clipping_visible" :allowed-default
     "enabled" :allowed-default
     "id" :non-overridable
     "inherit_alpha" :allowed-default
     "parent" :non-overridable
     "template_node_child" :unused
     "visible" :allowed-default}}

   ['dmGuiDDF.NodeDesc "[TYPE_PARTICLEFX]"]
   {:default
    {"blend_mode" :unused
     "clipping_inverted" :unused
     "clipping_mode" :unused
     "clipping_visible" :unused
     "custom_type" :unused
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "overridden_fields" :non-editable
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "pivot" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :non-editable
     "slice9" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused
     "type" :non-overridable}

    [["gui" "layouts" "nodes"]]
    {"enabled" :allowed-default
     "id" :non-overridable
     "inherit_alpha" :allowed-default
     "parent" :non-overridable
     "template_node_child" :unused
     "visible" :allowed-default}}

   ['dmGuiDDF.NodeDesc "[TYPE_PIE]"]
   {:default
    {"custom_type" :unused
     "font" :unused
     "line_break" :unused
     "outline" :unused
     "outline_alpha" :unused
     "overridden_fields" :non-editable
     "particlefx" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "slice9" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "type" :non-overridable}

    [["gui" "layouts" "nodes"]]
    {"id" :non-overridable
     "parent" :non-overridable
     "template_node_child" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_TEMPLATE]"]
   {:default
    {"adjust_mode" :unused
     "blend_mode" :unused
     "clipping_inverted" :unused
     "clipping_mode" :unused
     "clipping_visible" :unused
     "color" :non-editable
     "custom_type" :unused
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "material" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "overridden_fields" :non-editable
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "pivot" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :non-overridable
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused
     "type" :non-overridable
     "visible" :unused
     "xanchor" :unused
     "yanchor" :unused}

    [["gui" "layouts" "nodes"]]
    {"enabled" :allowed-default
     "id" :non-overridable
     "inherit_alpha" :allowed-default
     "parent" :non-overridable
     "template_node_child" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_TEXT]"]
   {:default
    {"clipping_inverted" :unused
     "clipping_mode" :unused
     "clipping_visible" :unused
     "custom_type" :unused
     "innerRadius" :unused
     "outerBounds" :unused
     "overridden_fields" :non-editable
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "texture" :unused
     "type" :non-overridable}

    [["gui" "layouts" "nodes"]]
    {"enabled" :allowed-default
     "id" :non-overridable
     "inherit_alpha" :allowed-default
     "line_break" :allowed-default
     "parent" :non-overridable
     "template_node_child" :unused
     "visible" :allowed-default}}

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
   {[["label" "size"]
     ["sprite" "size"]]
    {"w" :padding}

    [["gui" "nodes" "color"]
     ["gui" "nodes" "outline"]
     ["gui" "nodes" "position"]
     ["gui" "nodes" "rotation"]
     ["gui" "nodes" "scale"]
     ["gui" "nodes" "size"]
     ["gui" "nodes" "shadow"]
     ["gui" "layouts" "nodes" "color"]
     ["gui" "layouts" "nodes" "outline"]
     ["gui" "layouts" "nodes" "position"]
     ["gui" "layouts" "nodes" "rotation"]
     ["gui" "layouts" "nodes" "scale"]
     ["gui" "layouts" "nodes" "size"]
     ["gui" "layouts" "nodes" "shadow"]]
    {"w" :non-editable}}

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

(definline ^:private pb-descriptor-key [^Descriptors$Descriptor pb-desc]
  `(symbol (.getFullName ~pb-desc)))

(defn- pb-ignore-key [^Descriptors$Descriptor pb-desc type-token]
  {:pre [(or (nil? type-token) (s/valid? ::pb-type-token type-token))]}
  (let [pb-desc-key (pb-descriptor-key pb-desc)]
    (if type-token
      [pb-desc-key type-token]
      pb-desc-key)))

(defn- pb-field-ignore-reasons [pb-desc type-token pb-path]
  (let [pb-ignore-key (pb-ignore-key pb-desc type-token)
        pb-filter->pb-field->ignore-reason (get pb-ignored-fields pb-ignore-key)
        matched (filterv (fn [[pb-filter]]
                           (and (not= :default pb-filter)
                                (some #(= pb-path %) pb-filter)))
                         pb-filter->pb-field->ignore-reason)]
    (case (count matched)
      0 (:default pb-filter->pb-field->ignore-reason {})
      1 (into (:default pb-filter->pb-field->ignore-reason {})
              (val (first matched)))
      (throw (ex-info "The pb-path matches more than one filter in the pb-ignored-fields map."
                      {:pb-path pb-path
                       :matched matched})))))

(defn- valid-resource-ext? [str]
  (re-matches #"^[a-z0-9_]+$" str))

(defn- setting-valid-path-token? [str]
  (re-matches #"^[a-z][a-z0-9_]*$" str))

(defn- pb-valid-field-name? [str]
  (re-matches #"^[A-Za-z][A-Za-z0-9_]*$" str))

(defn- pb-valid-type-token? [str]
  (re-matches #"^\[.+?\]$" str))

(s/def ::class-java-symbol symbol?)
(s/def ::resource-type-ext (s/and string? valid-resource-ext?))
(s/def ::ignore-reason #{:allowed-default :deprecated :non-editable :non-overridable :padding :runtime-only :unimplemented :unused})
(s/def ::ignore-reason-set (s/coll-of ::ignore-reason :kind set?))

(s/def ::setting-path-token (s/and string? setting-valid-path-token?))
(s/def ::setting-path (s/coll-of ::setting-path-token :kind vector?))
(s/def ::setting->ignore-reason (s/map-of ::setting-path ::ignore-reason))
(s/def ::ext->setting->ignore-reason (s/map-of ::resource-type-ext ::setting->ignore-reason))

(s/def ::pb-field-name (s/and string? pb-valid-field-name?))
(s/def ::pb-type-token (s/and string? pb-valid-type-token?))
(s/def ::pb-identifier (s/or :field ::pb-field-name :type ::pb-type-token))
(s/def ::pb-ignore-key (s/or :class ::class-java-symbol :union-case (s/tuple ::class-java-symbol ::pb-type-token)))
(s/def ::pb-path-token ::pb-identifier)
(s/def ::pb-path (s/cat :ext ::resource-type-ext :field-path (s/* ::pb-path-token)))
(s/def ::pb-path-token->ignore-reason (s/map-of ::pb-path-token ::ignore-reason))
(s/def ::pb-filter (s/or :default #{:default} :paths (s/coll-of ::pb-path :kind vector?)))
(s/def ::pb-filter->pb-path-token->ignore-reason (s/map-of ::pb-filter ::pb-path-token->ignore-reason))
(s/def ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason (s/map-of ::pb-ignore-key ::pb-filter->pb-path-token->ignore-reason))

(deftest settings-ignored-paths-declaration-test
  (is (s/valid? ::ext->setting->ignore-reason settings-ignored-fields)
      (s/explain-str ::ext->setting->ignore-reason settings-ignored-fields)))

(deftest pb-ignored-fields-declaration-test
  (is (s/valid? ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason pb-ignored-fields)
      (s/explain-str ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason pb-ignored-fields)))

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

(def ^:private empty-sorted-coll-set (sorted-set-by coll-value-comparator))

(defn- editable-file-resource? [resource]
  (and (resource/file-resource? resource)
       (resource/editable? resource)
       (resource/openable? resource)
       (:write-fn (resource/resource-type resource))))

(defn- editable-resource-types-by-ext [workspace]
  (into (sorted-map)
        (filter #(:write-fn (val %)))
        (workspace/get-resource-type-map workspace :editable)))

(defn- checked-resources
  ([workspace]
   (checked-resources workspace nil))
  ([workspace pred]
   (let [root-level-editable-file-resources
         (->> (workspace/find-resource workspace "/")
              (resource/children)
              (filter editable-file-resource?))

         filtered-resources
         (cond->> root-level-editable-file-resources
                  pred (filter pred))]

     (->> filtered-resources
          (sort-by (juxt resource/type-ext resource/proj-path))
          (vec)))))

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

(definline ^:private pb-type-token
  ^String [type-field-value]
  `(.intern (str \[ ~type-field-value \])))

(definline ^:private pb-enum-field? [^Descriptors$FieldDescriptor field-desc]
  `(= Descriptors$FieldDescriptor$JavaType/ENUM (.getJavaType ~field-desc)))

(definline ^:private pb-message-field? [^Descriptors$FieldDescriptor field-desc]
  `(= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType ~field-desc)))

(defn- pb-enum-desc-usable-values-raw [^Descriptors$EnumDescriptor enum-desc]
  (let [values (.getValues enum-desc)
        last-index (dec (long (.size values)))]
    (into []
          (keep-indexed
            (fn [^long index ^Descriptors$EnumValueDescriptor value]
              (when-not (and (= last-index index)
                             (string/ends-with? (.getName value) "_COUNT"))
                value)))
          values)))

(def ^:private pb-enum-desc-usable-values (memoize pb-enum-desc-usable-values-raw))

(defn- pb-enum-desc-empty-frequencies-raw [^Descriptors$EnumDescriptor enum-desc]
  (let [type-token->ignore-reason (get pb-enum-ignored-values (pb-descriptor-key enum-desc))]
    (into (sorted-map)
          (keep (fn [enum-value-desc]
                  (let [type-token (pb-type-token enum-value-desc)]
                    (when-not (contains? type-token->ignore-reason type-token)
                      (pair type-token 0)))))
          (pb-enum-desc-usable-values enum-desc))))

(def ^:private pb-enum-desc-empty-frequencies (memoize pb-enum-desc-empty-frequencies-raw))

(defn- pb-field-has-single-valid-value? [^Descriptors$FieldDescriptor field-desc]
  (and (pb-enum-field? field-desc)
       (-> field-desc
           (.getEnumType)
           (pb-enum-desc-usable-values)
           (count)
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

(defn- pb-descriptor-expected-fields-raw [^Descriptors$Descriptor pb-desc type-token pb-path included-ignore-reasons]
  {:pre [(or (nil? type-token) (s/valid? ::pb-type-token type-token))
         (s/valid? ::pb-path pb-path)
         (s/valid? ::ignore-reason-set included-ignore-reasons)]}
  (let [pb-field->ignore-reason (pb-field-ignore-reasons pb-desc type-token pb-path)
        ignored-field? (fn [^Descriptors$FieldDescriptor field-desc]
                         (let [field-name (.getName field-desc)
                               ignore-reason (get pb-field->ignore-reason field-name)]
                           (and (some? ignore-reason)
                                (not (contains? included-ignore-reasons ignore-reason)))))]
    (into []
          (remove ignored-field?)
          (.getFields pb-desc))))

(def ^:private pb-descriptor-expected-fields (memoize pb-descriptor-expected-fields-raw))

(defn- pb-nested-field-frequencies [^Message pb pb-path count-field-value?]
  {:pre [(s/valid? ::pb-path pb-path)]}
  (let [pb-desc (.getDescriptorForType pb)
        pb-desc-key (pb-descriptor-key pb-desc)
        type-field-name (pb-type-field-names pb-desc-key)
        type-field-desc (some->> type-field-name (.findFieldByName pb-desc))
        type-field-value (some->> type-field-desc (.getField pb))
        type-token (some->> type-field-value pb-type-token)

        field-frequencies
        (into (sorted-map)
              (keep (fn [^Descriptors$FieldDescriptor field-desc]
                      (let [field-name (.getName field-desc)

                            field-frequency
                            (cond
                              (pb-message-field? field-desc)
                              (let [pb-path (conj pb-path field-name)]
                                (if (.isRepeated field-desc)
                                  (transduce
                                    (map #(pb-nested-field-frequencies % pb-path count-field-value?))
                                    merge-nested-frequencies
                                    (.getField pb field-desc))
                                  (pb-nested-field-frequencies (.getField pb field-desc) pb-path count-field-value?)))

                              (.isRepeated field-desc)
                              (if (pb-field-has-single-valid-value? field-desc)
                                (.getRepeatedFieldCount pb field-desc)
                                (util/count-where #(count-field-value? % field-desc)
                                                  (.getField pb field-desc)))

                              (.hasField pb field-desc)
                              (if (or (pb-field-has-single-valid-value? field-desc)
                                      (count-field-value? (.getField pb field-desc) field-desc))
                                1
                                0)

                              :else
                              0)]

                        (when (or (number? field-frequency)
                                  (pos? (count field-frequency)))
                          (pair field-name
                                field-frequency)))))
              (pb-descriptor-expected-fields pb-desc type-token pb-path #{:non-editable :non-overridable}))]

    (if (nil? type-field-desc)
      field-frequencies
      (cond-> (pb-enum-desc-empty-frequencies (.getEnumType type-field-desc))
              (pos? (count field-frequencies))
              (assoc type-token field-frequencies)))))

(defn- pb-read-resource
  ^Message [resource]
  ;; We do not use the :read-fn here since we want the rawest possible file contents.
  (let [resource-type (resource/resource-type resource)
        pb-class (-> resource-type :test-info :ddf-type)]
    (protobuf/read-pb pb-class resource)))

(defn- pb-field-value-non-default? [field-value ^Descriptors$FieldDescriptor field-desc]
  (not= (.getDefaultValue field-desc) field-value))

(defmulti ^:private nested-field-frequencies (fn [resource] (-> resource resource/resource-type :test-info :type)))

(defmethod nested-field-frequencies :code [resource]
  (sorted-map "lines" (if (string/blank? (slurp resource)) 0 1)))

(defmethod nested-field-frequencies :ddf [resource]
  (let [ext (resource/type-ext resource)
        pb (pb-read-resource resource)
        pb-path [ext]]
    (pb-nested-field-frequencies pb pb-path pb-field-value-non-default?)))

(defmethod nested-field-frequencies :settings [resource]
  (let [resource-type (resource/resource-type resource)
        resource-type-ext (:ext resource-type)
        ignore-reason-by-setting-path (get settings-ignored-fields resource-type-ext {})
        ignored-setting-path? #(contains? ignore-reason-by-setting-path %)
        meta-settings (-> resource-type :test-info :meta-settings)
        settings (with-open [reader (io/reader resource)]
                   (settings-core/parse-settings reader))]
    (transduce
      (comp cat
            (map :path)
            (distinct)
            (remove ignored-setting-path?)
            (map (fn [setting-path]
                   (let [value (settings-core/get-setting settings setting-path)]
                     (pair setting-path
                           (if (some? value) 1 0))))))
      (fn nested-map-rf
        ([nested-map] nested-map)
        ([nested-map [path value]]
         (coll/sorted-assoc-in nested-map path value)))
      (sorted-map)
      [meta-settings settings])))

(defn- uncovered-value-paths [resources]
  (->> resources
       (transduce (map nested-field-frequencies)
                  merge-nested-frequencies)
       (into empty-sorted-coll-set
             (comp coll/xform-nested-map->path-map
                   (keep (fn [[value-path ^long value-count]]
                           (when (zero? value-count)
                             value-path)))))))

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

(defn- gui-node-pb->id
  ^String [^Gui$NodeDesc node-pb]
  (.getId node-pb))

(defn- descending-slash-count+str [^String str]
  (pair (- (text-util/character-count str \/))
        str))

(defn- below-template-node-id? [node-id template-node-id]
  (and (= \/ (get node-id (count template-node-id)))
       (string/starts-with? node-id template-node-id)))

(defn- gui-template-node-override-infos [gui-node-pbs gui-proj-path->node-pbs]
  (let [[override-node-pbs template-node-pbs]
        (util/into-multiple
          [[] []]
          [(filter #(.getTemplateNodeChild ^Gui$NodeDesc %))
           (filter (fn [^Gui$NodeDesc node-pb]
                     (and (not (.getTemplateNodeChild node-pb))
                          (= Gui$NodeDesc$Type/TYPE_TEMPLATE (.getType node-pb)))))]
          gui-node-pbs)

        depth-ascending-template-node-ids
        (->> template-node-pbs
             (map gui-node-pb->id)
             (sort-by descending-slash-count+str))

        override-node-pb->template-node-id
        (fn override-node-pb->template-node-id [^Gui$NodeDesc override-node-pb]
          (let [override-node-id (gui-node-pb->id override-node-pb)]
            (util/first-where #(below-template-node-id? override-node-id %)
                              depth-ascending-template-node-ids)
            (some (fn [template-node-id]
                    (when (below-template-node-id? override-node-id template-node-id)
                      template-node-id))
                  depth-ascending-template-node-ids)))

        template-node-id->override-node-pbs
        (group-by override-node-pb->template-node-id
                  override-node-pbs)

        template-node-pb->override-infos
        (fn template-node-pb->override-infos [^Gui$NodeDesc template-node-pb]
          (let [template-node-id (gui-node-pb->id template-node-pb)

                original-node-id->pb
                (->> template-node-pb
                     (.getTemplate)
                     (gui-proj-path->node-pbs)
                     (coll/pair-map-by gui-node-pb->id))

                override-node-pb->override-info
                (fn override-node-pb->override-info [^Gui$NodeDesc override-node-pb]
                  (let [override-node-id (gui-node-pb->id override-node-pb)
                        original-node-id (subs override-node-id (inc (.length template-node-id))) ; Strip away template node id + slash char prefix.
                        original-node-pb (original-node-id->pb original-node-id)]
                    (assert (some? original-node-pb))
                    {:original-node-pb original-node-pb
                     :override-node-pb override-node-pb}))]

            (->> template-node-id
                 (template-node-id->override-node-pbs)
                 (mapv override-node-pb->override-info))))]

    (into []
          (mapcat template-node-pb->override-infos)
          template-node-pbs)))

(defn- pb-nested-field-differences [^Message original-pb ^Message altered-pb pb-path]
  {:pre [(s/valid? ::pb-path pb-path)
         (identical? (.getDescriptorForType original-pb)
                     (.getDescriptorForType altered-pb))]}
  (let [pb-desc (.getDescriptorForType original-pb)
        pb-desc-key (pb-descriptor-key pb-desc)
        type-field-name (pb-type-field-names pb-desc-key)
        type-field-desc (some->> type-field-name (.findFieldByName pb-desc))
        type-field-value (some->> type-field-desc (.getField original-pb))
        type-token (some->> type-field-value pb-type-token)

        diff-field
        (fn diff-field [^Descriptors$FieldDescriptor field-desc]
          (pair (.getName field-desc)
                (if (.isRepeated field-desc)
                  ;; Repeated field.
                  (if (not= (.getRepeatedFieldCount original-pb field-desc)
                            (.getRepeatedFieldCount altered-pb field-desc))
                    :count-mismatch
                    (mapv (if (pb-message-field? field-desc)
                            ;; Repeated message field.
                            (let [pb-path (conj pb-path (.getName field-desc))]
                              #(pb-nested-field-differences %1 %2 pb-path))
                            ;; Repeated primitive field.
                            #(if (= %1 %2) 0 1))
                          (.getField original-pb field-desc)
                          (.getField altered-pb field-desc)))
                  ;; Non-repeated field.
                  (let [a-value (when (.hasField original-pb field-desc)
                                  (.getField original-pb field-desc))
                        b-value (when (.hasField altered-pb field-desc)
                                  (.getField altered-pb field-desc))]
                    (if (pb-message-field? field-desc)
                      ;; Non-repeated message field.
                      (let [pb-path (conj pb-path (.getName field-desc))]
                        (pb-nested-field-differences a-value b-value pb-path))
                      ;; Non-repeated primitive-field.
                      (if (= a-value b-value) 0 1))))))]

    (into (sorted-map)
          (map diff-field)
          (pb-descriptor-expected-fields pb-desc type-token pb-path #{}))))

(defn- non-overridden-gui-node-field-paths [workspace diff-pb-path gui-resource->override-infos]
  (let [gui-resources (checked-resources workspace #(= "gui" (:ext %)))
        type-field-desc (.findFieldByName (Gui$NodeDesc/getDescriptor) "type")]
    (->> gui-resources
         (transduce
           (comp
             (mapcat gui-resource->override-infos)
             (map (fn [{:keys [^Gui$NodeDesc original-node-pb override-node-pb]}]
                    (let [node-type (.getType original-node-pb)
                          type-token (pb-type-token node-type)
                          differences (if (nil? override-node-pb)
                                        0
                                        (pb-nested-field-differences original-node-pb override-node-pb diff-pb-path))]
                      (sorted-map type-token differences)))))
           merge-nested-frequencies
           (pb-enum-desc-empty-frequencies (.getEnumType type-field-desc)))
         (into empty-sorted-coll-set
               (comp coll/xform-nested-map->path-map
                     (filter #(= 0 (val %)))
                     (map #(string/join " -> " (key %))))))))

(deftest all-gui-template-node-fields-overridden-test
  (test-util/with-loaded-project project-path
    (let [proj-path->resource #(test-util/resource workspace %)
          resource->gui-scene-pb (memoize #(protobuf/read-pb Gui$SceneDesc %))
          gui-scene-pb->node-pbs #(.getNodesList ^Gui$SceneDesc %)
          gui-proj-path->node-pbs (comp gui-scene-pb->node-pbs resource->gui-scene-pb proj-path->resource)

          gui-resource->template-override-infos
          (fn gui-resource->template-override-infos [resource]
            (let [scene-pb (resource->gui-scene-pb resource)
                  node-pbs (gui-scene-pb->node-pbs scene-pb)]
              (gui-template-node-override-infos node-pbs gui-proj-path->node-pbs)))

          non-template-overridden-gui-node-field-paths
          (non-overridden-gui-node-field-paths workspace ["gui" "nodes"] gui-resource->template-override-infos)]

      (is (= #{} non-template-overridden-gui-node-field-paths)
          (list-message
            (str "The following gui node fields are not covered by template overrides in any .gui files under `editor/" project-path "`:")
            non-template-overridden-gui-node-field-paths)))))

(deftest all-gui-layout-node-fields-overridden-test
  (test-util/with-loaded-project project-path
    (let [gui-resource->layout-override-infos
          (fn gui-resource->layout-override-infos [resource]
            (let [scene-pb (protobuf/read-pb Gui$SceneDesc resource)

                  original-node-id->override-node-pbs
                  (util/group-into
                    (sorted-map) []
                    gui-node-pb->id
                    (eduction
                      (mapcat (fn [^Gui$SceneDesc$LayoutDesc layout-pb]
                                (.getNodesList layout-pb)))
                      (.getLayoutsList scene-pb)))]

              (into []
                    (mapcat (fn [original-node-pb]
                              (let [original-node-id (gui-node-pb->id original-node-pb)
                                    override-node-pbs (original-node-id->override-node-pbs original-node-id)]
                                (if (coll/empty? override-node-pbs)
                                  [{:original-node-pb original-node-pb
                                    :override-node-pb nil}]
                                  (map (fn [override-node-pb]
                                         {:original-node-pb original-node-pb
                                          :override-node-pb override-node-pb})
                                       override-node-pbs)))))
                    (.getNodesList scene-pb))))

          non-layout-overridden-gui-node-field-paths
          (non-overridden-gui-node-field-paths workspace ["gui" "layouts" "nodes"] gui-resource->layout-override-infos)]

      (is (= #{} non-layout-overridden-gui-node-field-paths)
          (list-message
            (str "The following gui node fields are not covered by layout overrides in any .gui files under `editor/" project-path "`:")
            non-layout-overridden-gui-node-field-paths)))))
