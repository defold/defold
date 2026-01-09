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

(ns editor.code.script-compilation
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as data]
            [editor.code.preprocessors :as preprocessors]
            [editor.code.resource :as r]
            [editor.image :as image]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [editor.luajit :as luajit]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.types :as t]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair]]
            [util.eduction :as e])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [com.google.protobuf ByteString]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

(def ^Class built-pb-class Lua$LuaModule)

(defn script-property-type->go-prop-type
  "Controls how script property values are represented in the file formats."
  [script-property-type]
  (case script-property-type
    :script-property-type-number   :property-type-number
    :script-property-type-hash     :property-type-hash
    :script-property-type-url      :property-type-url
    :script-property-type-vector3  :property-type-vector3
    :script-property-type-vector4  :property-type-vector4
    :script-property-type-quat     :property-type-quat
    :script-property-type-boolean  :property-type-boolean
    :script-property-type-resource :property-type-hash))

(defn script-property-type->property-type
  "Controls how script property values are represented in the graph and edited."
  [script-property-type]
  (case script-property-type
    :script-property-type-number g/Num
    :script-property-type-hash g/Str
    :script-property-type-url g/Str
    :script-property-type-vector3 t/Vec3
    :script-property-type-vector4 t/Vec4
    :script-property-type-quat t/Vec3
    :script-property-type-boolean g/Bool
    :script-property-type-resource resource/Resource))

(def resource-kind->workspace->extensions
  "Declares which file extensions are valid for different kinds of resource
  properties. This affects the Property Editor, but is also used for validation."
  {"atlas"        #(workspace/resource-kind-extensions % :atlas)
   "font"          (constantly "font")
   "material"      (constantly "material")
   "buffer"        (constantly "buffer")
   "texture"       (constantly (conj image/exts "cubemap" "render_target"))
   "tile_source"   (constantly "tilesource")
   "render_target" (constantly "render_target")})

(def valid-resource-kind? (partial contains? resource-kind->workspace->extensions))

(defn resource-kind-extensions [workspace resource-kind]
  (when-let [workspace->extensions (resource-kind->workspace->extensions resource-kind)]
    (workspace->extensions workspace)))

(defn script-property-edit-type [workspace prop-type resource-kind script-property-type]
  (let [base-map (if (= resource/Resource prop-type)
                   {:type prop-type :ext (resource-kind-extensions workspace resource-kind)}
                   {:type prop-type})]
    (assoc base-map :script-property-type script-property-type)))

(defn- resource-assignment-error [node-id prop-kw prop-name resource expected-ext]
  (when (some? resource)
    (let [resource-ext (resource/ext resource)
          ext-match? (if (coll? expected-ext)
                       (some? (some (partial = resource-ext) expected-ext))
                       (= expected-ext resource-ext))]
      (cond
        (not ext-match?)
        (g/->error node-id prop-kw :fatal resource
                   (format "%s '%s' is not of type %s"
                           (validation/format-name prop-name)
                           (resource/proj-path resource)
                           (validation/format-ext expected-ext)))

        (not (resource/exists? resource))
        (g/->error node-id prop-kw :fatal resource
                   (format "%s '%s' could not be found"
                           (validation/format-name prop-name)
                           (resource/proj-path resource)))))))

(defn validate-value-against-edit-type [node-id prop-kw prop-name value edit-type]
  (when (= resource/Resource (:type edit-type))
    (resource-assignment-error node-id prop-kw prop-name value (:ext edit-type))))

