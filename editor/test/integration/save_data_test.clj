;; Copyright 2020-2025 The Defold Foundation
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
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn]
            [util.text-util :as text-util])
  (:import [com.dynamo.gamesys.proto Gui$NodeDesc Gui$NodeDesc$Builder Gui$NodeDesc$Type Gui$SceneDesc Gui$SceneDesc$LayoutDesc]
           [com.dynamo.proto DdfExtensions]
           [com.google.protobuf Descriptors$Descriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType Descriptors$GenericDescriptor Message]))

;; Note: We use symbol or string representations of protobuf types and values
;; instead of the imported classes and enum values when declaring exclusions and
;; field rules. This enables us to cover things that are dynamically loaded from
;; editor extensions, such as the Spine plugin.

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private project-path "test/resources/save_data_project")

;; Make it simple to re-run tests after adding content.
;; This project is not used by any other tests.
(test-util/evict-cached-project! project-path)

(def ^:private valid-ignore-reason?
  "This is the set of valid reasons why a setting or field may be ignored when
  we ensure every property is covered by the files in the save data test
  project. We want every property of every editable resource type to be set to a
  non-default value in at least one of the root-level files in the project."
  #{:allowed-default ; The value is allowed to be the default value for the field. This is useful for type enum fields that have a valid type as the default.
    :deprecated      ; The field is deprecated. This should be accompanied by a comment directing the reader to a test that covers the migration.
    :non-editable    ; The field is expected to be read and written to the file, but cannot be edited by the user.
    :non-overridable ; The field is expected to be read and written to the file, but cannot be overridden from its base value by the user.
    :padding         ; The field is only present in the protobuf declaration to ensure consecutive values are byte-aligned to a desired boundary in the compiled binaries for the runtime.
    :runtime-only    ; The field is only present in the compiled binaries for the runtime. This also happens if you annotate a field with [(runtime_only)=true] in a .proto file.
    :unimplemented   ; The field was added to support a new feature in the runtime, but is not yet fully implemented in the editor. We don't need to load it, as it cannot be in the project files yet. This will eventually lead to a file format change, deprecated fields, and a test that covers the migration.
    :unused})        ; The field is not expected to have a value in either the project files or the compiled binaries for the runtime. Typically used with union-style protobuf types such as dmGuiDDF.NodeDesc, where the expected fields are dictated by the value of the "type" field.

(def ^:private settings-ignored-fields
  "This structure is used to exclude certain key paths in setting files from
  having to be covered by the files in the save data test project. It is a map of
  file extensions to maps of setting path to ignore reasons."
  ;; Note: Deprecated settings in `game.project` are not automatically migrated,
  ;; so there are no migration tests for that. Instead, they appear as errors
  ;; for the user to address manually.
  {"project"
   {["bootstrap" "debug_init_script"] :deprecated
    ["display" "variable_dt"] :deprecated
    ["html5" "custom_heap_size"] :deprecated
    ["html5" "set_custom_heap_size"] :deprecated
    ["shader" "output_spirv"] :deprecated}})

