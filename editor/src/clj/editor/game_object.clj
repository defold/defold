(ns editor.game-object
  (:require [clojure.java.io :as io]
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
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))


(def game-object-icon "icons/brick.png")

(defn- gen-ref-ddf [id ^Vector3d position ^Vector3d rotation save-data]
  (let [^DdfMath$Point3 protobuf-position (protobuf/vecmath->pb (Point3d. position))
        ^DdfMath$Quat protobuf-rotation (protobuf/vecmath->pb rotation)]
    (.build (doto (GameObject$ComponentDesc/newBuilder)
              (.setId id)
              (.setPosition protobuf-position)
              (.setRotation protobuf-rotation)
              (.setComponent (workspace/proj-path (:resource save-data)))))))

(defn- gen-embed-ddf [id ^Vector3d position ^Vector3d rotation save-data]
  (let [^DdfMath$Point3 protobuf-position (protobuf/vecmath->pb (Point3d. position))
        ^DdfMath$Quat protobuf-rotation (protobuf/vecmath->pb rotation)]
    (.build (doto (GameObject$EmbeddedComponentDesc/newBuilder)
              (.setId id)
              (.setType (:ext (workspace/resource-type (:resource save-data))))
              (.setPosition protobuf-position)
              (.setRotation protobuf-rotation)
              (.setData (:content save-data))))))

(g/defnode ComponentNode
  (inherits scene/SceneNode)

  (property id t/Str)
  (property embedded (t/maybe t/Bool) (visible false))
  (property path (t/maybe t/Str) (visible false))

  (input source t/Any)
  (input outline t/Any)
  (input save-data t/Any)
  (input scene t/Any)

  (output outline t/Any (g/fnk [self embedded path id outline] (let [suffix (if embedded "" (format " (%s)" path))]
                                                                 (assoc outline :self self :label (str id suffix)))))
  (output ddf-message t/Any :cached (g/fnk [id embedded position rotation save-data] (if embedded
                                                                                       (gen-embed-ddf id position rotation save-data)
                                                                                       (gen-ref-ddf id position rotation save-data))))
  (output scene t/Any :cached (g/fnk [self transform scene]
                                     (assoc scene
                                            :id (g/node-id self)
                                            :transform transform
                                            :aabb (geom/aabb-transform (geom/aabb-incorporate (get :aabb scene (geom/null-aabb)) 0 0 0) transform))))

  core/MultiNode
  (sub-nodes [self] (if (:embedded self) [(g/node-value self :source)] [])))

(g/defnk produce-save-data [resource ref-ddf embed-ddf]
  {:resource resource
   :content (protobuf/pb->str
              (let [^GameObject$PrototypeDesc$Builder builder (GameObject$PrototypeDesc/newBuilder)]
                (doseq [^GameObject$ComponentDesc ddf ref-ddf]
                  (.addComponents builder ddf))
                (doseq [^GameObject$EmbeddedComponentDesc ddf embed-ddf]
                  (.addEmbeddedComponents builder ddf))
                (.build builder)))})

(g/defnk produce-scene [self child-scenes]
  {:id (g/node-id self)
   :aabb (reduce geom/aabb-union (geom/null-aabb) (filter #(not (nil? %)) (map :aabb child-scenes)))
   :children child-scenes})

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input outline t/Any :array)
  (input ref-ddf t/Any :array)
  (input embed-ddf t/Any :array)
  (input child-scenes t/Any :array)
  (input child-ids t/Str :array)

  (output outline t/Any (g/fnk [self outline] {:self self :label "Game Object" :icon game-object-icon :children outline}))
  (output save-data t/Any :cached produce-save-data)
  (output scene t/Any :cached produce-scene))

(defn- connect-if-output [out-node out-label in-node in-label]
  (if ((g/outputs out-node) out-label)
    (g/connect out-node out-label in-node in-label)
    []))