(defn lua-info->script-properties [lua-info]
  (into []
        (comp (filter #(= :ok (:status %)))
              (util/distinct-by :name))
        (:script-properties lua-info)))

(defn lua-info->modules [lua-info]
  (into [] (remove lua/preinstalled-modules) (:modules lua-info)))

(defn- script->bytecode [code proj-path]
  (try
    (luajit/bytecode code proj-path)
    (catch Exception e
      (let [{:keys [filename line message]} (ex-data e)]
        (g/map->error
          {:_label :modified-lines
           :message (.getMessage e)
           :severity :fatal
           :user-data (assoc (r/make-code-error-user-data filename line)
                        :message message)})))))

(defn- build-script [resource dep-resources user-data]
  ;; We always compile the full source code in order to find syntax errors.
  ;; We then strip go.property() declarations and recompile if needed.
  (let [code (:code user-data)
        proj-path (:proj-path user-data)
        bytecode-or-error (script->bytecode code proj-path)]
    (g/precluding-errors
      [bytecode-or-error]
      (let [go-props (properties/build-go-props dep-resources (:go-props user-data))
            modules (:modules user-data)]
        {:resource resource
         :content (protobuf/map->bytes
                    Lua$LuaModule
                    {:source {:script (ByteString/copyFromUtf8 code)
                              :filename (str "@" (luajit/luajit-path-to-chunk-name (resource/proj-path (:resource resource))))}
                     :modules modules
                     :resources (mapv lua/lua-module->build-path modules)
                     :properties (properties/go-props->decls go-props true)
                     :property-resources (into (sorted-set)
                                               (keep properties/try-get-go-prop-proj-path)
                                               go-props)})}))))

(def ^:const file-line-pattern #"(?<=^|\s|[<\"'`])(\/[^\s>\"'`:]+)(?::?)(\d+)?")

(defn- try-parse-file-line [^String message]
  (when-some [[_ proj-path line-number-string] (re-find file-line-pattern message)]
    (let [line-number (some-> line-number-string Long/parseLong)]
      (pair proj-path line-number))))

(defn- lua-info-errors [_node-id resource lua-info]
  (->> lua-info
       :errors
       (e/map
         (fn [{:keys [message cursor-range]}]
           (g/->error _node-id :modified-lines :fatal resource message {:cursor-range cursor-range})))))

(defn build-targets [_node-id resource lines lua-preprocessors script-properties original-resource-property-build-targets proj-path->resource-node]
  (let [workspace (resource/workspace resource)]
    (if-some [errors
              (not-empty
                (keep (fn [{:keys [name resource-kind type value]}]
                        (let [prop-type (script-property-type->property-type type)
                              edit-type (script-property-edit-type workspace prop-type resource-kind type)]
                          (validate-value-against-edit-type _node-id :lines name value edit-type)))
                      script-properties))]
      (g/error-aggregate errors :_node-id _node-id :_label :build-targets)
      (let [preprocessed-lines
            (try
              (preprocessors/preprocess-lua-lines lua-preprocessors lines resource :debug)
              (catch Exception exception
                exception))]
        (if-some [exception-message (ex-message preprocessed-lines)]
          (let [exception preprocessed-lines
                build-error-message (str "Lua preprocessing failed.\n" exception-message)
                log-error-message (format "Lua preprocessing failed for file '%s'." (resource/proj-path resource))]
            (log/error :message log-error-message :exception exception)
            (if-some [[proj-path line-number] (try-parse-file-line exception-message)]
              (let [exception-resource (workspace/resolve-resource resource proj-path)
                    exception-node-id (proj-path->resource-node proj-path)
                    error-node-id (or exception-node-id _node-id)
                    error-resource (if (nil? exception-node-id) resource exception-resource)
                    error-user-data (r/make-code-error-user-data proj-path line-number)]
                (g/->error error-node-id :modified-lines :fatal error-resource build-error-message error-user-data))
              (g/->error _node-id :modified-lines :fatal resource build-error-message)))
          (let [preprocessed-lua-info
                (with-open [reader (data/lines-reader preprocessed-lines)]
                  (lua-parser/lua-info workspace valid-resource-kind? reader))]
            (g/precluding-errors (lua-info-errors _node-id resource preprocessed-lua-info)
              (let [preprocessed-script-properties (lua-info->script-properties preprocessed-lua-info)
                    preprocessed-modules (lua-info->modules preprocessed-lua-info)
                    preprocessed-go-props-with-source-resources
                    (map (fn [{:keys [name type value]}]
                           (let [go-prop-type (script-property-type->go-prop-type type)
                                 go-prop-value (properties/clj-value->go-prop-value go-prop-type value)]
                             {:id name
                              :type go-prop-type
                              :value go-prop-value
                              :clj-value value}))
                         preprocessed-script-properties)

                    proj-path->resource-property-build-target
                    (bt/make-proj-path->build-target original-resource-property-build-targets)

                    [preprocessed-go-props preprocessed-go-prop-dep-build-targets]
                    (properties/build-target-go-props proj-path->resource-property-build-target preprocessed-go-props-with-source-resources)]
                ;; NOTE: The :user-data must not contain any overridden data. If it does,
                ;; the build targets won't be fused and the script will be recompiled
                ;; for every instance of the script component. The :go-props here describe
                ;; the original property values from the script, never overridden values.
                [(bt/with-content-hash
                   {:node-id _node-id
                    :resource (workspace/make-build-resource resource)
                    :build-fn build-script
                    :user-data {:code (:code preprocessed-lua-info)
                                :go-props preprocessed-go-props
                                :modules preprocessed-modules
                                :proj-path (resource/proj-path resource)}
                    :deps preprocessed-go-prop-dep-build-targets
                    :dynamic-deps (mapv lua/lua-module->path preprocessed-modules)})]))))))))