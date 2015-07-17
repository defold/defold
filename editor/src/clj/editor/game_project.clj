(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as s]
            [dynamo.graph :as g]
            [editor.project :as project]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc  GameObject$ComponentDesc GameObject$EmbeddedComponentDesc GameObject$PrototypeDesc$Builder]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader StringReader BufferedReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def game-project-icon "icons/16/Icons_04-Project-file.png")

(g/defnk produce-save-data [resource content]
  {:resource resource
   :content content})

(defn- build-game-project [self basis resource dep-resources user-data]
  {:resource resource :content (.getBytes (:content user-data))})

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property content g/Str)

  (input dep-build-targets g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource content dep-build-targets]
                                             [{:node-id _node-id
                                               :resource (workspace/make-build-resource resource)
                                               :build-fn build-game-project
                                               :user-data {:content content}
                                               :deps (vec (flatten dep-build-targets))}])))

(defn- line! [reader]
  (let [line (.readLine reader)]
    (when line
      (s/trim line))))

(defn- parse-properties [reader]
  (loop [line (line! reader)
         properties (transient {})]
    (if line
      (if (.startsWith line "[")
        (let [cat-name (subs line 1 (dec (count line)))
              [line cat-props] (loop [line (line! reader)
                                      cat-props (transient (get properties name {}))]
                                 (if (or (nil? line) (.startsWith line "["))
                                   [line cat-props]
                                   (let [sep (.indexOf line "=")]
                                     (if (= sep -1)
                                       (recur (line! reader) cat-props)
                                       (let [key (s/trim (subs line 0 sep))
                                             val (s/trim (subs line (inc sep)))]
                                         (recur (line! reader) (assoc! cat-props key val)))))))]
          (recur line (assoc! properties cat-name (persistent! cat-props))))
        (recur (line! reader) properties))
      (persistent! properties))))

(def meta-properties (parse-properties (io/reader (io/resource "meta.properties"))))

(defn- property [properties category key]
  (get-in properties [category key] (get-in meta-properties [category (str key ".default")])))

(defn- root-resource [base-resource properties category key]
  (let [path (property properties category key)
        ; TODO - hack for compiled files in game.project
        path (subs path 0 (dec (count path)))
        ; TODO - hack for inconsistencies in game.project paths
        path (if (.startsWith path "/")
               path
               (str "/" path))]
    (workspace/resolve-resource base-resource path)))

(defn load-game-project [project self input]
  (let [content (slurp input)
        properties (parse-properties (BufferedReader. (StringReader. content)))
        resource (:resource self)
        roots (map (fn [[category field]] (root-resource resource properties category field))
                   [["bootstrap" "main_collection"] ["input" "game_binding"] ["input" "gamepads"]
                    ["bootstrap" "render"] ["display" "display_profiles"]])]
    (concat
      (g/set-property self :content content)
      (for [root roots]
        (project/connect-resource-node project root (g/node-id self) [[:build-targets :dep-build-targets]])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:text]))
