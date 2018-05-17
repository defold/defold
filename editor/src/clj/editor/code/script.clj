(ns editor.code.script
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code-completion :as code-completion]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.lua :as lua]
            [editor.luajit :as luajit]
            [editor.lua-parser :as lua-parser]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.types :as t]
            [editor.workspace :as workspace]
            [editor.validation :as validation])
  (:import (com.dynamo.lua.proto Lua$LuaModule)
           (com.google.protobuf ByteString)))

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

(def ^:private lua-code-opts {:code {:grammar lua-grammar}})

(defn script-property-type->property-type [script-property-type]
  (case script-property-type
    :script-property-type-number   g/Num
    :script-property-type-hash     g/Str
    :script-property-type-url      g/Str
    :script-property-type-vector3  t/Vec3
    :script-property-type-vector4  t/Vec4
    :script-property-type-quat     t/Vec3
    :script-property-type-boolean  g/Bool
    :script-property-type-resource resource/Resource))

(defn script-property-type->go-prop-type [script-property-type]
  (case script-property-type
    :script-property-type-number   :property-type-number
    :script-property-type-hash     :property-type-hash
    :script-property-type-url      :property-type-url
    :script-property-type-vector3  :property-type-vector3
    :script-property-type-vector4  :property-type-vector4
    :script-property-type-quat     :property-type-quat
    :script-property-type-boolean  :property-type-boolean
    :script-property-type-resource :property-type-hash))

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

(g/defnk produce-properties [_declared-properties user-properties]
  ;; TODO - fix this when corresponding graph issue has been fixed
  (cond
    (g/error? _declared-properties) _declared-properties
    (g/error? user-properties) user-properties
    :else (-> _declared-properties
              (update :properties into (:properties user-properties))
              (update :display-order into (:display-order user-properties)))))

