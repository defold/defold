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
            [editor.math :as math]
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
           [com.dynamo.tile.proto Tile$TileGrid]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.spine.proto Spine$SpineModelDesc]
           [com.dynamo.render.proto Render$DisplayProfiles]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(defn particle-fx-transform [pb]
  (let [xform (fn [v]
                (let [p (doto (Point3d.) (math/clj->vecmath (:position v)))
                      r (doto (Quat4d.) (math/clj->vecmath (:rotation v)))]
                  [p r]))
        global-modifiers (:modifiers pb)
        new-emitters (mapv (fn [emitter]
                             (let [[ep er] (xform emitter)]
                               (update-in emitter [:modifiers] concat
                                          (mapv (fn [modifier]
                                                  (let [[mp mr] (xform modifier)]
                                                    (math/inv-transform ep er mp)
                                                    (math/inv-transform er mr)
                                                    (assoc modifier
                                                           :position (math/vecmath->clj mp)
                                                           :rotation (math/vecmath->clj mr))))
                                                global-modifiers))))
                           (:emitters pb))]
    (-> pb
      (assoc :emitters new-emitters)
      (dissoc :modifiers))))

(def pb-defs [{:ext "input_binding"
               :icon "icons/32/Icons_35-Inputbinding.png"
               :pb-class Input$InputBinding
               :label "Input Binding"}
              {:ext "render"
               :icon "icons/32/Icons_30-Render.png"
               :pb-class Render$RenderPrototypeDesc
               :resource-fields [:script]
               :label "Render"}
              {:ext "material"
               :icon "icons/32/Icons_31-Material.png"
               :pb-class Material$MaterialDesc
               :resource-fields [:vertex-program :fragment-program]
               :label "Material"}
              {:ext "factory"
               :label "Factory"
               :icon "icons/32/Icons_07-Factory.png"
               :pb-class GameSystem$FactoryDesc
               :resource-fields [:prototype]
               :tags #{:component}}
              {:ext "collectionfactory"
               :label "Collection Factory"
               :icon "icons/32/Icons_08-Collection-factory.png"
               :pb-class GameSystem$CollectionFactoryDesc
               :resource-fields [:prototype]
               :tags #{:component}}
              {:ext "collectionproxy"
               :label "Collection Proxy"
               :icon "icons/32/Icons_09-Collection.png"
               :pb-class GameSystem$CollectionProxyDesc
               :resource-fields [:collection]
               :tags #{:component}}
              {:ext "light"
               :label "Light"
               :icon "icons/32/Icons_21-Light.png"
               :pb-class GameSystem$LightDesc
               :tags #{:component}}
              {:ext "collisionobject"
               :label "Collision Object"
               ; TODO - missing icon
               :icon "icons/32/Icons_43-Tilesource-Collgroup.png"
               :pb-class Physics$CollisionObjectDesc
               :resources-fields [:collision_shape]
               :tags #{:component}}
              {:ext "gamepads"
               :label "Gamepads"
               :icon "icons/32/Icons_34-Gamepad.png"
               :pb-class Input$GamepadMaps}
              {:ext "camera"
               :label "Camera"
               :icon "icons/32/Icons_20-Camera.png"
               :pb-class Camera$CameraDesc
               :tags #{:component}}
              {:ext "model"
               :label "Model"
               :icon "icons/32/Icons_22-Model.png"
               :resource-fields [:mesh :material [:textures]]
               :pb-class Model$ModelDesc
               :tags #{:component}}
              {:ext "convexshape"
               :label "Convex Shape"
               ; TODO - missing icon
               :icon "icons/32/Icons_43-Tilesource-Collgroup.png"
               :pb-class Physics$ConvexShape}
              {:ext "gui"
               :label "Gui"
               :icon "icons/32/Icons_38-GUI.png"
               :pb-class Gui$SceneDesc
               :resource-fields [:script :material [:fonts :font] [:textures :texture]]
               :tags #{:component}}
              {:ext ["tilemap" "tilegrid"]
               :build-ext "tilegridc"
               :label "Tile Map"
               :icon "icons/32/Icons_48-Tilemap.png"
               :pb-class Tile$TileGrid
               :resource-fields [:tile-set :material]
               :tags #{:component}}
              {:ext "particlefx"
               :label "Particle FX"
               :icon "icons/32/Icons_17-ParticleFX.png"
               :pb-class Particle$ParticleFX
               :resource-fields [[:emitters :tile-source] [:emitters :material]]
               :tags #{:component}
               :transform-fn particle-fx-transform}
              {:ext "sound"
               :label "Sound"
               :icon "icons/32/Icons_26-AT-Sound.png"
               :pb-class Sound$SoundDesc
               :resource-fields [:sound]
               :tags #{:component}}
              {:ext "spinemodel"
               :label "Spine Model"
               :icon "icons/32/Icons_15-Spine-model.png"
               :pb-class Spine$SpineModelDesc
               :resource-fields [:spine-scene :material]
               :tags #{:component}}
              {:ext "display_profiles"
               :label "Display Profiles"
               ; TODO - missing icon
               :icon "icons/32/Icons_30-Render.png"
               :pb-class Render$DisplayProfiles}])

(g/defnk produce-save-data [resource def pb]
  {:resource resource
   :content (protobuf/map->str (:pb-class def) pb)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb (:pb user-data)
        pb (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb (reduce (fn [pb [label resource]]
                     (if (vector? label)
                       (assoc-in pb label resource)
                       (assoc pb label resource)))
                   pb (map (fn [[label res]]
                             [label (workspace/proj-path (get dep-resources res))])
                           (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [_node-id project-id resource pb def dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(workspace/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb (first field))))) [field])) (:resource-fields def))
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb label) (get pb label)))]) resource-fields)]
    [{:node-id _node-id
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
  (output outline g/Any :cached (g/fnk [_node-id def] {:node-id _node-id :label (:label def) :icon (:icon def)})))

(defn- connect-build-targets [self project path]
  (let [resource (workspace/resolve-resource (:resource self) path)]
    (project/connect-resource-node project resource (g/node-id self) [[:build-targets :dep-build-targets]])))

(defn load-pb [project self input def]
  (let [pb (protobuf/read-text (:pb-class def) input)
        resource (:resource self)]
    (concat
      (g/set-property (g/node-id self) :pb pb)
      (g/set-property (g/node-id self) :def def)
      (for [res (:resource-fields def)]
        (if (vector? res)
          (for [v (get pb (first res))]
            (let [path (if (second res) (get v (second res)) v)]
              (connect-build-targets self project path)))
          (connect-build-targets self project (get pb res)))))))

(defn- register [workspace def]
  (let [ext (:ext def)
        exts (if (vector? ext) ext [ext])]
    (for [ext exts]
      (workspace/register-resource-type workspace
                                     :ext ext
                                     :label (:label def)
                                     :build-ext (:build-ext def)
                                     :node-type ProtobufNode
                                     :load-fn (fn [project self input] (load-pb project self input def))
                                     :icon (:icon def)
                                     :tags (:tags def)
                                     :template (:template def)))))

(defn register-resource-types [workspace]
  (for [def pb-defs]
    (register workspace def)))
