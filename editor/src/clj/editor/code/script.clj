;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.code.script
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code-completion :as code-completion]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.script-intelligence :as si]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
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
            [schema.core :as s])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/deftype Modules [String])

(def lua-grammar
  {:name "Lua"
   :scope-name "source.lua"
   ;; indent patterns shamelessly stolen from textmate:
   ;; https://github.com/textmate/lua.tmbundle/blob/master/Preferences/Indent.tmPreferences
   :indent {:begin #"^([^-]|-(?!-))*((\b(else|function|then|do|repeat)\b((?!\b(end|until)\b)[^\"'])*)|(\{\s*))$"
            :end #"^\s*((\b(elseif|else|end|until)\b)|(\})|(\)))"}
   :line-comment "--"
   :patterns [{:captures {1 {:name "keyword.control.lua"}
                          2 {:name "entity.name.function.scope.lua"}
                          3 {:name "entity.name.function.lua"}
                          4 {:name "punctuation.definition.parameters.begin.lua"}
                          5 {:name "variable.parameter.function.lua"}
                          6 {:name "punctuation.definition.parameters.end.lua"}}
               :match #"\b(function)(?:\s+([a-zA-Z_.:]+[.:])?([a-zA-Z_]\w*)\s*)?(\()([^)]*)(\))"
               :name "meta.function.lua"}
              {:match #"(?<![\d.])\s0x[a-fA-F\d]+|\b\d+(\.\d+)?([eE]-?\d+)?|\.\d+([eE]-?\d+)?"
               :name "constant.numeric.lua"}
              {:begin #"'"
               :begin-captures {0 {:name "punctuation.definition.string.begin.lua"}}
               :end #"'"
               :end-captures {0 {:name "punctuation.definition.string.end.lua"}}
               :name "string.quoted.single.lua"
               :patterns [{:match #"\\."
                           :name "constant.character.escape.lua"}]}
              {:begin #"\""
               :begin-captures {0 {:name "punctuation.definition.string.begin.lua"}}
               :end #"\""
               :end-captures {0 {:name "punctuation.definition.string.end.lua"}}
               :name "string.quoted.double.lua"
               :patterns [{:match #"\\."
                           :name "constant.character.escape.lua"}]}
              {:begin #"(?<!--)\[(=*)\["
               :begin-captures {0 {:name "punctuation.definition.string.begin.lua"}}
               :end #"\]\1\]"
               :end-captures {0 {:name "punctuation.definition.string.end.lua"}}
               :name "string.quoted.other.multiline.lua"}
              {:begin #"--\[(=*)\["
               :captures {0 {:name "punctuation.definition.comment.lua"}}
               :end #"\]\1\]"
               :name "comment.block.lua"}
              {:captures {1 {:name "punctuation.definition.comment.lua"}}
               :match #"(--).*"
               :name "comment.line.double-dash.lua"}
              {:match #"\b(and|or|not|break|do|else|for|if|elseif|return|then|repeat|while|until|end|function|local|in|goto)\b"
               :name "keyword.control.lua"}
              {:match #"(?<![^.]\.|:)\b([A-Z_][0-9A-Z_]*|false|nil|true|math\.(pi|huge))\b|(?<![.])\.{3}(?!\.)"
               :name "constant.language.lua"}
              {:match #"(?<![^.]\.|:)\b(self)\b"
               :name "variable.language.self.lua"}
              {:match #"(?<![^.]\.|:)\b(assert|collectgarbage|dofile|error|getfenv|getmetatable|ipairs|loadfile|loadstring|module|next|pairs|pcall|print|rawequal|rawget|rawset|require|select|setfenv|setmetatable|tonumber|tostring|type|unpack|xpcall)\b(?=\s*(?:[({\"']|\[\[))"
               :name "support.function.lua"}
              {:match #"(?<![^.]\.|:)\b(coroutine\.(create|resume|running|status|wrap|yield)|string\.(byte|char|dump|find|format|gmatch|gsub|len|lower|match|rep|reverse|sub|upper)|table\.(concat|insert|maxn|remove|sort)|math\.(abs|acos|asin|atan2?|ceil|cosh?|deg|exp|floor|fmod|frexp|ldexp|log|log10|max|min|modf|pow|rad|random|randomseed|sinh?|sqrt|tanh?)|io\.(close|flush|input|lines|open|output|popen|read|tmpfile|type|write)|os\.(clock|date|difftime|execute|exit|getenv|remove|rename|setlocale|time|tmpname)|package\.(cpath|loaded|loadlib|path|preload|seeall)|debug\.(debug|[gs]etfenv|[gs]ethook|getinfo|[gs]etlocal|[gs]etmetatable|getregistry|[gs]etupvalue|traceback))\b(?=\s*(?:[({\"']|\[\[))"
               :name "support.function.library.lua"}
              {:match #"\b([A-Za-z_]\w*)\b(?=\s*(?:[({\"']|\[\[))"
               :name "support.function.any-method.lua"}
              {:match #"(?<=[^.]\.|:)\b([A-Za-z_]\w*)"
               :name "variable.other.lua"}
              {:match #"\+|-|%|#|\*|\/|\^|==?|~=|<=?|>=?|(?<!\.)\.{2}(?!\.)"
               :name "keyword.operator.lua"}]})

(def lua-code-opts {:code {:grammar lua-grammar}})

(defn script-property-type->property-type
  "Controls how script property values are represented in the graph and edited."
  [script-property-type]
  (case script-property-type
    :script-property-type-number   g/Num
    :script-property-type-hash     g/Str
    :script-property-type-url      g/Str
    :script-property-type-vector3  t/Vec3
    :script-property-type-vector4  t/Vec4
    :script-property-type-quat     t/Vec3
    :script-property-type-boolean  g/Bool
    :script-property-type-resource resource/Resource))

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

(g/deftype ScriptPropertyType
  (s/enum :script-property-type-number
          :script-property-type-hash
          :script-property-type-url
          :script-property-type-vector3
          :script-property-type-vector4
          :script-property-type-quat
          :script-property-type-boolean
          :script-property-type-resource))

(def script-defs [{:ext "script"
                   :label "Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:component :debuggable :non-embeddable :overridable-properties}
                   :tag-opts {:component {:transform-properties #{}}}}
                  {:ext "render_script"
                   :label "Render Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:debuggable}}
                  {:ext "gui_script"
                   :label "Gui Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:debuggable}}
                  {:ext "lua"
                   :label "Lua Module"
                   :icon "icons/32/Icons_11-Script-general.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:debuggable}}])

(def ^:private status-errors
  {:ok nil
   :invalid-args (g/error-fatal "Invalid arguments to go.property call") ; TODO: not used for now
   :invalid-value (g/error-fatal "Invalid value in go.property call")})

(defn- prop->key [p]
  (-> p :name properties/user-name->key))

(def resource-kind->ext
  "Declares which file extensions are valid for different kinds of resource
  properties. This affects the Property Editor, but is also used for validation."
  {"atlas"       ["atlas" "tilesource"]
   "font"        "font"
   "material"    "material"
   "buffer"      "buffer"
   "texture"     (conj image/exts "cubemap")
   "tile_source" "tilesource"})

(def ^:private valid-resource-kind? (partial contains? resource-kind->ext))

(defn- script-property-edit-type [prop-type resource-kind]
  (if (= resource/Resource prop-type)
    {:type prop-type :ext (resource-kind->ext resource-kind)}
    {:type prop-type}))

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

(defn- validate-value-against-edit-type [node-id prop-kw prop-name value edit-type]
  (when (= resource/Resource (:type edit-type))
    (resource-assignment-error node-id prop-kw prop-name value (:ext edit-type))))

(g/defnk produce-script-property-entries [_this _node-id deleted? name resource-kind type value]
  (when-not deleted?
    (let [prop-kw (properties/user-name->key name)
          prop-type (script-property-type->property-type type)
          edit-type (script-property-edit-type prop-type resource-kind)
          error (validate-value-against-edit-type _node-id :value name value edit-type)
          go-prop-type (script-property-type->go-prop-type type)
          overridden? (g/node-property-overridden? _this :value)
          read-only? (not (g/node-override? _this))
          visible? (not deleted?)]
      ;; NOTE: :assoc-original-value? here tells the node internals that it
      ;; needs to assoc in the :original-value from the overridden node when
      ;; evaluating :_properties or :_declared-properties. This happens
      ;; automatically when the overridden property is in the properties map of
      ;; the OverrideNode, but in our case the ScriptNode is presenting a
      ;; property that resides on the ScriptPropertyNode, so there won't be an
      ;; entry in the properties map of the ScriptNode OverrideNode.
      {prop-kw {:assoc-original-value? overridden?
                :edit-type edit-type
                :error error
                :go-prop-type go-prop-type
                :node-id _node-id
                :prop-kw :value
                :read-only? read-only?
                :type prop-type
                :value value
                :visible visible?}})))

(g/deftype NameNodeIDPair [(s/one s/Str "name") (s/one s/Int "node-id")])

(g/deftype NameNodeIDMap {s/Str s/Int})

(g/deftype ResourceKind (apply s/enum (keys resource-kind->ext)))

(g/deftype ScriptPropertyEntries
  {s/Keyword {:node-id s/Int
              :go-prop-type properties/TGoPropType
              :type s/Any
              :value s/Any
              s/Keyword s/Any}})

;; Every declared `go.property("name", 0.0)` in the script code gets a
;; corresponding ScriptPropertyNode. This started as a workaround for the fact
;; that dynamic properties couldn't host the property setter required for
;; resource properties, but it turns out to have other benefits. Like the fact
;; you can edit a property name in the script and have the rename propagate to
;; all usages of the script.
(g/defnode ScriptPropertyNode
  ;; The deleted? property is used instead of deleting the ScriptPropertyNode so
  ;; that overrides in collections and game objects survive cut-paste operations
  ;; on the go.property declarations in the script code.
  (property deleted? g/Bool)
  (property edit-type g/Any)
  (property name g/Str)
  (property resource-kind ResourceKind)
  (property type ScriptPropertyType)
  (property value g/Any
            (value (g/fnk [type resource value]
                     ;; For resource properties we use the Resource from the
                     ;; connected ResourceNode in order to track renames, etc.
                     (case type
                       :script-property-type-resource resource
                       value)))
            (set (fn [evaluation-context self _old-value new-value]
                   ;; When assigning a resource property, we must make sure the
                   ;; assigned resource is built and included in the game.
                   (let [basis (:basis evaluation-context)
                         project (project/get-project self)]
                     (concat
                       (g/disconnect-sources basis self :resource)
                       (g/disconnect-sources basis self :resource-build-targets)
                       (when (resource/resource? new-value)
                         (:tx-data (project/connect-resource-node
                                     evaluation-context project new-value self
                                     [[:resource :resource]
                                      [:build-targets :resource-build-targets]]))))))))

  (input resource resource/Resource)
  (input resource-build-targets g/Any)

  (output build-targets g/Any (g/fnk [deleted? resource-build-targets type]
                                (when (and (not deleted?)
                                           (= :script-property-type-resource type))
                                  resource-build-targets)))
  (output name+node-id NameNodeIDPair (g/fnk [_node-id name] [name _node-id]))
  (output property-entries ScriptPropertyEntries :cached produce-script-property-entries))

(defmethod g/node-key ::ScriptPropertyNode [node-id evaluation-context]
  ;; By implementing this multi-method, overridden property values will be
  ;; restored on OverrideNode ScriptPropertyNodes when a script is reloaded from
  ;; disk during resource-sync. The node-key must be unique within the script.
  (g/node-value node-id :name evaluation-context))

(defn- detect-edits [old-info-by-name new-info-by-name]
  (into {}
        (filter (fn [[name new-info]]
                  (when-some [old-info (old-info-by-name name)]
                    (not= old-info new-info))))
        new-info-by-name))

(defn- detect-renames [old-info-by-removed-name new-info-by-added-name]
  (let [added-name-by-new-info (into {} (map (juxt val key)) new-info-by-added-name)]
    (into {}
          (keep (fn [[old-name old-info]]
                  (when-some [renamed-name (added-name-by-new-info old-info)]
                    [old-name renamed-name])))
          old-info-by-removed-name)))

(def ^:private xform-to-name-info-pairs (map (juxt :name #(dissoc % :name))))

(defn- edit-script-property [node-id type resource-kind value]
  (g/set-property node-id :type type :resource-kind resource-kind :value value))

(defn- create-script-property [script-node-id name type resource-kind value]
  (g/make-nodes (g/node-id->graph-id script-node-id) [node-id [ScriptPropertyNode :name name]]
                (edit-script-property node-id type resource-kind value)
                (g/connect node-id :_node-id script-node-id :nodes)
                (g/connect node-id :build-targets script-node-id :resource-property-build-targets)
                (g/connect node-id :name+node-id script-node-id :script-property-name+node-ids)
                (g/connect node-id :property-entries script-node-id :script-property-entries)))

(defn- update-script-properties [evaluation-context script-node-id old-value new-value]
  (assert (or (nil? old-value) (vector? old-value)))
  (assert (or (nil? new-value) (vector? new-value)))
  (let [old-info-by-name (into {} xform-to-name-info-pairs old-value)
        new-info-by-name (into {} xform-to-name-info-pairs new-value)
        old-info-by-removed-name (into {} (remove (comp (partial contains? new-info-by-name) key)) old-info-by-name)
        new-info-by-added-name (into {} (remove (comp (partial contains? old-info-by-name) key)) new-info-by-name)
        renamed-name-by-old-name (detect-renames old-info-by-removed-name new-info-by-added-name)
        removed-names (into #{} (remove renamed-name-by-old-name) (keys old-info-by-removed-name))
        added-info-by-name (apply dissoc new-info-by-added-name (vals renamed-name-by-old-name))
        edited-info-by-name (detect-edits old-info-by-name new-info-by-name)
        node-ids-by-name (g/node-value script-node-id :script-property-node-ids-by-name evaluation-context)]
    (concat
      ;; Renamed properties.
      (for [[old-name new-name] renamed-name-by-old-name
            :let [node-id (node-ids-by-name old-name)]]
        (g/set-property node-id :name new-name))

      ;; Added properties.
      (for [[name {:keys [resource-kind type value]}] added-info-by-name]
        (if-some [node-id (node-ids-by-name name)]
          (concat
            (g/set-property node-id :deleted? false)
            (edit-script-property node-id type resource-kind value))
          (create-script-property script-node-id name type resource-kind value)))

      ;; Edited properties (i.e. the default value or property type changed).
      (for [[name {:keys [resource-kind type value]}] edited-info-by-name
            :let [node-id (node-ids-by-name name)]]
        (edit-script-property node-id type resource-kind value))

      ;; Removed properties.
      (for [name removed-names
            :let [node-id (node-ids-by-name name)]]
        (g/set-property node-id :deleted? true)))))

(defn- lift-error [node-id [prop-kw {:keys [error] :as prop} :as property-entry]]
  (if (nil? error)
    property-entry
    (let [lifted-error (g/error-aggregate [error] :_node-id node-id :_label prop-kw :message (:message error) :value (:value error))]
      [prop-kw (assoc prop :error lifted-error)])))

(g/defnk produce-properties [_declared-properties _node-id script-properties script-property-entries]
  (cond
    (g/error? _declared-properties) _declared-properties
    (g/error? script-property-entries) script-property-entries
    :else (-> _declared-properties
              (update :properties into (map (partial lift-error _node-id)) script-property-entries)
              (update :display-order into (map prop->key) script-properties))))

(defn- go-property-declaration-cursor-ranges
  "Find the CursorRanges that encompass each `go.property('name', ...)`
  declaration among the specified lines. These will be replaced with whitespace
  before the script is compiled for the engine."
  [lines]
  (loop [cursor-ranges (transient [])
         tokens (lua-parser/tokens (data/lines-reader lines))
         paren-count 0
         consumed []]
    (if-some [[text :as token] (first tokens)]
      (case (count consumed)
        0 (recur cursor-ranges (next tokens) 0 (case text "go" (conj consumed token) []))
        1 (recur cursor-ranges (next tokens) 0 (case text "." (conj consumed token) []))
        2 (recur cursor-ranges (next tokens) 0 (case text "property" (conj consumed token) []))
        3 (case text
            "(" (recur cursor-ranges (next tokens) (inc paren-count) consumed)
            ")" (let [paren-count (dec paren-count)]
                  (assert (not (neg? paren-count)))
                  (if (pos? paren-count)
                    (recur cursor-ranges (next tokens) paren-count consumed)
                    (let [next-tokens (next tokens)
                          [next-text :as next-token] (first next-tokens)
                          [_ start-row start-col] (first consumed)
                          [end-text end-row end-col] (if (= ";" next-text) next-token token)
                          end-col (+ ^long end-col (count end-text))
                          start-cursor (data/->Cursor start-row start-col)
                          end-cursor (data/->Cursor end-row end-col)
                          cursor-range (data/->CursorRange start-cursor end-cursor)]
                      (recur (conj! cursor-ranges cursor-range)
                             next-tokens
                             0
                             []))))
            (recur cursor-ranges (next tokens) paren-count consumed)))
      (persistent! cursor-ranges))))

(defn- line->whitespace [line]
  (string/join (repeat (count line) \space)))

(defn- cursor-range->whitespace-lines [lines cursor-range]
  (let [{:keys [first-line middle-lines last-line]} (data/cursor-range-subsequence lines cursor-range)]
    (cond-> (into [(line->whitespace first-line)]
                  (map line->whitespace)
                  middle-lines)
            (some? last-line) (conj (line->whitespace last-line)))))

(defn- strip-go-property-declarations [lines]
  (data/splice-lines lines (map (juxt identity (partial cursor-range->whitespace-lines lines))
                                (go-property-declaration-cursor-ranges lines))))

(defn- script->bytecode [lines proj-path arch]
  (try
    (luajit/bytecode (data/lines-reader lines) proj-path arch)
    (catch Exception e
      (let [{:keys [filename line message]} (ex-data e)
            cursor-range (some-> line data/line-number->CursorRange)]
        (g/map->error
          {:_label :modified-lines
           :message (.getMessage e)
           :severity :fatal
           :user-data (cond-> {:filename filename
                               :message message}

                              (some? cursor-range)
                              (assoc :cursor-range cursor-range))})))))

(defn- build-script [resource dep-resources user-data]
  ;; We always compile the full source code in order to find syntax errors.
  ;; We then strip go.property() declarations and recompile if needed.
  (let [lines (:lines user-data)
        proj-path (:proj-path user-data)
        bytecode-or-error (script->bytecode lines proj-path :32-bit)]
    (g/precluding-errors
      [bytecode-or-error]
      (let [go-props (properties/build-go-props dep-resources (:go-props user-data))
            modules (:modules user-data)
            cleaned-lines (strip-go-property-declarations lines)]
        {:resource resource
         :content (protobuf/map->bytes
                    Lua$LuaModule
                    {:source {:script (ByteString/copyFromUtf8
                                        (slurp (data/lines-reader cleaned-lines)))
                              :filename (str "@" (luajit/luajit-path-to-chunk-name (resource/proj-path (:resource resource))))}
                     :modules modules
                     :resources (mapv lua/lua-module->build-path modules)
                     :properties (properties/go-props->decls go-props true)
                     :property-resources (into (sorted-set)
                                               (keep properties/try-get-go-prop-proj-path)
                                               go-props)})}))))

(g/defnk produce-build-targets [_node-id resource lines script-properties modules module-build-targets original-resource-property-build-targets]
  (if-some [errors
            (not-empty
              (keep (fn [{:keys [name resource-kind type value]}]
                      (let [prop-type (script-property-type->property-type type)
                            edit-type (script-property-edit-type prop-type resource-kind)]
                        (validate-value-against-edit-type _node-id :lines name value edit-type)))
                    script-properties))]
    (g/error-aggregate errors :_node-id _node-id :_label :build-targets)
    (let [go-props-with-source-resources
          (map (fn [{:keys [name type value]}]
                 (let [go-prop-type (script-property-type->go-prop-type type)
                       go-prop-value (properties/clj-value->go-prop-value go-prop-type value)]
                   {:id name
                    :type go-prop-type
                    :value go-prop-value
                    :clj-value value}))
               script-properties)

          [go-props go-prop-dep-build-targets]
          (properties/build-target-go-props original-resource-property-build-targets go-props-with-source-resources)]
      ;; NOTE: The :user-data must not contain any overridden data. If it does,
      ;; the build targets won't be fused and the script will be recompiled
      ;; for every instance of the script component. The :go-props here describe
      ;; the original property values from the script, never overridden values.
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-script
          :user-data {:lines lines :go-props go-props :modules modules :proj-path (resource/proj-path resource)}
          :deps (into go-prop-dep-build-targets module-build-targets)})])))

(g/defnk produce-completions [completion-info module-completion-infos script-intelligence-completions]
  (code-completion/combine-completions completion-info module-completion-infos script-intelligence-completions))

(g/defnk produce-breakpoints [resource regions]
  (into []
        (comp (filter data/breakpoint-region?)
              (map (fn [region]
                     {:resource resource
                      :row (data/breakpoint-row region)})))
        regions))

(g/defnode ScriptNode
  (inherits r/CodeEditorResourceNode)

  (input module-build-targets g/Any :array)
  (input module-completion-infos g/Any :array :substitute gu/array-subst-remove-errors)
  (input script-intelligence-completions si/ScriptCompletions)
  (input script-property-name+node-ids NameNodeIDPair :array)
  (input script-property-entries ScriptPropertyEntries :array)

  (input resource-property-resources resource/Resource :array)
  (input resource-property-build-targets g/Any :array :substitute gu/array-subst-remove-errors)
  (input original-resource-property-build-targets g/Any :array :substitute gu/array-subst-remove-errors)

  (property completion-info g/Any (default {}) (dynamic visible (g/constantly false)))

  ;; Overrides modified-lines property in CodeEditorResourceNode.
  (property modified-lines r/Lines
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [resource (g/node-value self :resource evaluation-context)
                         lua-info (lua-parser/lua-info (resource/workspace resource) valid-resource-kind? (data/lines-reader new-value))
                         own-module (lua/path->lua-module (resource/proj-path resource))
                         completion-info (assoc lua-info :module own-module)
                         modules (into [] (comp (map second) (remove lua/preinstalled-modules)) (:requires lua-info))
                         script-properties (into []
                                                 (comp (filter #(= :ok (:status %)))
                                                       (util/distinct-by :name))
                                                 (:script-properties completion-info))]
                     (concat
                       (g/set-property self :completion-info completion-info)
                       (g/set-property self :modules modules)
                       (g/set-property self :script-properties script-properties))))))

  (property modules Modules
            (default [])
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)]
                     (concat
                       (g/disconnect-sources basis self :module-build-targets)
                       (g/disconnect-sources basis self :module-completion-infos)
                       (for [module new-value]
                         (let [path (lua/lua-module->path module)]
                           (:tx-data (project/connect-resource-node
                                       evaluation-context project path self
                                       [[:build-targets :module-build-targets]
                                        [:completion-info :module-completion-infos]])))))))))

  (property script-properties g/Any
            (default [])
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project self)]
                     (concat
                       (update-script-properties evaluation-context self old-value new-value)
                       (g/disconnect-sources basis self :original-resource-property-build-targets)
                       (for [{:keys [type value]} new-value
                             :when (and (= :script-property-type-resource type) (some? value))]
                         (:tx-data (project/connect-resource-node
                                     evaluation-context project value self
                                     [[:build-targets :original-resource-property-build-targets]]))))))))

  ;; Breakpoints output only consumed by project (array input of all code files)
  ;; and already cached there. Changing breakpoints and pulling project breakpoints
  ;; does imply a pass over all ScriptNodes to produce new breakpoints, but does
  ;; not seem to be much of a perf issue.
  (output breakpoints project/Breakpoints produce-breakpoints)

  (output _properties g/Properties :cached produce-properties)
  (output build-targets g/Any :cached produce-build-targets)
  (output completions g/Any :cached produce-completions)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output script-property-entries ScriptPropertyEntries (g/fnk [script-property-entries] (reduce into {} script-property-entries)))
  (output script-property-node-ids-by-name NameNodeIDMap (g/fnk [script-property-name+node-ids] (into {} script-property-name+node-ids))))

(defn- additional-load-fn
  [project self resource]
  (let [script-intelligence (project/script-intelligence project)]
    (g/connect script-intelligence :lua-completions self :script-intelligence-completions)))

(defn register-resource-types [workspace]
  (for [def script-defs
        :let [args (assoc def
                     :node-type ScriptNode
                     :eager-loading? true
                     :additional-load-fn additional-load-fn)]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
