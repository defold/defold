(ns editor.tile-source
  (:require [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.gl.texture :as texture]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pipeline.texture-set-gen :as texture-set-gen]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.tile.proto Tile$TileSet]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.google.protobuf ByteString]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer ByteOrder FloatBuffer]))

(def atlas-icon "icons/images.png")

; TODO - fix real profiles
(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(g/defnk produce-save-data [resource pb]
  {:resource resource
   :content (protobuf/map->str Tile$TileSet pb)})

(defn- build-texture [self basis resource dep-resources user-data]
  {:resource resource :content (tex-gen/->bytes (:image user-data) test-profile)})

(defn- build-texture-set [self basis resource dep-resources user-data]
  (let [tex-set (assoc (:proto user-data) :texture (workspace/proj-path (second (first dep-resources))))]
    {:resource resource :content (protobuf/map->bytes TextureSetProto$TextureSet tex-set)}))

(g/defnk produce-build-targets [node-id project-id resource texture-set-data save-data]
  (let [workspace (project/workspace (g/node-by-id project-id))
        texture-type (workspace/get-resource-type workspace "texture")
        texture-resource (workspace/make-memory-resource workspace texture-type (:content save-data))
        texture-target {:node-id node-id
                        :resource (workspace/make-build-resource texture-resource)
                        :build-fn build-texture
                        :user-data {:image (:image texture-set-data)}}]
    [{:node-id node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture-set
      :user-data {:proto (:texture-set texture-set-data)}
      :deps [texture-target]}]))

(g/defnk produce-texture-set-data [pb image-content collision-content]
  (texture-set-gen/tile-source->texture-set-data pb image-content collision-content))

; TODO - copied from atlas, generalize into separate file - texture-set-util?

(defn- ->uv-vertex [vert-index ^FloatBuffer tex-coords]
  (let [index (* vert-index 2)]
    [(.get tex-coords ^int index) (.get tex-coords ^int (inc index))]))

(defn- ->uv-quad [quad-index tex-coords]
  (let [offset (* quad-index 4)]
    (mapv #(->uv-vertex (+ offset %) tex-coords) (range 4))))

(defn- ->anim-frame [frame-index tex-coords]
  {:tex-coords (->uv-quad frame-index tex-coords)})

(defn- ->anim-data [anim tex-coords uv-transforms]
  {:width (:width anim)
   :height (:height anim)
   :frames (mapv #(->anim-frame % tex-coords) (range (:start anim) (:end anim)))
   :uv-transforms (subvec uv-transforms (:start anim) (:end anim))})

(g/defnk produce-anim-data [texture-set-data]
  (let [tex-set (:texture-set texture-set-data)
        tex-coords (-> ^ByteString (:tex-coords tex-set)
                     (.asReadOnlyByteBuffer)
                     (.order ByteOrder/LITTLE_ENDIAN)
                     (.asFloatBuffer))
        uv-transforms (:uv-transforms texture-set-data)
        animations (:animations tex-set)]
    (into {} (map #(do [(:id %) (->anim-data % tex-coords uv-transforms)]) animations))))

(g/defnode TileSourceNode
  (inherits project/ResourceNode)

  (property pb g/Any)

  (input image-content BufferedImage)
  (input collision-content BufferedImage)

  (output save-data g/Any        :cached produce-save-data)
  (output texture-set-data g/Any :cached produce-texture-set-data)
  (output build-targets g/Any    :cached produce-build-targets)
  (output gpu-texture g/Any      :cached (g/fnk [texture-set-data] (texture/image-texture (:image texture-set-data))))
  (output anim-data g/Any        :cached produce-anim-data))

(defn- load-tile-source [project self input]
  (let [tile-source (protobuf/read-text Tile$TileSet input)]
    (concat
      (g/set-property self :pb tile-source)
      (if-let [image-node (project/resolve-resource-node self (:image tile-source))]
        (g/connect image-node :content self :image-content)
        [])
      (if-let [image-node (project/resolve-resource-node self (:collision tile-source))]
        (g/connect image-node :content self :collision-content)
        []))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext ["tilesource" "tileset"]
                                    :build-ext "texturesetc"
                                    :node-type TileSourceNode
                                    :load-fn load-tile-source
                                    :icon atlas-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid false}}))
