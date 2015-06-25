(ns editor.protobuf-types
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.pipeline.font-gen :as font-gen]
            [internal.render.pass :as pass])
  (:import [com.dynamo.input.proto Input$InputBinding]
           [com.dynamo.render.proto Render$RenderPrototypeDesc Material$MaterialDesc]
           [com.dynamo.gamesystem.proto GameSystem$FactoryDesc GameSystem$CollectionFactoryDesc
            GameSystem$CollectionProxyDesc GameSystem$LightDesc]
           [com.dynamo.physics.proto Physics$CollisionObjectDesc]
           [com.dynamo.input.proto Input$GamepadMaps]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def pb-defs [{:ext "input_binding"
               :icon "icons/pictures.png"
               :pb-class Input$InputBinding}
              {:ext "render"
               :icon "icons/pictures.png"
               :pb-class Render$RenderPrototypeDesc
               :resource-fields [:script]}
              {:ext "material"
               :icon "icons/pictures.png"
               :pb-class Material$MaterialDesc
               ; TODO shaders
               :resource-fields []}
              {:ext "factory"
               :label "Factory"
               :icon "icons/pictures.png"
               :pb-class GameSystem$FactoryDesc
               :resource-fields [:prototype]
               :tags #{:component}}
              {:ext "collectionfactory"
               :label "Collection Factory"
               :icon "icons/pictures.png"
               :pb-class GameSystem$CollectionFactoryDesc
               :resource-fields [:prototype]
               :tags #{:component}}
              {:ext "collectionproxy"
               :label "Collection Proxy"
               :icon "icons/pictures.png"
               :pb-class GameSystem$CollectionProxyDesc
               :resource-fields [:collection]
               :tags #{:component}}
              {:ext "light"
               :label "Light"
               :icon "icons/pictures.png"
               :pb-class GameSystem$LightDesc
               :tags #{:component}}
              {:ext "collisionobject"
               :label "Collision Object"
               :icon "icons/pictures.png"
               :pb-class Physics$CollisionObjectDesc
               :resources-fields [:collision_shape]
               :tags #{:component}}
              {:ext "gamepads"
               :icon "icons/pictures.png"
               :pb-class Input$GamepadMaps}])

(g/defnk produce-save-data [resource def pb]
  {:resource resource
   :content (protobuf/map->str (:pb-class def) pb)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb (:pb user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (workspace/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [node-id project-id resource pb def dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(workspace/proj-path (:resource res)) res]) dep-build-targets))
        dep-resources (map (fn [label] [label (get deps-by-source (get pb label))]) (:resource-fields def))]
    [{:node-id node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb pb
                  :pb-class (:pb-class def)
                  :def def
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnode ProtobufNode
  (inherits project/ResourceNode)

  (property pb g/Any (visible (g/fnk [] false)))
  (property def g/Any (visible (g/fnk [] false)))

  (input dep-build-targets g/Any :array)

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/fnk [] {}))
  (output outline g/Any :cached (g/fnk [node-id def] {:node-id node-id :label (:label def) :icon (:icon def)})))

(defn load-pb [project self input def]
  (let [pb (protobuf/read-text (:pb-class def) input)
        resource (:resource self)]
    (concat
      (g/set-property self :pb pb)
      (g/set-property self :def def)
      (for [res (:resource-fields def)]
        (let [resource-node (project/resolve-resource-node self (get pb res))]
          (g/connect resource-node :build-targets self :dep-build-targets))))))

(defn- register [workspace def]
  (workspace/register-resource-type workspace
                                   :ext (:ext def)
                                   :node-type ProtobufNode
                                   :load-fn (fn [project self input] (load-pb project self input def))
                                   :icon (:icon def)
                                   :tags (:tags def)))

(defn register-resource-types [workspace]
  (for [def pb-defs]
    (register workspace def)))
