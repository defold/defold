(ns editor.script
  (:require [clojure.string :as string]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.code-completion :as code-completion]
            [editor.types :as t]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.pipeline.lua-scan :as lua-scan]
            [editor.gl.pass :as pass]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.google.protobuf ByteString]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)

(def ^:private lua-code-opts {:code lua/lua})
(def ^:private go-prop-type->property-types
  {:property-type-number  g/Num
   :property-type-hash    g/Str
   :property-type-url     g/Str
   :property-type-vector3 t/Vec3
   :property-type-vector4 t/Vec4
   :property-type-quat    t/Vec3
   :property-type-boolean g/Bool})

(def script-defs [{:ext "script"
                   :label "Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code :default]
                   :view-opts lua-code-opts
                   :tags #{:component :overridable-properties}}
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
   :invalid-args (g/error-severe "Invalid arguments to go.property call") ; TODO: not used for now
   :invalid-value (g/error-severe "Invalid value in go.property call")})

(defn- prop->key [p]
  (-> p :name properties/user-name->key))

(g/defnk produce-user-properties [_node-id script-properties]
  (let [script-props (filter (comp #{:ok} :status) script-properties)
        props (into {} (map (fn [p]
                              (let [type (:type p)
                                    prop (-> (select-keys p [:value])
                                           (assoc :node-id _node-id
                                                  :type (go-prop-type->property-types type)
                                                  :validation-problems (status-errors (:status p))
                                                  :edit-type {:type (go-prop-type->property-types type)}
                                                  :go-prop-type type
                                                  :read-only? (nil? (g/override-original _node-id))))]
                                [(prop->key p) prop]))
                            script-props))
        display-order (mapv prop->key script-props)]
    {:properties props
     :display-order display-order}))

(g/defnk produce-save-data [resource code]
  {:resource resource
   :content code})


(defn- build-script [self basis resource dep-resources user-data]
  (let [user-properties (:user-properties user-data)
        properties (mapv (fn [[k v]] (let [type (:go-prop-type v)]
                                       {:id (properties/key->user-name k)
                                        :value (properties/go-prop->str (:value v) type)
                                        :type type}))
                         (:properties user-properties))
        modules (:modules user-data)]
    {:resource resource :content (protobuf/map->bytes Lua$LuaModule
                                                     {:source {:script (ByteString/copyFromUtf8 (:content user-data))
                                                               :filename (resource/proj-path (:resource resource))}
                                                      :modules modules
                                                      :resources (mapv lua/lua-module->build-path modules)
                                                      :properties (properties/properties->decls properties)})}))

(g/defnk produce-build-targets [_node-id resource code user-properties modules]
  [{:node-id   _node-id
    :resource  (workspace/make-build-resource resource)
    :build-fn  build-script
    :user-data {:content code :user-properties user-properties :modules modules}
    :deps      (mapcat (fn [mod]
                         (let [path     (lua/lua-module->path mod)
                               mod-node (project/get-resource-node (project/get-project _node-id) path)]
                           (g/node-value mod-node :build-targets))) modules)}])


(g/defnode ScriptNode
  (inherits project/ResourceNode)

  (property code g/Str (dynamic visible (g/always false)))
  (property caret-position g/Int (dynamic visible (g/always false)) (default 0))
  (property prefer-offset g/Int (dynamic visible (g/always false)) (default 0))
  (property tab-triggers g/Any (dynamic visible (g/always false)) (default nil))
  (property selection-offset g/Int (dynamic visible (g/always false)) (default 0))
  (property selection-length g/Int (dynamic visible (g/always false)) (default 0))

  (input module-nodes g/Any :array)

  ;; todo replace this with the lua-parser modules
  (output modules g/Any :cached (g/fnk [code] (lua-scan/src->modules code)))
  (output script-properties g/Any :cached (g/fnk [code] (lua-scan/src->properties code)))
  (output user-properties g/Properties :cached produce-user-properties)
  (output _properties g/Properties :cached (g/fnk [_declared-properties user-properties]
                                                  ;; TODO - fix this when corresponding graph issue has been fixed
                                                  (cond
                                                    (g/error? _declared-properties) _declared-properties
                                                    (g/error? user-properties) user-properties
                                                    true (merge-with into _declared-properties user-properties))))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)

  (output completion-info g/Any :cached (g/fnk [code resource]
                                               (lua-parser/lua-info code)))
  (output completions g/Any :cached (g/fnk [_node-id completion-info module-nodes]
                                           (code-completion/combine-completions _node-id
                                                                                completion-info
                                                                                module-nodes))))

(defn load-script [project self resource]
  (g/set-property self :code (slurp resource)))

(defn- register [workspace def]
  (let [args (merge def
               {:node-type ScriptNode
                :load-fn load-script})]
    (apply workspace/register-resource-type workspace (mapcat seq (seq args)))))

(defn register-resource-types [workspace]
  (for [def script-defs]
    (register workspace def)))
