(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as s]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.math :as math]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.dialogs :as dialogs]
            [editor.outline-view :as outline-view])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc  GameObject$ComponentDesc GameObject$EmbeddedComponentDesc GameObject$PrototypeDesc$Builder]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader StringReader BufferedReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def game-object-icon "icons/brick.png")

(g/defnk produce-save-data [resource content]
  {:resource resource
   :content content})

(defn- build-game-project [])

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property content t/Str)

  (input dep-build-targets t/Any :array)

  (output outline t/Any :cached (g/fnk [node-id] {:node-id node-id :label "Game Project" :icon game-object-icon}))
  (output save-data t/Any :cached produce-save-data)
  (output build-targets t/Any :cached (g/fnk [node-id resource content dep-build-targets]
                                             [{:node-id node-id
                                               :resource (workspace/make-build-resource resource)
                                               :build-fn (fn [self basis resource dep-resources user-data]
                                                           {:resource resource :content (.getBytes (:content user-data))})
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

(defn- root-node [self properties category key]
  (let [path (property properties category key)
        ; TODO - hack for compiled files in game.project
        path (subs path 0 (dec (count path)))]
    (project/resolve-resource-node self path)))

(defn load-game-project [project self input]
  (let [content (slurp input)
        properties (parse-properties (BufferedReader. (StringReader. content)))
        main-collection (root-node self properties "bootstrap" "main_collection")]
    (concat
      (g/set-property self :content content)
      (g/connect main-collection :build-targets self :dep-build-targets))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-object-icon
                                    :view-types [:text]))
