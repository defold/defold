(ns editor.game-object
  (:require [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file.protobuf :as protobuf]
            [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc GameObject$ComponentDesc GameObject$EmbeddedComponentDesc]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def game-object-icon "icons/brick.png")

(defn- produce-ref-ddf [id save-data]
  (.build (doto (GameObject$ComponentDesc/newBuilder)
            (.setId id)
            ; TODO - fix global path hack
            (.setComponent (str "/" (workspace/path (:resource save-data)))))))

(defn- produce-embed-ddf [id save-data]
  (.build (doto (GameObject$EmbeddedComponentDesc/newBuilder)
            (.setId id)
            (.setType (:ext (workspace/resource-type (:resource save-data))))
            (.setData (:content save-data)))))

(g/defnode ComponentNode
  (property id t/Str)
  (property embedded t/Bool (visible false))

  (input source t/Any)
  (input outline t/Any)
  (input save-data t/Any)

  (output context-menu t/Any (g/fnk [self embedded] [{:label "Delete"
                                                      :handler-fn (fn [event] (g/transact
                                                                                (concat
                                                                                  (g/operation-label "Delete")
                                                                                  (g/delete-node self))))}]))
  (output outline t/Any (g/fnk [self id outline context-menu] (merge outline {:self self :label id :context-menu context-menu})))
  (output render-setup t/Any (g/fnk [source]
                                    (when-let [resource-type (project/get-resource-type source)]
                                      (let [view-fns (:scene (:view-fns resource-type))]
                                        {:self source :setup-rendering-fn (:setup-rendering-fn view-fns)}))))
  (output ddf-message t/Any :cached (g/fnk [id embedded save-data] (if embedded (produce-embed-ddf id save-data) (produce-ref-ddf id save-data)))))

(g/defnk produce-save-data [resource ref-ddf embed-ddf]
  {:resource resource
   :content (protobuf/pb->str
              (let [builder (GameObject$PrototypeDesc/newBuilder)]
                (doseq [ddf ref-ddf]
                  (.addComponents builder ddf))
                (doseq [ddf embed-ddf]
                  (.addEmbeddedComponents builder ddf))
                (.build builder)))})

(g/defnode GameObjectNode
  (inherits project/ResourceNode)

  (input aabbs [AABB])
  (input outline [t/Any])
  (input render-setups [t/Any])
  (input ref-ddf [t/Any])
  (input embed-ddf [t/Any])

  (output outline t/Any (g/fnk [self outline] {:self self :label "Game Object" :icon game-object-icon :children outline}))
  (output aabb AABB (g/fnk [aabbs] (reduce (fn [a b] (geom/aabb-union a b)) (geom/null-aabb) aabbs)))
  (output save-data t/Any :cached produce-save-data))

(defn- connect-if-output [out-node out-label in-node in-label]
  (if ((g/outputs out-node) out-label)
    (g/connect out-node out-label in-node in-label)
    []))

(defn load-game-object [project self input]
  (let [project-graph (g/node->graph-id self)
        prototype (protobuf/pb->map (protobuf/read-text GameObject$PrototypeDesc input))]
    (concat
      (for [component (:components prototype)]
        (g/make-nodes project-graph
                      [comp-node [ComponentNode :id (:id component)]]
                      (g/connect comp-node :outline self :outline)
                      (if-let [source-node (project/resolve-resource-node self (:component component))]
                        (concat
                          (g/connect comp-node :render-setup self :render-setups)
                          (g/connect comp-node :ddf-message self :ref-ddf)
                          (g/connect source-node :self comp-node :source)
                          (connect-if-output source-node :outline comp-node :outline)
                          (connect-if-output source-node :save-data comp-node :save-data))
                        [])))
      (for [embedded (:embedded-components prototype)]
        (let [resource (project/make-embedded-resource project (:type embedded) (:data embedded))]
          (if-let [resource-type (and resource (workspace/resource-type resource))]
            (g/make-nodes project-graph
                          [comp-node [ComponentNode :id (:id embedded) :embedded true]
                           source-node [(:node-type resource-type) :resource resource :parent project :resource-type resource-type]]
                          (g/connect source-node :self         comp-node :source)
                          (g/connect source-node :outline      comp-node :outline)
                          (g/connect source-node :save-data comp-node :save-data)
                          (g/connect comp-node   :outline      self      :outline)
                          (g/connect comp-node   :render-setup self      :render-setups)
                          (g/connect comp-node   :ddf-message  self      :embed-ddf))
            (g/make-nodes project-graph
                          [comp-node [ComponentNode :id (:id embedded) :embedded true]]
                          (g/connect comp-node   :outline      self      :outline))))))))

(defn setup-rendering [self view]
  (for [render-setup (g/node-value self :render-setups)
        :when (:setup-rendering-fn render-setup)
        :let [node (:self render-setup)
              render-fn (:setup-rendering-fn render-setup)]]
    (render-fn (g/refresh node) view)))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "go"
                                    :node-type GameObjectNode
                                    :load-fn load-game-object
                                    :icon game-object-icon
                                    :view-types [:scene]
                                    :view-fns {:scene {:setup-view-fn (fn [self view] (scene/setup-view view :grid true))
                                                       :setup-rendering-fn setup-rendering}}))
