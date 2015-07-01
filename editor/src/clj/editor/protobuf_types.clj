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
           [com.dynamo.physics.proto Physics$CollisionObjectDesc Physics$ConvexShape]
           [com.dynamo.input.proto Input$GamepadMaps]
           [com.dynamo.camera.proto Camera$CameraDesc]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.dynamo.model.proto Model$ModelDesc]
           [com.dynamo.gui.proto Gui$SceneDesc]
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
               :resource-fields [:vertex-program :fragment-program]}
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
               :pb-class Input$GamepadMaps}
              {:ext "camera"
               :label "Camera"
               :icon "icons/pictures.png"
               :pb-class Camera$CameraDesc
               :tags #{:component}}
              {:ext "mesh"
               :icon "icons/pictures.png"
               :pb-class Mesh$MeshDesc}
              {:ext "model"
               :label "Model"
               :icon "icons/pictures.png"
               :pb-class Model$ModelDesc
               :tags #{:component}}
              {:ext "convexshape"
               :icon "icons/pictures.png"
               :pb-class Physics$ConvexShape}
              {:ext "gui"
               :label "Gui"
               :icon "icons/16/Icons_38-GUI.png"
               :pb-class Gui$SceneDesc
               :resource-fields [:script :material [:fonts :font] [:textures :texture]]
               :tags #{:component}}])

(g/defnk produce-save-data [resource def pb]
  {:resource resource
   :content (protobuf/map->str (:pb-class def) pb)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb (:pb user-data)
        pb (reduce (fn [pb [label resource]] (if (vector? label) (assoc-in pb label resource) (assoc pb label resource))) pb (map (fn [[label res]] [label (workspace/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [node-id project-id resource pb def dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(workspace/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] [(first field) i (second field)]) (range (count (get pb (first field))))) [field])) (:resource-fields def))
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb label) (get pb label)))]) resource-fields)]
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

  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))

  (input dep-build-targets g/Any :array)

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/always {}))
  (output outline g/Any :cached (g/fnk [node-id def] {:node-id node-id :label (:label def) :icon (:icon def)})))

(defn- connect-build-targets [self path]
  (if-let [resource-node (project/resolve-resource-node self path)]
    (g/connect resource-node :build-targets self :dep-build-targets)
    []))

(defn load-pb [project self input def]
  (let [pb (protobuf/read-text (:pb-class def) input)
        resource (:resource self)]
    (concat
      (g/set-property self :pb pb)
      (g/set-property self :def def)
      (for [res (:resource-fields def)]
        (if (vector? res)
          (for [v (get pb (first res))]
            (connect-build-targets self (get v (second res))))
          (connect-build-targets self (get pb res)))))))

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
