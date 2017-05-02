(ns editor.edscript
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
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
            [editor.pipeline.lua-scan :as lua-scan]
            [editor.gl.pass :as pass]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [util.luaj :as luaj]
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

(defonce ^:dynamic *lua-state* (atom nil))

(defn- ->lua-state [workspace]
  (when (not @*lua-state*)
    (reset! *lua-state* (let [is-factory (reify luaj/InputStreamFactory
                                           (->input-stream [this filename]
                                             (io/input-stream (workspace/file-resource workspace filename))))]
                          (luaj/->state is-factory))))
  @*lua-state*)

(defn reset-lua-state []
  (reset! *lua-state* nil))

(g/defnk produce-save-data [resource code]
  {:resource resource
   :content code})

(g/defnode EdScriptNode
  (inherits project/ResourceNode)

  (property prev-modules g/Any
            (dynamic visible (g/constantly false)))

  (property code g/Str
            (set (fn [basis self old-value new-value]
                   (let [modules (set (lua-scan/src->modules new-value))
                         prev-modules (g/node-value self :prev-modules {:basis basis})]
                     (when-not (= modules prev-modules)
                       (let [project (project/get-project self)]
                         (concat
                           (g/set-property self :prev-modules modules)
                           (gu/disconnect-all basis self :module-completion-infos)
                           (for [module modules]
                             (let [path (lua/lua-module->path module)]
                               (project/connect-resource-node project path self
                                                              [[:completion-info :module-completion-infos]])))))))))
            (dynamic visible (g/constantly false)))

  (property caret-position g/Int (dynamic visible (g/constantly false)) (default 0))
  (property prefer-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property tab-triggers g/Any (dynamic visible (g/constantly false)) (default nil))
  (property selection-offset g/Int (dynamic visible (g/constantly false)) (default 0))
  (property selection-length g/Int (dynamic visible (g/constantly false)) (default 0))

  (input module-completion-infos g/Any :array)

  ;; todo replace this with the lua-parser modules
  (output modules g/Any :cached (g/fnk [code] (lua-scan/src->modules code)))
  (output save-data g/Any :cached produce-save-data)

  (output completion-info g/Any :cached (g/fnk [_node-id code resource]
                                          (assoc (lua-parser/lua-info code)
                                                 :module (lua/path->lua-module (resource/proj-path resource)))))
  (output completions g/Any :cached (g/fnk [completion-info module-completion-infos]
                                           (code-completion/combine-completions completion-info module-completion-infos)))
  (output lua-state g/Any :cached (g/fnk [_node-id]
                                    (let [project (project/get-project _node-id)
                                          ws (project/workspace project)]
                                      (->lua-state ws))))
  (output script g/Any :cached (g/fnk [lua-state resource]
                                 (luaj/load-script lua-state (resource/proj-path resource))))
  (output handlers g/Any :cached (g/fnk [lua-state script]
                                   (luaj/run-script script)
                                   (luaj/get-value lua-state "handlers"))))

(defn load-ed-script [project self resource]
  (g/set-property self :code (text-util/crlf->lf (slurp resource)))
  (g/connect self :handlers project :user-handlers))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
    :ext "edscript"
    :node-type EdScriptNode
    :load-fn load-ed-script
    :label "Editor Script"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-types [:code :default]
    :view-opts {:code lua/lua}
    :textual? true))