(defn- go-property-declaration-cursor-ranges [lines]
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
                    (let [[_ start-row start-col] (first consumed)
                          [_ end-row end-col] token
                          end-col (+ ^long end-col (count text))
                          start-cursor (data/->Cursor start-row start-col)
                          end-cursor (data/->Cursor end-row end-col)
                          cursor-range (data/->CursorRange start-cursor end-cursor)]
                      (recur (conj! cursor-ranges cursor-range)
                             (next tokens)
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

(defn- script->bytecode [lines proj-path]
  (try
    (luajit/bytecode (data/lines-reader lines) proj-path)
    (catch Exception e
      (let [{:keys [filename line message]} (ex-data e)]
        (g/->error nil :lines :fatal e (.getMessage e)
                   {:filename filename
                    :line     line
                    :message  message})))))

(defn- build-script [resource dep-resources user-data]
  ;; We always compile the full source code in order to find syntax errors.
  ;; We then strip out go.property() declarations and compile again if needed.
  (let [lines (:lines user-data)
        proj-path (:proj-path user-data)
        bytecode-or-error (script->bytecode lines proj-path)]
    (g/precluding-errors
      [bytecode-or-error]
      (let [go-props (properties/build-go-props dep-resources (:go-props user-data))
            modules (:modules user-data)
            cleaned-lines (strip-go-property-declarations lines)
            bytecode (if (identical? lines cleaned-lines)
                       bytecode-or-error
                       (script->bytecode cleaned-lines proj-path))]
        (assert (not (g/error? bytecode)))
        {:resource resource
         :content (protobuf/map->bytes Lua$LuaModule
                                       {:source {:script (ByteString/copyFromUtf8 (slurp (data/lines-reader lines)))
                                                 :filename (resource/proj-path (:resource resource))
                                                 :bytecode (ByteString/copyFrom ^bytes bytecode)}
                                        :modules modules
                                        :resources (mapv lua/lua-module->build-path modules)
                                        :properties (properties/go-props->decls go-props true)
                                        :property-resources (into (sorted-set)
                                                                  (keep properties/try-get-go-prop-proj-path)
                                                                  go-props)})}))))

(g/defnk produce-build-targets [_node-id resource lines user-properties modules module-build-targets original-resource-property-build-targets]
  (let [user-property-entries (:properties user-properties)
        go-props-with-source-resources (keep properties/property-entry->go-prop user-property-entries)]
    (g/precluding-errors
      (keep :error go-props-with-source-resources)
      (let [[go-props go-prop-dep-build-targets] (properties/build-target-go-props original-resource-property-build-targets go-props-with-source-resources)]
        ;; NOTE: The user-data must not contain any overridden data. If it does,
        ;; the build targets won't be fused and the script will be recompiled
        ;; for every instance of the script component.
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-script
          :user-data {:lines lines :go-props go-props :modules modules :proj-path (resource/proj-path resource)}
          :deps (into go-prop-dep-build-targets module-build-targets)}]))))

(g/defnk produce-completions [completion-info module-completion-infos]
  (code-completion/combine-completions completion-info module-completion-infos))

(def resource-kind->ext
  {"atlas"       ["atlas" "tilesource"]
   "font"        font/font-file-extensions
   "material"    "material"
   "texture"     (conj image/exts "cubemap")
   "tile_source" "tilesource"})

(def ^:private valid-resource-kind? (partial contains? resource-kind->ext))

(defn- script-property-edit-type [script-property]
  (let [prop-kw (prop->key script-property)
        prop-type (script-property-type->property-type (:type script-property))]
    (if (not= resource/Resource prop-type)
      {:type prop-type}
      {:type prop-type
       :ext (resource-kind->ext (:resource-kind script-property))
       :clear-fn properties/clear-resource-property
       :set-fn (fn set-resource-property [_evaluation-context self _old-value new-value]
                 (properties/set-resource-property self prop-kw new-value))})))

(defn- resource-assignment-error [node-id prop-kw resource expected-ext]
  (when (some? resource)
    (let [resource-ext (resource/ext resource)
          ext-match? (if (coll? expected-ext)
                       (some? (some (partial = resource-ext) expected-ext))
                       (= expected-ext resource-ext))]
      (cond
        (not ext-match?)
        (g/->error node-id prop-kw :fatal resource
                   (format "%s '%s' is not of type %s"
                           (validation/keyword->name prop-kw)
                           (resource/proj-path resource)
                           (validation/format-ext expected-ext)))

        (not (resource/exists? resource))
        (g/->error node-id prop-kw :fatal resource
                   (format "%s '%s' could not be found"
                           (validation/keyword->name prop-kw)
                           (resource/proj-path resource)))))))

(defn- validate-value-against-edit-type [node-id prop-kw value edit-type]
  (when (= resource/Resource (:type edit-type))
    (resource-assignment-error node-id prop-kw value (:ext edit-type))))

(defn- script-property->property-entry [node-id override-node? override-values-by-prop-kw {:keys [type value] :as script-property}]
  (let [prop-kw (prop->key script-property)
        prop-type (script-property-type->property-type type)
        go-prop-type (script-property-type->go-prop-type type)
        edit-type (script-property-edit-type script-property)
        validated-value (if override-node? (override-values-by-prop-kw prop-kw) value)
        error (validate-value-against-edit-type node-id prop-kw validated-value edit-type)
        read-only? (not override-node?)]
    [prop-kw {:node-id node-id
              :type prop-type
              :edit-type edit-type
              :go-prop-type go-prop-type
              :value value
              :error error
              :read-only? read-only?}]))

(g/defnk produce-user-properties [_node-id script-properties property-resources]
  (let [override-node? (g/override? _node-id)]
    {:display-order (mapv prop->key script-properties)
     :properties (into {}
                       (map (partial script-property->property-entry _node-id override-node? property-resources))
                       script-properties)}))

(g/defnode ScriptNode
  (inherits r/CodeEditorResourceNode)

  (input module-build-targets g/Any :array)
  (input module-completion-infos g/Any :array :substitute gu/array-subst-remove-errors)
  (input resource-property-build-targets g/Any :array :substitute gu/array-subst-remove-errors)
  (input original-resource-property-build-targets g/Any :array :substitute gu/array-subst-remove-errors)

  (property completion-info g/Any (default {}) (dynamic visible (g/constantly false)))

  ;; Overrides lines property in CodeEditorResourceNode.
  (property lines r/Lines
            (default [""])
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [resource (g/node-value self :resource evaluation-context)
                         lua-info (lua-parser/lua-info (resource/workspace resource) valid-resource-kind? (data/lines-reader new-value))
                         own-module (lua/path->lua-module (resource/proj-path resource))
                         completion-info (assoc lua-info :module own-module)
                         modules (into [] (comp (map second) (remove lua/preinstalled-modules)) (:requires lua-info))
                         script-properties (filterv #(= :ok (:status %)) (:script-properties completion-info))]
                     (concat
                       (g/set-property self :completion-info completion-info)
                       (g/set-property self :modules modules)
                       (g/set-property self :script-properties script-properties))))))

  (property modules Modules
            (default [])
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project self)]
                     (concat
                       (gu/disconnect-all basis self :module-build-targets)
                       (gu/disconnect-all basis self :module-completion-infos)
                       (for [module new-value]
                         (let [path (lua/lua-module->path module)]
                           (project/connect-resource-node project path self
                                                          [[:build-targets :module-build-targets]
                                                           [:completion-info :module-completion-infos]]))))))))

  (property script-properties g/Any
            (default [])
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project self)]
                     (concat
                       (gu/disconnect-all basis self :original-resource-property-build-targets)
                       (for [{:keys [type value]} new-value
                             :when (= :script-property-type-resource type)
                             :when (some? value)]
                         (project/connect-resource-node project value self
                                                        [[:build-targets :original-resource-property-build-targets]])))))))

  (property property-resources g/Any
            (default {})
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project self)]
                     (concat
                       (gu/disconnect-all basis self :resource-property-build-targets)
                       (for [[_prop-kw resource] new-value
                             :when (some? resource)]
                         (project/connect-resource-node project resource self
                                                        [[:build-targets :resource-property-build-targets]])))))))

  (output _properties g/Properties :cached produce-properties)
  (output build-targets g/Any :cached produce-build-targets)
  (output completions g/Any :cached produce-completions)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output user-properties g/Properties produce-user-properties))

(defn register-resource-types [workspace]
  (for [def script-defs
        :let [args (assoc def :node-type ScriptNode)]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
