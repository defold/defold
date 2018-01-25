(ns editor.script
  (:require [clojure.string :as string]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.code-completion :as code-completion]
            [internal.util :as iutil]
            [editor.types :as t]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.graph-util :as gu]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.pipeline.lua-scan :as lua-scan]
            [editor.gl.pass :as pass]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [editor.luajit :as luajit]
            [util.text-util :as text-util])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.google.protobuf ByteString]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)

(def ^:private lua-code-opts {:code lua/lua})
(def script-defs [{:ext "script"
                   :label "Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:component :non-embeddable :overridable-properties}
                   :tag-opts {:component {:transform-properties #{}}}}
                  {:ext "render_script"
                   :label "Render Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   }
                  {:ext "gui_script"
                   :label "Gui Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   }
                  {:ext "lua"
                   :label "Lua Module"
                   :icon "icons/32/Icons_11-Script-general.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   }])

(def ^:private status-errors
  {:ok nil
   :invalid-args (g/error-fatal "Invalid arguments to go.property call") ; TODO: not used for now
   :invalid-value (g/error-fatal "Invalid value in go.property call")})

(defn- prop->key [p]
  (-> p :name properties/user-name->key))

(g/defnk produce-user-properties [_node-id script-properties]
  (let [script-properties (filter (comp #{:ok} :status) script-properties)
        display-order (mapv prop->key script-properties)
        read-only? (nil? (g/override-original _node-id))
        props (into {}
                    (map (fn [key {:keys [type sub-type value]}]
                           [key {:node-id _node-id
                                 :type (properties/go-prop-type->property-type type)
                                 :edit-type (properties/go-prop-edit-type key type sub-type)
                                 :go-prop-type type
                                 :go-prop-sub-type sub-type
                                 :value value
                                 :read-only? read-only?}])
                         display-order
                         script-properties))]
    {:properties props
     :display-order display-order}))

(g/defnk produce-bytecode
  [_node-id code resource]
  (try
    (luajit/bytecode code (resource/proj-path resource))
    (catch Exception e
      (let [{:keys [filename line message]} (ex-data e)]
        (g/->error _node-id :code :fatal e (.getMessage e)
                   {:filename filename
                    :line     line
                    :message  message})))))

(defn- build-script [resource dep-resources user-data]
  (let [modules (:modules user-data)
        bytecode (:bytecode user-data)
        go-props (properties/build-go-props dep-resources (:go-props user-data))]
    {:resource resource
     :content (protobuf/map->bytes Lua$LuaModule
                                   {:source {:script (ByteString/copyFromUtf8 (:content user-data))
                                             :filename (resource/proj-path (:resource resource))
                                             :bytecode (when-not (g/error? bytecode)
                                                         (ByteString/copyFrom ^bytes bytecode))}
                                    :modules modules
                                    :resources (mapv lua/lua-module->build-path modules)
                                    :properties (properties/go-props->decls go-props)
                                    :property-resources (properties/go-prop-resource-paths go-props)})}))

(g/defnk produce-build-targets [_node-id resource code bytecode user-properties modules dep-build-targets resource-property-build-targets]
  (let [unresolved-go-props (map properties/property-entry->go-prop (:properties user-properties))
        [go-props go-prop-dep-build-targets] (properties/build-target-go-props resource-property-build-targets unresolved-go-props)]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-script
    :user-data {:content code :go-props go-props :modules modules :bytecode bytecode}
    :deps (into go-prop-dep-build-targets dep-build-targets)}]))

(g/defnode ScriptNode
  (inherits resource-node/TextResourceNode)

  (property prev-modules g/Any
            (dynamic visible (g/constantly false)))

  (property code g/Str
            (set (fn [evaluation-context self old-value new-value]
                   (let [basis (:basis evaluation-context)
                         modules (set (lua-scan/src->modules new-value))
                         prev-modules (g/node-value self :prev-modules evaluation-context)]
                     (when-not (= modules prev-modules)
                       (let [project (project/get-project self)]
                         (concat
                           (g/set-property self :prev-modules modules)
                           (gu/disconnect-all basis self :dep-build-targets)
                           (gu/disconnect-all basis self :module-completion-infos)
                           (for [module modules]
                             (let [path (lua/lua-module->path module)]
                               (project/connect-resource-node project path self
                                                              [[:build-targets :dep-build-targets]
                                                               [:completion-info :module-completion-infos]])))))))))
            (dynamic visible (g/constantly false)))

  (property caret-position g/Int (dynamic visible (g/constantly false)) (default 0))
  (property prefer-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property tab-triggers g/Any (dynamic visible (g/constantly false)) (default nil))
  (property selection-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property selection-length g/Int (dynamic visible (g/constantly false)) (default 0))

  (input dep-build-targets g/Any :array)
  (input module-completion-infos g/Any :array)
  (input resource-property-build-targets g/Any :array) ; TODO: If we want the old script node to work, referenced default resources from script-properties will have to be connected here.

  ;; todo replace this with the lua-parser modules
  (output modules g/Any :cached (g/fnk [code] (lua-scan/src->modules code)))
  (output script-properties g/Any :cached (g/fnk [code]
                                                 (->> (lua-scan/src->properties code)
                                                      (filter #(not (string/blank? (:name %))))
                                                      (iutil/distinct-by :name)
                                                      vec)))
  (output user-properties g/Properties :cached produce-user-properties)
  (output _properties g/Properties :cached (g/fnk [_declared-properties user-properties]
                                                  ;; TODO - fix this when corresponding graph issue has been fixed
                                                  (cond
                                                    (g/error? _declared-properties) _declared-properties
                                                    (g/error? user-properties) user-properties
                                                    true (-> _declared-properties
                                                           (update :properties into (:properties user-properties))
                                                           (update :display-order into (:display-order user-properties))))))
  (output bytecode g/Any :cached produce-bytecode)
  (output build-targets g/Any :cached produce-build-targets)

  (output completion-info g/Any :cached (g/fnk [_node-id code resource]
                                          (assoc (lua-parser/lua-info code)
                                                 :module (lua/path->lua-module (resource/proj-path resource)))))
  (output completions g/Any :cached (g/fnk [completion-info module-completion-infos]
                                           (code-completion/combine-completions completion-info module-completion-infos))))

(defn register-resource-types [workspace]
  (for [def script-defs
        :let [args (assoc def :node-type ScriptNode)]]
    (apply resource-node/register-text-resource-type workspace (mapcat identity args))))
