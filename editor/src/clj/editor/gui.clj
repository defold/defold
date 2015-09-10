(ns editor.gui
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
            [internal.render.pass :as pass])
  (:import [com.dynamo.gui.proto Gui$SceneDesc]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(def pb-def {:ext "gui"
             :label "Gui"
             :icon "icons/32/Icons_38-GUI.png"
             :pb-class Gui$SceneDesc
             :resource-fields [:script :material [:fonts :font] [:textures :texture]]
             :tags #{:component}})

(g/defnk produce-save-data [resource pb]
  (let [def pb-def]
    {:resource resource
     :content (protobuf/map->str (:pb-class def) pb)}))

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb  (:pb user-data)
        pb  (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (workspace/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [_node-id project-id resource pb dep-build-targets]
  (let [def pb-def
        dep-build-targets (flatten dep-build-targets)
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

(g/defnode GuiSceneNode
  (inherits project/ResourceNode)

  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))

  (input dep-build-targets g/Any :array)

  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any (g/always {}))
  (output outline g/Any :cached (g/fnk [_node-id def] {:node-id _node-id :label (:label def) :icon (:icon def)})))

(defn- connect-build-targets [self project path]
  (let [resource (workspace/resolve-resource (g/node-value self :resource) path)]
    (project/connect-resource-node project resource self [[:build-targets :dep-build-targets]])))

(defn load-gui-scene [project self input]
  (let [def pb-def
        pb (protobuf/read-text (:pb-class def) input)
        resource (g/node-value self :resource)]
    (concat
     (g/set-property self :pb pb)
     (g/set-property self :def def)
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
                                     :node-type GuiSceneNode
                                     :load-fn load-gui-scene
                                     :icon (:icon def)
                                     :tags (:tags def)
                                     :template (:template def)))))

(defn register-resource-types [workspace]
  (register workspace pb-def))
