(ns editor.script
  (:require [clojure.string :as string]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.types :as t]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.properties :as properties]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.pipeline.lua-scan :as lua-scan]
            [internal.render.pass :as pass])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.google.protobuf ByteString]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def script-defs [{:ext "script"
                   :label "Script"
                   :icon "icons/32/Icons_12-Script-type.png"
                   :view-types [:code]
                   :tags #{:component}}
                  {:ext "render_script"
                   :label "Render Script"
                   :icon "icons/32/Icons_12-Script-type.png"}
                  {:ext "gui_script"
                   :label "Gui Script"
                   :icon "icons/32/Icons_12-Script-type.png"}
                  {:ext "lua"
                   :label "Lua Module"
                   :icon "icons/32/Icons_11-Script-general.png"}])

(def ^:private status-errors
  {:ok nil
   :invalid-args (g/error-severe "Invalid arguments to go.property call") ; TODO: not used for now
   :invalid-value (g/error-severe "Invalid value in go.property call")})

(g/defnk produce-user-properties [script-properties]
  (let [script-props (filter (comp #{:ok :invalid-value} :status) script-properties)
        props (into {} (map (fn [p]
                              (let [key (:name p)
                                    prop (-> (select-keys p [:value])
                                           (assoc :validation-problems (status-errors (:status p)))
                                           (assoc :edit-type {:type (properties/go-prop-type->clj-type (:type p))
                                                              :go-prop-type (:type p)}))]
                                [key prop]))
                            script-props))
        display-order (mapv :name script-props)]
    {:properties props
     :display-order display-order}))

(g/defnk produce-save-data [resource code]
  {:resource resource
   :content code})

(defn- lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn- lua-module->build-path [module]
  (str (lua-module->path module) "c"))

(defn- build-script [self basis resource dep-resources user-data]
  (let [user-properties (:user-properties user-data)
        properties (mapv (fn [[k v]] {:id k :value (:value v) :type (get-in v [:edit-type :go-prop-type])})
                         (:properties user-properties))
        modules (:modules user-data)]
    {:resource resource :content (protobuf/map->bytes Lua$LuaModule
                                                     {:source {:script (ByteString/copyFromUtf8 (:content user-data))
                                                               :filename (resource/proj-path (:resource resource))}
                                                      :modules modules
                                                      :resources (mapv lua-module->build-path modules)
                                                      :properties (properties/properties->decls properties)})}))

(g/defnk produce-build-targets [_node-id resource code user-properties modules]
  [{:node-id   _node-id
    :resource  (workspace/make-build-resource resource)
    :build-fn  build-script
    :user-data {:content code :user-properties user-properties :modules modules}
    :deps      (mapcat (fn [mod]
                         (let [path     (lua-module->path mod)
                               mod-node (project/get-resource-node (project/get-project _node-id) path)]
                           (g/node-value mod-node :build-targets))) modules)}])

(g/defnode ScriptNode
  (inherits project/ResourceNode)

  (property code g/Str (dynamic visible (g/always false)))
  (property caret-position g/Int (dynamic visible (g/always false)) (default 0))

  (output modules g/Any :cached (g/fnk [code] (lua-scan/src->modules code)))
  (output script-properties g/Any :cached (g/fnk [code] (lua-scan/src->properties code)))
  (output user-properties g/Any :cached produce-user-properties)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))

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