(def ^:private pb-type-field-names
  "This structure is used to declare type-distinguishing fields in union-style
  protobuf types. Without this, we will consider a protobuf field covered if it
  is set to a non-default value anywhere at a particular field path. However, if
  you declare a type field name for a protobuf type, we will distinguish between
  field coverage based on the type value. The field name is expected to refer to
  an enum field in the specified protobuf type."
  {'dmBufferDDF.StreamDesc "value_type"
   'dmGameObjectDDF.PropertyDesc "type"
   'dmGameSystemDDF.LightDesc "type"
   'dmGraphics.VertexAttribute "data_type"
   'dmGuiDDF.NodeDesc "type"
   'dmInputDDF.GamepadMapEntry "type"
   'dmParticleDDF.Emitter "type"
   'dmParticleDDF.Modifier "type"
   'dmPhysicsDDF.CollisionShape.Shape "shape_type"
   'dmPhysicsDDF.ConvexShape "shape_type"})

(def ^:private pb-enum-ignored-values
  "This structure is used in conjunction with `pb-type-field-names` above to
  exclude certain enum values from consideration when determining coverage."
  {'dmGameObjectDDF.PropertyType
   {"[PROPERTY_TYPE_MATRIX4]" :unimplemented} ; There's currently no way to edit matrix script properties. But they can be declared and used at runtime.

   'dmGuiDDF.NodeDesc.Type
   {"[TYPE_SPINE]" :deprecated} ; Migration tested in integration.extension-spine-test/legacy-spine-project-user-migration-test.

   'dmPhysicsDDF.CollisionShape.Type
   {"[TYPE_HULL]" :runtime-only}}) ; If the .collisionobject file specifies a .convexshape for its collision_shape, it gets embedded as a TYPE_HULL in the compiled binary. We don't have any way of creating these from the editor yet.

(def ^:private pb-ignored-fields
  "This structure is used to exclude certain fields in protobuf-based file
  formats from having to be covered by the files in the save data test project.

  The leaf-level maps pair protobuf field names with an ignore reason.

  These field ignore rules are then associated with a context which can be
  either :default (which applies everywhere), or a vector of paths from the file
  extension through the field names leading up to the ignored fields.

  Finally, the context ignore rules are associated with a protobuf type
  identifier. This is the full name of the protobuf message as a symbol, or such
  a symbol wrapped in a vector alongside a bracketed string of the value of the
  field name associated with that symbol in the `pb-type-field-names` map.

  When determining if a field should be ignored, we first match the protobuf
  type without taking its type field into account. We then merge that with the
  ignore rules for the protobuf type field value specialization, if present.

  Finally, the field rules from the :default section is merged with the pb-path
  specialization field rules.

  Let's have a closer look at an example:

  ;; When we see a VertexAttribute anywhere, we know that the binary_values and
  ;; name_hash fields are only used in the compiled data for the engine runtime,
  ;; so we should not expect these fields to be set in the project files.
  'dmGraphics.VertexAttribute
   {:default ; This applies to all VertexAttributes.
    {\"binary_values\" :runtime-only
     \"name_hash\" :runtime-only}

    ;; When we see a VertexAttribute among the attributes list of an emitter
    ;; inside a .particlefx file, or below the attributes list in a .sprite,
    ;; we should not expect values for coordinate_space, data_type, and so on.
    ;; This is because the VertexAttribute protobuf type is used both to declare
    ;; attributes inside .material files and override their values in various
    ;; component files such as .sprite and .particlefx. Note that the ignored
    ;; fields from the :default section also applies, unless overwritten here.
    [[\"particlefx\" \"emitters\" \"[*]\" \"attributes\"]
     [\"sprite\" \"attributes\"]]
    {\"coordinate_space\" :unused
     \"data_type\" :unused
     \"element_count\" :unused
     \"normalize\" :unused
     \"semantic_type\" :unused}}

   ;; At another location within this file, we declare the data_type field to
   ;; determine the subtype of dmGraphics.VertexAttribute. And here we declare
   ;; specialized ignore rules for the case where the data_type is TYPE_FLOAT.
   ;; In this case, we don't want to enforce the rule that the data_type field
   ;; must be set to a non-default value, because TYPE_FLOAT is the default.
   ;; We also declare that we should not expect to see any long_values, because
   ;; floating-point values are instead stored in the double_values fields.
   ;; As above, these specialized ignore rules are merged with any other
   ;; matching ignore rules.
   ['dmGraphics.VertexAttribute \"[TYPE_FLOAT]\"]
   {:default
    {\"data_type\" :allowed-default
     \"long_values\" :unused}}"

  {'dmGameObjectDDF.CollectionInstanceDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmGameObjectDDF.EmbeddedInstanceDesc
   {:default
    {"component_properties" :unused ; Not used by the editor, Bob, or the runtime. Perhaps declared by mistake. Any edits to components are directly embedded in the PrototypeDesc inside the "data" field, so why do we need it?
     "scale" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmGameObjectDDF.InstanceDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   ['dmGameObjectDDF.PropertyDesc "[PROPERTY_TYPE_NUMBER]"]
   {:default
    {"type" :allowed-default}}

   'dmGameSystemDDF.LabelDesc
   {:default
    {"scale" :deprecated}} ; Migration tested in integration.label-test/label-migration-test.

   ['dmGameSystemDDF.LightDesc "[POINT]"]
   {:default
    {"type" :allowed-default
     "cone_angle" :unused
     "drop_off" :unused
     "penumbra_angle" :unused}}

   'dmGameSystemDDF.SpineSceneDesc
   {:default
    {"sample_rate" :deprecated}} ; This was a legacy setting in our own Spine implementation. There is no equivalent in the official Spine runtime.

   'dmGameSystemDDF.SpriteDesc
   {:default
    {"tile_set" :deprecated}} ; Replaced with 'textures'; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmGraphics.VertexAttribute
   {:default
    {"element_count" :deprecated} ; Migration tested in integration.save-data-test/silent-migrations-test.

    [["particlefx" "emitters" "[*]" "attributes"]
     ["sprite" "attributes"]
     ["model" "materials" "attributes"]]
    {"coordinate_space" :unused
     "data_type" :unused
     "normalize" :unused
     "semantic_type" :unused
     "step_function" :unused}}

   ['dmGraphics.VertexAttribute "[TYPE_FLOAT]"]
   {:default
    {"data_type" :allowed-default
     "long_values" :unused}}

   'dmGuiDDF.NodeDesc
   {:default
    {"overridden_fields" :non-editable ; Not editable, but used to determine which fields are overridden when loading.
     "type" :non-overridable}

    [["gui" "layouts" "nodes"]]
    {"id" :non-overridable
     "parent" :non-overridable
     "template_node_child" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_BOX]"]
   {:default
    {"custom_type" :unused
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "spine_create_bones" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "type" :allowed-default}}

   ['dmGuiDDF.NodeDesc "[TYPE_CUSTOM]"]
   {:default
    {"custom_type" :non-overridable
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_node_child" :deprecated ; This was a legacy setting in our own Spine implementation. The Spine/Rive extensions now create GUI bones themselves.
     "template" :unused
     "template_node_child" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused}}

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
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "pivot" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_create_bones" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_PIE]"]
   {:default
    {"custom_type" :unused
     "font" :unused
     "line_break" :unused
     "outline" :unused
     "outline_alpha" :unused
     "particlefx" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "slice9" :unused
     "spine_create_bones" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_TEMPLATE]"]
   {:default
    {"adjust_mode" :unused
     "blend_mode" :unused
     "clipping_inverted" :unused
     "clipping_mode" :unused
     "clipping_visible" :unused
     "color" :unused
     "custom_type" :unused
     "font" :unused
     "innerRadius" :unused
     "line_break" :unused
     "material" :unused
     "outerBounds" :unused
     "outline" :unused
     "outline_alpha" :unused
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "pivot" :unused
     "shadow" :unused
     "shadow_alpha" :unused
     "size" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_create_bones" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :non-overridable
     "text" :unused
     "text_leading" :unused
     "text_tracking" :unused
     "texture" :unused
     "visible" :unused
     "xanchor" :unused
     "yanchor" :unused}

    [["gui" "layouts" "nodes"]]
    {"template" :unused}}

   ['dmGuiDDF.NodeDesc "[TYPE_TEXT]"]
   {:default
    {"clipping_inverted" :unused
     "clipping_mode" :unused
     "clipping_visible" :unused
     "custom_type" :unused
     "innerRadius" :unused
     "outerBounds" :unused
     "particlefx" :unused
     "perimeterVertices" :unused
     "pieFillAngle" :unused
     "size_mode" :unused
     "slice9" :unused
     "spine_create_bones" :unused
     "spine_default_animation" :unused
     "spine_node_child" :unused
     "spine_scene" :unused
     "spine_skin" :unused
     "template" :unused
     "texture" :unused}}

   'dmGuiDDF.SceneDesc
   {:default
    {"background_color" :deprecated ; Migration tested in integration.save-data-test/silent-migrations-test.
     "spine_scenes" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   ['dmInputDDF.GamepadMapEntry "[GAMEPAD_TYPE_AXIS]"]
   {:default
    {"hat_mask" :unused
     "type" :allowed-default}}

   ['dmInputDDF.GamepadMapEntry "[GAMEPAD_TYPE_BUTTON]"]
   {:default
    {"hat_mask" :unused
     "mod" :unused}}

   'dmMath.Point3
   {:default
    {"d" :padding}}

   'dmMath.Vector3
   {:default
    {"d" :padding}}

   'dmMath.Vector3One
   {:default
    {"d" :padding}}

   'dmMath.Vector4
   {[["label" "size"]
     ["sprite" "size"]]
    {"w" :padding}

    [["gui" "nodes" "[*]" "position"]
     ["gui" "nodes" "[*]" "rotation"]
     ["gui" "nodes" "[*]" "size"]
     ["gui" "layouts" "nodes" "[*]" "position"]
     ["gui" "layouts" "nodes" "[*]" "rotation"]
     ["gui" "layouts" "nodes" "[*]" "size"]]
    {"w" :padding}}

   'dmMath.Vector4One
   {[["gui" "nodes" "[*]" "color"]
     ["gui" "nodes" "[*]" "scale"]
     ["gui" "layouts" "nodes" "[*]" "color"]
     ["gui" "layouts" "nodes" "[*]" "scale"]]
    {"w" :padding}}

   'dmMath.Vector4WOne
   {[["gui" "nodes" "[*]" "outline"]
     ["gui" "nodes" "[*]" "shadow"]
     ["gui" "layouts" "nodes" "[*]" "outline"]
     ["gui" "layouts" "nodes" "[*]" "shadow"]]
    {"w" :padding}}

   'dmModelDDF.ModelDesc
   {:default
    {"material" :deprecated   ; Migration tested in integration.save-data-test/silent-migrations-test.
     "textures" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   ['dmParticleDDF.Emitter "[EMITTER_TYPE_CIRCLE]"]
   {:default
    {"type" :allowed-default}}

   ['dmParticleDDF.Modifier "[MODIFIER_TYPE_ACCELERATION]"]
   {:default
    {"type" :allowed-default}}

   'dmParticleDDF.Modifier.Property
   {[["particlefx" "modifiers" "[MODIFIER_TYPE_ACCELERATION]" "properties"]
     ["particlefx" "modifiers" "[MODIFIER_TYPE_DRAG]" "properties"]
     ["particlefx" "emitters" "[*]" "modifiers" "[MODIFIER_TYPE_ACCELERATION]" "properties"]
     ["particlefx" "emitters" "[*]" "modifiers" "[MODIFIER_TYPE_DRAG]" "properties"]]
    {"key" :allowed-default}}

   'dmPhysicsDDF.CollisionShape.Shape
   {:default
    {"index" :allowed-default}}

   ['dmPhysicsDDF.CollisionShape.Shape "[TYPE_SPHERE]"]
   {:default
    {"shape_type" :allowed-default}}

   ['dmPhysicsDDF.ConvexShape "[TYPE_SPHERE]"]
   {:default
    {"shape_type" :allowed-default}}

   'dmRenderDDF.FontDesc
   {:default
    {"extra_characters" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmRenderDDF.MaterialDesc
   {:default
    {"textures" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmRenderDDF.MaterialDesc.Sampler
   {:default
    {"texture" :unimplemented}} ; Default texture resources not supported yet.

   'dmRenderDDF.RenderPrototypeDesc
   {:default
    {"materials" :deprecated}} ; Migration tested in integration.save-data-test/silent-migrations-test.

   'dmRenderDDF.RenderTargetDesc.DepthStencilAttachment
   {:default
    {"format" :unimplemented}} ; Non-default depth/stencil format not supported yet.

   'dmRigDDF.AnimationSetDesc
   {:default
    {"skeleton" :deprecated}}}) ; Non-default depth/stencil format not supported yet.

(definline ^:private pb-descriptor-key [^Descriptors$Descriptor pb-desc]
  `(symbol (.getFullName ~(with-meta pb-desc {:tag `Descriptors$GenericDescriptor}))))

(defn- pb-path-matches-filter-path? [pb-path pb-filter-path]
  (s/assert ::pb-path pb-path)
  (s/assert ::pb-filter-path pb-filter-path)
  (and (== (count pb-path)
           (count pb-filter-path))
       (every? true?
               (map (fn [pb-path-token pb-filter-path-token]
                      (or (= "[*]" pb-filter-path-token) ; TODO: Implement wildcards properly.
                          (= pb-path-token pb-filter-path-token)))
                    pb-path
                    pb-filter-path))))

(defn- pb-field-option-ignore-rules-raw [^Descriptors$Descriptor pb-desc]
  (let [runtime-only-field-option-field-desc (.getDescriptor DdfExtensions/runtimeOnly)]
    {:default
     (into {}
           (keep (fn [^Descriptors$FieldDescriptor field-desc]
                   (let [field-name (.getName field-desc)
                         field-options (.getOptions field-desc)]
                     (when (.getField field-options runtime-only-field-option-field-desc)
                       (pair field-name :runtime-only)))))
           (.getFields pb-desc))}))

(def ^:private pb-field-option-ignore-rules (fn/memoize pb-field-option-ignore-rules-raw))

(defn- pb-field-ignore-rules-raw [^Descriptors$Descriptor pb-desc type-token]
  (let [pb-desc-key (pb-descriptor-key pb-desc)]
    (merge-with
      merge
      (pb-field-option-ignore-rules pb-desc)
      (get pb-ignored-fields pb-desc-key)
      (when type-token
        (get pb-ignored-fields [pb-desc-key type-token])))))

(def ^:private pb-field-ignore-rules (fn/memoize pb-field-ignore-rules-raw))

(defn- pb-field-ignore-reasons [^Descriptors$Descriptor pb-desc type-token pb-path]
  (s/assert (s/nilable ::pb-type-token) type-token)
  (s/assert ::pb-path pb-path)
  (let [pb-filter->pb-field->ignore-reason (pb-field-ignore-rules pb-desc type-token)

        matched
        (filterv (fn [[pb-filter]]
                   (and (not= :default pb-filter)
                        (some #(pb-path-matches-filter-path? pb-path %)
                              pb-filter)))
                 pb-filter->pb-field->ignore-reason)]

    (case (count matched)
      0 (:default pb-filter->pb-field->ignore-reason {})
      1 (into (:default pb-filter->pb-field->ignore-reason {})
              (val (first matched)))
      (throw (ex-info "The pb-path matches more than one filter in the `pb-ignored-fields` map."
                      {:pb-path pb-path
                       :matched matched})))))

(defn- valid-resource-ext? [str]
  (re-matches #"^[a-z0-9_]+$" str))

(defn- setting-valid-path-token? [str]
  (re-matches #"^[a-z][a-z0-9_]*$" str))

(defn- pb-valid-desc-key-symbol? [sym]
  (re-matches #"^[_a-z][_a-zA-Z0-9]+(\.[_a-zA-Z0-9]+)+$" (name sym)))

(defn- pb-valid-field-name? [str]
  (re-matches #"^[A-Za-z][A-Za-z0-9_]*$" str))

(defn- pb-valid-type-token? [str]
  (re-matches #"^\[.+?\]$" str))

(s/def ::class-java-symbol symbol?)
(s/def ::resource-type-ext (s/and string? valid-resource-ext?))
(s/def ::ignore-reason valid-ignore-reason?)
(s/def ::ignore-reason-set (s/coll-of ::ignore-reason :kind set?))

(s/def ::setting-path-token (s/and string? setting-valid-path-token?))
(s/def ::setting-path (s/coll-of ::setting-path-token :kind vector?))
(s/def ::setting->ignore-reason (s/map-of ::setting-path ::ignore-reason))
(s/def ::ext->setting->ignore-reason (s/map-of ::resource-type-ext ::setting->ignore-reason))

(s/def ::pb-desc-key (s/and symbol? pb-valid-desc-key-symbol?))
(s/def ::pb-field-name (s/and string? pb-valid-field-name?))
(s/def ::pb-type-token (s/and string? pb-valid-type-token?))
(s/def ::pb-identifier (s/or :field ::pb-field-name :type ::pb-type-token))
(s/def ::pb-ignore-key (s/or :class ::class-java-symbol :union-case (s/tuple ::class-java-symbol ::pb-type-token)))
(s/def ::pb-path-token ::pb-identifier)
(s/def ::pb-path (s/cat :ext ::resource-type-ext :field-path (s/* ::pb-path-token)))
(s/def ::pb-path-token->ignore-reason (s/map-of ::pb-path-token ::ignore-reason))
(s/def ::pb-filter-path-token (s/or :identifier ::pb-identifier :wildcard #{"[*]"})) ; TODO: Implement wildcards properly.
(s/def ::pb-filter-path (s/cat :ext ::resource-type-ext :field-filter-path (s/* ::pb-filter-path-token)))
(s/def ::pb-filter (s/or :default #{:default} :filter-paths (s/coll-of ::pb-filter-path :kind vector?)))
(s/def ::pb-filter->pb-path-token->ignore-reason (s/map-of ::pb-filter ::pb-path-token->ignore-reason))
(s/def ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason (s/map-of ::pb-ignore-key ::pb-filter->pb-path-token->ignore-reason))

(deftest settings-ignored-paths-declaration-test
  ;; This test is intended to verify that the structure we use to ignore certain
  ;; setting file paths is valid. If it fails, check the structure of the
  ;; `settings-ignored-fields` declaration at the top of this file.
  (is (s/valid? ::ext->setting->ignore-reason settings-ignored-fields)
      (s/explain-str ::ext->setting->ignore-reason settings-ignored-fields)))

(deftest pb-ignored-fields-declaration-test
  ;; This test is intended to verify that the structure we use to ignore certain
  ;; protobuf fields is valid. If it fails, check the structure of the
  ;; `pb-ignored-fields` declaration at the top of this file.
  (is (s/valid? ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason pb-ignored-fields)
      (s/explain-str ::pb-ignore-key->pb-filter->pb-path-token->ignore-reason pb-ignored-fields)))

(def texture-profile-format-combinations
  (let [formats [:texture-format-luminance
                 :texture-format-luminance-alpha
                 :texture-format-rgb
                 :texture-format-rgb-16bpp
                 :texture-format-rgba
                 :texture-format-rgba-16bpp]
        compressors [{:compressor "Uncompressed"
                      :presets ["UNCOMPRESSED"]}
                     {:compressor "BasisU"
                      :presets ["BASISU_LOW" "BASISU_MEDIUM" "BASISU_HIGH" "BASISU_HIGHEST"]}]]
    (for [format formats
          {:keys [compressor presets]} compressors
          preset presets]
      {:format format
       :compressor compressor
       :compressor-preset preset})))

(deftest silent-migrations-test
  ;; This test is intended to verify that certain silent data migrations are
  ;; performed correctly. A silent migration typically involves a :sanitize-fn
  ;; to silently convert the read data structure into the updated save data
  ;; structure. This ensures the file will not be saved in the updated format
  ;; until the user changes something significant in the file. More involved
  ;; migrations might be covered by tests elsewhere.
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)

    (testing "collection"
      (let [uniform-scale-collection (project/get-resource-node project "/silently_migrated/uniform_scale.collection")
            referenced-collection (:node-id (test-util/outline uniform-scale-collection [0]))
            referenced-go (:node-id (test-util/outline uniform-scale-collection [1]))
            embedded-go (:node-id (test-util/outline uniform-scale-collection [2]))]
        (is (= collection/CollectionInstanceNode (g/node-type* referenced-collection)))
        (is (= collection/EmbeddedGOInstanceNode (g/node-type* embedded-go)))
        (is (= collection/ReferencedGOInstanceNode (g/node-type* referenced-go)))
        (is (= [2.0 2.0 2.0] (g/node-value referenced-collection :scale)))
        (is (= [2.0 2.0 2.0] (g/node-value embedded-go :scale)))
        (is (= [2.0 2.0 2.0] (g/node-value referenced-go :scale)))))

    (testing "font"
      (let [extra-characters-font (project/get-resource-node project "/silently_migrated/extra_characters.font")]
        (is (= " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~åäö"
               (g/node-value extra-characters-font :characters)))))

    (testing "gui"
      (let [background-color-gui (test-util/resource-node project "/silently_migrated/background_color.gui")]
        (is (not (contains? (g/node-value background-color-gui :source-value) :background-color)))
        (is (not (contains? (g/node-value background-color-gui :save-value) :background-color))))
      (let [redundant-sizes-gui (test-util/resource-node project "/silently_migrated/redundant_sizes.gui")]
        (is (= (g/node-value redundant-sizes-gui :source-value)
               (g/node-value redundant-sizes-gui :save-value))))
      (let [redundant-layout-field-values-gui (test-util/resource-node project "/silently_migrated/redundant_layout_field_values.gui")]
        (is (= (g/node-value redundant-layout-field-values-gui :source-value)
               (g/node-value redundant-layout-field-values-gui :save-value))))
      (let [redundant-template-field-values-gui (test-util/resource-node project "/silently_migrated/redundant_template_field_values.gui")]
        (is (= (g/node-value redundant-template-field-values-gui :source-value)
               (g/node-value redundant-template-field-values-gui :save-value))))
      (let [legacy-spine-resources-gui (test-util/resource-node project "/silently_migrated/legacy_spine_resources.gui")]
        (is (= [{:name "first_spinescene"
                 :path "/checked.spinescene"}]
               (g/node-value legacy-spine-resources-gui :resource-msgs)))))

    (testing "material"
      (let [legacy-textures-material (project/get-resource-node project "/silently_migrated/legacy_textures.material")]
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
               (g/node-value legacy-textures-material :samplers))))
      (let [legacy-element-count-material (project/get-resource-node project "/silently_migrated/legacy_vertex_attribute_element_count.material")
            legacy-attributes (g/node-value legacy-element-count-material :attributes)
            vector-types-by-attribute-name (into (sorted-map)
                                                 (map (fn [attribute]
                                                        (pair (:name attribute)
                                                              (select-keys attribute [:vector-type :values]))))
                                                 legacy-attributes)]
        (is (= {"legacy_count_1" {:vector-type :vector-type-scalar
                                  :values [1.1]}
                "legacy_count_2" {:vector-type :vector-type-vec2
                                  :values [1.1 1.2]}
                "legacy_count_3" {:vector-type :vector-type-vec3
                                  :values [1.1 1.2 1.3]}
                "legacy_count_4" {:vector-type :vector-type-vec4
                                  :values [1.1 1.2 1.3 1.4]}}
               vector-types-by-attribute-name))))

    (testing "render"
      (let [legacy-render-prototype (project/get-resource-node project "/silently_migrated/legacy_render_prototype.render")]
        (is (= [{:name "test"
                 :path "/builtins/materials/sprite.material"}]
               (:render-resources (g/node-value legacy-render-prototype :save-value))))))

    (testing "model"
      (let [legacy-material-and-textures-model (project/get-resource-node project "/silently_migrated/legacy_material_and_textures.model")
            material-resource (workspace/find-resource workspace "/builtins/materials/model.material")
            tex0-resource (workspace/find-resource workspace "/referenced/images/red.png")
            tex1-resource (workspace/find-resource workspace "/referenced/images/green.png")]
        (is (= [{:material material-resource
                 :name "default"
                 :textures [{:sampler "tex0"
                             :texture tex0-resource}
                            {:sampler "tex1"
                             :texture tex1-resource}]
                 :attributes {}}]
               (g/node-value legacy-material-and-textures-model :materials)))))

    (testing "sprite"
      (let [legacy-tile-set-sprite (project/get-resource-node project "/silently_migrated/legacy_tile_set.sprite")]
        (is (= [{:sampler "texture_sampler"
                 :texture (workspace/find-resource workspace "/checked.atlas")}]
               (g/node-value legacy-tile-set-sprite :textures))))
      (let [legacy-tile-set-sprite-go (project/get-resource-node project "/silently_migrated/legacy_tile_set_sprite.go")
            embedded-component (:node-id (test-util/outline legacy-tile-set-sprite-go [0]))
            embedded-sprite (test-util/to-component-resource-node-id embedded-component)]
        (is (= [{:sampler "texture_sampler"
                 :texture (workspace/find-resource workspace "/checked.atlas")}]
               (g/node-value embedded-sprite :textures)))))

    (testing "texture_profiles"
      (let [legacy-texture-profiles (project/get-resource-node project "/silently_migrated/legacy_texture_profile_formats.texture_profiles")
            legacy-texture-profiles-save-value (g/node-value legacy-texture-profiles :save-value)
            legacy-texture-profiles-formats (set (get-in legacy-texture-profiles-save-value [:profiles 0 :platforms 0 :formats]))
            all-format-combinations (set texture-profile-format-combinations)]
        (is (= legacy-texture-profiles-formats all-format-combinations))))))

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
       (let [resource-type (resource/resource-type resource)]
         (if (resource/placeholder-resource-type? resource-type)
           (not (text-util/binary? resource))
           (some? (:write-fn resource-type))))))

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
  ;; This test is intended to verify that every editable resource type has one
  ;; or more files at root-level in the save data test project. If you've
  ;; registered a new editable resource type with the workspace, you need to add
  ;; a file for it in the save data test project. You will also need to ensure
  ;; non-default values are assigned to all properties, which is enforced by
  ;; `all-fields-covered-test` below.
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
            (format "The following editable resource types do not have files under `editor/%s`:"
                    project-path)
            non-covered-resource-exts)))))

(deftest editable-resource-types-have-valid-test-info
  ;; This test is intended to verify that every resource type registered with
  ;; the workspace has a valid :test-info map associated with it. The high-level
  ;; functions such as `resource-node/register-ddf-resource-type` will add this
  ;; automatically, but if you register a resource type using the low-level
  ;; `workspace/register-resource-type` function, you'll need to specify
  ;; :test-info as a map of {:type [keyword]} and additional keys dependent on
  ;; the :type. The tests need this information to be able to check that every
  ;; property has a non-default value in the save data project.
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

(def ^:private pb-enum-desc-usable-values (fn/memoize pb-enum-desc-usable-values-raw))

(defn- pb-enum-desc-empty-frequencies-raw [^Descriptors$EnumDescriptor enum-desc]
  (let [type-token->ignore-reason (get pb-enum-ignored-values (pb-descriptor-key enum-desc))]
    (into (sorted-map)
          (keep (fn [enum-value-desc]
                  (let [type-token (pb-type-token enum-value-desc)
                        ignore-reason (get type-token->ignore-reason type-token)]
                    (case ignore-reason
                      (nil :allowed-default :non-editable :non-overridable) (pair type-token 0)
                      nil))))
          (pb-enum-desc-usable-values enum-desc))))

(def ^:private pb-enum-desc-empty-frequencies (fn/memoize pb-enum-desc-empty-frequencies-raw))

(defn- pb-field-has-single-valid-value? [^Descriptors$FieldDescriptor field-desc]
  ;; For protobuf fields that have a single valid value, we don't enforce the
  ;; rule that every field needs to have a non-default value somewhere.
  ;; We have some enum types that have only a single valid value.
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

(defn- pb-descriptor-expected-fields-raw [^Descriptors$Descriptor pb-desc type-token pb-path disregarded-ignore-reasons]
  (s/assert (s/nilable ::pb-type-token) type-token)
  (s/assert ::pb-path pb-path)
  (s/assert ::ignore-reason-set disregarded-ignore-reasons)
  (let [pb-field->ignore-reason (pb-field-ignore-reasons pb-desc type-token pb-path)
        ignored-field? (fn [^Descriptors$FieldDescriptor field-desc]
                         (let [field-name (.getName field-desc)
                               ignore-reason (get pb-field->ignore-reason field-name)]
                           (and (some? ignore-reason)
                                (not (contains? disregarded-ignore-reasons ignore-reason)))))]
    (into []
          (remove ignored-field?)
          (.getFields pb-desc))))

(def ^:private pb-descriptor-expected-fields (fn/memoize pb-descriptor-expected-fields-raw))

(defn- pb-type-field-name [^Descriptors$Descriptor pb-desc pb-path]
  (s/assert ::pb-path pb-path)
  (when-let [type-field-name (-> pb-desc pb-descriptor-key pb-type-field-names)]
    (let [pb-field->ignore-reason (pb-field-ignore-reasons pb-desc nil pb-path)
          ignore-reason (get pb-field->ignore-reason type-field-name)]
      (case ignore-reason
        (nil :allowed-default :non-editable :non-overridable) type-field-name
        nil))))

(defn- pb-nested-field-frequencies [^Message pb pb-path count-field-value?]
  (s/assert ::pb-path pb-path)
  (let [pb-desc (.getDescriptorForType pb)
        type-field-name (pb-type-field-name pb-desc pb-path)
        type-field-desc (some->> type-field-name (.findFieldByName pb-desc))
        type-field-value (some->> type-field-desc (.getField pb))
        type-token (some->> type-field-value pb-type-token)

        typed-pb-path
        (if type-token
          (conj pb-path type-token)
          pb-path)

        field-frequencies
        (into (sorted-map)
              (keep (fn [^Descriptors$FieldDescriptor field-desc]
                      (let [field-name (.getName field-desc)

                            field-frequency
                            (cond
                              (pb-message-field? field-desc)
                              (let [pb-path (conj typed-pb-path field-name)]
                                (if (.isRepeated field-desc)
                                  (transduce
                                    (map #(pb-nested-field-frequencies % pb-path count-field-value?))
                                    merge-nested-frequencies
                                    (.getField pb field-desc))
                                  (pb-nested-field-frequencies (.getField pb field-desc) pb-path count-field-value?)))

                              (.isRepeated field-desc)
                              (.getRepeatedFieldCount pb field-desc) ; Repeated fields cannot specify a default, so any values count.

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
  ;; We do not use the read-fn here since we want the rawest possible file contents.
  (let [resource-type (resource/resource-type resource)
        pb-class (-> resource-type :test-info :ddf-type)]
    (protobuf/read-pb pb-class resource)))

(defn- pb-field-value-non-default? [field-value ^Descriptors$FieldDescriptor field-desc]
  (not= (.getDefaultValue field-desc) field-value))

(defmulti ^:private nested-field-frequencies
  (fn [resource]
    (let [resource-type (resource/resource-type resource)]
      (if (resource/placeholder-resource-type? resource-type)
        (if (text-util/binary? resource)
          :binary
          :code)
        (:type (:test-info resource-type))))))

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
        meta-settings (-> resource-type :test-info :meta-settings)
        ignore-reason-by-setting-path (get settings-ignored-fields resource-type-ext {})

        ignored-setting-path?
        (fn ignored-setting-path? [setting-path]
          (let [ignore-reason (get ignore-reason-by-setting-path setting-path)]
            (case ignore-reason
              (nil :allowed-default :non-editable :non-overridable) false
              true)))

        settings
        (with-open [reader (io/reader resource)]
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
         (coll/assoc-in-ex nested-map path value coll/sorted-assoc-in-empty-fn)))
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
  ;; This test is intended to verify that every property across all editable
  ;; files has a non-default value in the save data test project, so we can be
  ;; sure all properties are read and saved property by the editor. If you add
  ;; fields to the protobuf messages used by the editor, you must either add a
  ;; field ignore rule to the `pb-ignored-fields` map at the top of this file,
  ;; or set the field to a non-default value in a root-level file in the save
  ;; data test project. Alternatively, if the field is only for the compiled
  ;; binaries read by the runtime, you can annotate it [(runtime_only)=true]
  ;; directly in the .proto file.
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
              (format "The following fields are not covered by any .%s files under `editor/%s`:"
                      ext
                      project-path)
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

(defn- apply-gui-overrides
  ^Gui$NodeDesc [^Gui$NodeDesc original-node-pb ^Gui$NodeDesc override-node-pb]
  (let [overridden-pb-field-index? (set (.getOverriddenFieldsList override-node-pb))

        ^Gui$NodeDesc$Builder overridden-node-pb-builder
        (reduce
          (fn [^Gui$NodeDesc$Builder overridden-node-pb-builder ^Descriptors$FieldDescriptor field-desc]
            (if (or (if (.isRepeated field-desc)
                      (pos? (.getRepeatedFieldCount override-node-pb field-desc))
                      (.hasField override-node-pb field-desc))
                    (overridden-pb-field-index? (.getNumber field-desc)))
              (.setField overridden-node-pb-builder field-desc (.getField override-node-pb field-desc))
              overridden-node-pb-builder))
          (.toBuilder original-node-pb)
          (.getFields (.getDescriptorForType original-node-pb)))]

    (.build overridden-node-pb-builder)))

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
                     :override-node-pb (if override-node-pb
                                         (apply-gui-overrides original-node-pb override-node-pb)
                                         original-node-pb)}))]

            (->> template-node-id
                 (template-node-id->override-node-pbs)
                 (mapv override-node-pb->override-info))))]

    (into []
          (mapcat template-node-pb->override-infos)
          template-node-pbs)))

(defn- pb-nested-field-differences [^Message original-pb ^Message altered-pb pb-path]
  {:pre [(identical? (.getDescriptorForType original-pb)
                     (.getDescriptorForType altered-pb))]}
  (s/assert ::pb-path pb-path)
  (let [pb-desc (.getDescriptorForType original-pb)
        type-field-name (pb-type-field-name pb-desc pb-path)
        type-field-desc (some->> type-field-name (.findFieldByName pb-desc))
        type-field-value (some->> type-field-desc (.getField original-pb))
        type-token (some->> type-field-value pb-type-token)

        typed-pb-path
        (if type-token
          (conj pb-path type-token)
          pb-path)

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
                            (let [pb-path (conj typed-pb-path (.getName field-desc))]
                              #(pb-nested-field-differences %1 %2 pb-path))
                            ;; Repeated primitive field.
                            #(if (= %1 %2) 0 1))
                          (.getField original-pb field-desc)
                          (.getField altered-pb field-desc)))
                  ;; Non-repeated field.
                  (let [a-value (.getField original-pb field-desc)
                        b-value (.getField altered-pb field-desc)]
                    (if (pb-message-field? field-desc)
                      ;; Non-repeated message field.
                      (let [pb-path (conj typed-pb-path (.getName field-desc))]
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

(deftest all-gui-layout-node-fields-overridden-test
  ;; This test is intended to verify that every field in dmGuiDDF.NodeDesc is
  ;; being overridden by a layout in one of the root-level files in the save
  ;; data test project. If you add a field to the NodeDesc protobuf message,
  ;; you'll either need to add a layout override for it (we suggest you add it
  ;; to the Landscape layout in `checked01.gui`, which hosts the majority of the
  ;; layout overrides), or add a field ignore rule to the `pb-ignored-fields`
  ;; map at the top of this file.
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
                                          :override-node-pb (apply-gui-overrides original-node-pb override-node-pb)})
                                       override-node-pbs)))))
                    (.getNodesList scene-pb))))

          non-layout-overridden-gui-node-field-paths
          (non-overridden-gui-node-field-paths workspace ["gui" "layouts" "nodes"] gui-resource->layout-override-infos)]

      (is (= #{} non-layout-overridden-gui-node-field-paths)
          (list-message
            (format "The following gui node fields are not covered by layout overrides in any .gui files under `editor/%s`:"
                    project-path)
            non-layout-overridden-gui-node-field-paths)))))

(deftest all-gui-template-node-fields-overridden-test
  ;; This test is intended to verify that every field in dmGuiDDF.NodeDesc is
  ;; being overridden from a template node in one of the root-level files in the
  ;; save data test project. If you add a field to the NodeDesc protobuf
  ;; message, you'll either need to add a template override for it (we suggest
  ;; you add it to `checked01.gui`, which hosts the majority of the template
  ;; overrides), or add a field ignore rule to the `pb-ignored-fields` map at
  ;; the top of this file.
  (test-util/with-loaded-project project-path
    (let [proj-path->resource #(workspace/find-resource workspace %)
          resource->gui-scene-pb (fn/memoize #(protobuf/read-pb Gui$SceneDesc %))
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
            (format "The following gui node fields are not covered by template overrides in any .gui files under `editor/%s`:"
                    project-path)
            non-template-overridden-gui-node-field-paths)))))

(defn- check-project-save-data-disk-equivalence! [project->save-datas]
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)
    (doseq [save-data (project->save-datas project)]
      (test-util/check-save-data-disk-equivalence! save-data project-path))))

(deftest save-value-is-equivalent-to-source-value-test
  ;; This test is intended to verify that the saved data contains all the same
  ;; information as the read data. These tests bypass the dirty check in order
  ;; to verify that the information saved is equivalent to the data loaded.
  ;; Failures might signal that we've forgotten to read data from the file, or
  ;; somehow we're not writing all properties to disk.
  (testing "Saved data should be equivalent to read data."
    (check-project-save-data-disk-equivalence! project/all-save-data)))

(deftest no-unsaved-changes-after-load-test
  ;; This test is intended to verify that changes to the file formats do not
  ;; cause undue changes to existing content in game projects. For example,
  ;; adding fields to component protobuf definitions may cause the default
  ;; values to be written to every instance of those components embedded in
  ;; collection or game object files, because the embedded components are
  ;; written as a string literal.
  ;;
  ;; If this test fails, you need to ensure the loaded data is migrated to the
  ;; new format by adding a :sanitize-fn when registering your resource type
  ;; (Example: `collision_object.clj`). Non-embedded components do not have this
  ;; issue as long as your added protobuf field has a default value. But more
  ;; drastic file format changes have happened in the past, and you can find
  ;; other examples of :sanitize-fn usage in non-component resource types.
  (testing "The project should not have unsaved changes immediately after loading."
    (check-project-save-data-disk-equivalence! project/dirty-save-data)))

(deftest no-unsaved-changes-after-save-test
  ;; This test is intended to verify that we track unsaved changes properly. If
  ;; any other tests in this module are failing as well, you should address them
  ;; first.
  (test-util/with-scratch-project project-path
    (test-util/clear-cached-save-data! project)
    (let [checked-resources (checked-resources workspace)
          dirty-proj-paths (into (sorted-set)
                                 (map (comp resource/proj-path :resource))
                                 (project/dirty-save-data project))]
      (when (is (= #{} dirty-proj-paths)
                (list-message
                  (format "Unable to proceed with test due to unsaved changes in the following files immediately after loading the project `editor/%s`:"
                          project-path)
                  dirty-proj-paths))
        (doseq [resource checked-resources]
          (let [proj-path (resource/proj-path resource)
                node-id (project/get-resource-node project resource)]
            (when (testing (format "File `%s` should not have unsaved changes prior to editing." proj-path)
                    (let [save-data (g/valid-node-value node-id :save-data)]
                      (if (not (:dirty save-data))
                        true
                        (let [message (str "Unsaved changes detected before editing. This is likely due to an interdependency between resources. You might need to adjust the order resources are edited.\n"
                                           (test-util/save-data-diff-message save-data))]
                          (is (not (:dirty save-data)) message)))))
              (when (test-util/can-edit-resource-node? node-id)
                (test-util/edit-resource-node! node-id)
                (testing (format "File `%s` should have unsaved changes after editing." proj-path)
                  (let [save-data (g/valid-node-value node-id :save-data)]
                    (is (:dirty save-data)
                        "No unsaved changes detected after editing. Possibly, `test-util/edit-resource-node!` is not making a meaningful change to the file?")))))))
        (test-util/save-project! project)
        (test-util/clear-cached-save-data! project)
        (doseq [resource checked-resources]
          (let [proj-path (resource/proj-path resource)]
            (testing (format "File `%s` should not have unsaved changes after saving." proj-path)
              (let [node-id (project/get-resource-node project resource)
                    save-data (g/valid-node-value node-id :save-data)]
                (is (not (:dirty save-data))
                    "Unsaved changes detected after saving.")
                (when (:dirty save-data)
                  (test-util/check-save-data-disk-equivalence! save-data project-path))))))))))

(deftest resource-save-data-retention-test
  ;; This test is intended to verify that the system cache is populated with the
  ;; save-related data for each editable resource after the project loads, but
  ;; is evicted from the cache when the resource is edited.
  (letfn [(check-resource-at-index [resource-index]
            ;; We want to test each resource in isolation to avoid interference
            ;; across resources. Otherwise, editing a referenced template GUI
            ;; might evict cached entries for the referencing GUI, for example.
            (test-util/with-loaded-project project-path
              (let [checked-resources (checked-resources workspace)]
                (assert (vector? checked-resources))
                (when-some [resource (get checked-resources resource-index)]
                  (let [proj-path (resource/proj-path resource)
                        node-id (test-util/resource-node project resource)]

                    (testing (format "File `%s` should have its save-related data in the cache before editing." proj-path)
                      (is (= (test-util/cacheable-save-data-outputs node-id)
                             (test-util/cached-save-data-outputs node-id))))

                    (when (test-util/can-edit-resource-node? node-id)
                      (test-util/edit-resource-node! node-id)

                      (testing (format "File `%s` should not have its save-related data in the cache after editing." proj-path)
                        (is (= #{} (test-util/cached-save-data-outputs node-id)))))

                    true)))))]

    (loop [resource-index 0]
      (when (check-resource-at-index resource-index)
        (recur (inc resource-index))))))

(deftest overall-save-data-retention-test
  ;; This test works in conjunction with the resource-save-data-retention-test,
  ;; and is intended to verify that save-related data is retained in memory in
  ;; situations that are impractical to cover on an individual resource basis.
  (test-util/with-scratch-project project-path
    (let [checked-resources (checked-resources workspace)]

      (testing "Save-related data is in cache after loading the project."
        (is (= {} (test-util/uncached-save-data-outputs-by-proj-path project))))

      ;; Perform edits to all checked resources.
      (g/transact
        (for [resource checked-resources
              :let [node-id (test-util/resource-node project resource)]]
          (when (test-util/can-edit-resource-node? node-id)
            (test-util/edit-resource-node node-id))))

      ;; Save the changes.
      (test-util/save-project! project)

      (testing "Save-related data is in cache after saving the project."
        (is (= {} (test-util/uncached-save-data-outputs-by-proj-path project)))))))