(defn- gen-component-id [go-node base]
  (let [ids (g/node-value go-node :child-ids)]
    (loop [postfix 0]
      (let [id (if (= postfix 0) base (str base postfix))]
        (if (empty? (filter #(= id %) ids))
          id
          (recur (inc postfix)))))))

(defn- add-component [self source-node id position rotation]
  (let [path (if source-node (workspace/proj-path (:resource source-node)) "")]
    (g/make-nodes (g/node->graph-id self)
                  [comp-node [ComponentNode :id id :position position :rotation rotation :path path]]
                  (g/connect comp-node :outline self :outline)
                  (if source-node
                    (concat
                      (g/connect comp-node   :ddf-message self      :ref-ddf)
                      (g/connect comp-node   :id          self      :child-ids)
                      (g/connect comp-node   :scene       self      :child-scenes)
                      (g/connect source-node :self        comp-node :source)
                      (connect-if-output source-node :outline comp-node :outline)
                      (connect-if-output source-node :save-data comp-node :save-data)
                      (connect-if-output source-node :scene comp-node :scene))
                    []))))

(defn add-component-handler [self]
  (let [project (:parent self)
        workspace (:workspace (:resource self))
        component-exts (map :ext (workspace/get-resource-types workspace :component))]
    (when-let [; TODO - filter component files
               resource (first (dialogs/make-resource-dialog workspace {}))]
      (let [id (gen-component-id self (:ext (workspace/resource-type resource)))
            op-seq (gensym)
            [comp-node] (g/tx-nodes-added
                          (g/transact
                            (concat
                              (g/operation-label "Add Component")
                              (g/operation-sequence op-seq)
                              (add-component self (project/get-resource-node project resource) id [0 0 0] [0 0 0]))))]
        ; Selection
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (g/operation-label "Add Component")
            (project/select project [comp-node])))))))

(handler/defhandler :add-from-file
    (enabled? [selection] (and (= 1 (count selection)) (= GameObjectNode (g/node-type (:self (first selection))))))
    (run [selection] (add-component-handler (:self (first selection)))))

(defn- add-embedded-component [self project type data id position rotation]
  (let [resource (project/make-embedded-resource project type data)]
    (if-let [resource-type (and resource (workspace/resource-type resource))]
      (g/make-nodes (g/node->graph-id self)
                    [comp-node [ComponentNode :id id :embedded true :position position :rotation rotation]
                     source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                    (g/connect source-node :self        comp-node :source)
                    (g/connect source-node :outline     comp-node :outline)
                    (g/connect source-node :save-data   comp-node :save-data)
                    (g/connect source-node :scene       comp-node :scene)
                    (g/connect comp-node   :outline     self      :outline)
                    (g/connect comp-node   :ddf-message self      :embed-ddf)
                    (g/connect comp-node   :id          self      :child-ids)
                    (g/connect comp-node   :scene       self      :child-scenes))
      (g/make-nodes (g/node->graph-id self)
                    [comp-node [ComponentNode :id id :embedded true]]
                    (g/connect comp-node   :outline      self      :outline)))))

(defn add-embedded-component-handler [self]
  (let [project (:parent self)
        workspace (:workspace (:resource self))
        ; TODO - add sub menu with all components
        component-type (first (workspace/get-resource-types workspace :component))
        template (workspace/template component-type)]
    (let [id (gen-component-id self (:ext component-type))
          op-seq (gensym)
          [comp-node source-node] (g/tx-nodes-added
                                    (g/transact
                                      (concat
                                        (g/operation-sequence op-seq)
                                        (g/operation-label "Add Component")
                                        (add-embedded-component self project (:ext component-type) template id [0 0 0] [0 0 0]))))]
      (g/transact
        (concat
          (g/operation-sequence op-seq)
          (g/operation-label "Add Component")
          ((:load-fn component-type) project source-node (io/reader (:resource source-node)))
          (project/select project [comp-node]))))))

(handler/defhandler :add
    (enabled? [selection] (and (= 1 (count selection)) (= GameObjectNode (g/node-type (:self (first selection))))))
    (run [selection] (add-embedded-component-handler (:self (first selection)))))

(defn load-game-object [project self input]
  (let [project-graph (g/node->graph-id self)
        prototype (protobuf/pb->map (protobuf/read-text GameObject$PrototypeDesc input))]
    (concat
      (for [component (:components prototype)
            :let [source-node (project/resolve-resource-node self (:component component))]]
        (add-component self source-node (:id component) (t/Point3d->Vec3 (:position component)) (math/quat->euler (:rotation component))))
      (for [embedded (:embedded-components prototype)]
        (add-embedded-component self project (:type embedded) (:data embedded) (:id embedded) (t/Point3d->Vec3 (:position embedded)) (math/quat->euler (:rotation embedded)))))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :icon game-object-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}
                                    :template "templates/template.go"))
