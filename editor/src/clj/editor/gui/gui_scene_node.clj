(ns editor.gui.gui-scene-node
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.graph-util :as gu]
            [editor.gui.icons :as icons]
            [editor.gl.shader :as shader]
            [editor.colors :as colors]
            [editor.gl.vertex :as vtx]
            [editor.gl.pass :as pass]
            [editor.properties :as properties]
            [editor.scene :as scene]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.gui.util :as gutil])
  (:import [com.dynamo.gui.proto Gui$SceneDesc Gui$SceneDesc$AdjustReference]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [editor.types AABB]
           editor.gl.shader.ShaderLifecycle))

(defn- ->scene-pb-msg [script-resource material-resource adjust-reference background-color node-msgs layer-msgs font-msgs texture-msgs layout-msgs]
  {:script (gutil/proj-path script-resource)
   :material (gutil/proj-path material-resource)
   :adjust-reference adjust-reference
   :background-color background-color
   :nodes (map #(dissoc % :index) (flatten (sort-by #(get-in % [0 :index]) node-msgs)))
   :layers (map #(dissoc % :index) (sort-by :index layer-msgs))
   :fonts font-msgs
   :textures texture-msgs
   :layouts layout-msgs})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb  (:pb user-data)
        pb  (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (gutil/proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(defn- merge-rt-pb-msg [rt-pb-msg template-build-targets]
  (let [merge-fn! (fn [coll msg kw] (reduce conj! coll (map #(do [(:name %) %]) (get msg kw))))
        [textures fonts] (loop [textures (transient {})
                                fonts (transient {})
                                msgs (conj (mapv #(get-in % [:user-data :pb]) template-build-targets) rt-pb-msg)]
                           (if-let [msg (first msgs)]
                             (recur
                               (merge-fn! textures msg :textures)
                               (merge-fn! fonts msg :fonts)
                               (next msgs))
                             [(persistent! textures) (persistent! fonts)]))]
    (assoc rt-pb-msg :textures (mapv second textures) :fonts (mapv second fonts))))

(g/defnk produce-pb-msg [script-resource material-resource adjust-reference background-color node-msgs layer-msgs font-msgs texture-msgs layout-msgs]
  (->scene-pb-msg script-resource material-resource adjust-reference background-color node-msgs layer-msgs font-msgs texture-msgs layout-msgs))

(g/defnk produce-rt-pb-msg [script-resource material-resource adjust-reference background-color node-rt-msgs layer-msgs font-msgs texture-msgs layout-msgs]
  (->scene-pb-msg script-resource material-resource adjust-reference background-color node-rt-msgs layer-msgs font-msgs texture-msgs layout-msgs))

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str (:pb-class gutil/pb-def) pb-msg)})

(g/defnk produce-build-targets [_node-id resource rt-pb-msg dep-build-targets template-build-targets]
  (let [def gutil/pb-def
        template-build-targets (flatten template-build-targets)
        rt-pb-msg (merge-rt-pb-msg rt-pb-msg template-build-targets)
        dep-build-targets (concat (flatten dep-build-targets) (mapcat :deps (flatten template-build-targets)))
        deps-by-source (into {} (map #(let [res (:resource %)] [(gutil/proj-path (:resource res)) res]) dep-build-targets))
        resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get rt-pb-msg (first field))))) [field])) (:resource-fields def))
        dep-resources (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in rt-pb-msg label) (get rt-pb-msg label)))]) resource-fields)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb rt-pb-msg
                  :pb-class (:pb-class def)
                  :def def
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnk produce-outline [_node-id child-outlines]
  {:node-id _node-id
   :label (:label gutil/pb-def)
   :icon (:icon gutil/pb-def)
   :children (vec (sort-by :order child-outlines))})

(defn- apply-alpha [parent-alpha scene]
  (let [scene-alpha (get-in scene [:renderable :user-data :color 3] 1.0)]
    (if (get-in scene [:renderable :user-data :inherit-alpha] true)
      (let [alpha (* parent-alpha scene-alpha)
            inherit? (get-in scene [:renderable :user-data :inherit-alpha] false)]
        (cond-> scene
          inherit?
          (assoc-in [:renderable :user-data :color 3] alpha)

          true
          (update :children #(mapv (partial apply-alpha alpha) %))))
      (update scene :children #(mapv (partial apply-alpha scene-alpha) %)))))

(defn- sorted-children [scene]
  (sort-by (comp :index :renderable) (:children scene)))

(defn- sort-children [node-order scene]
  (-> scene
    (assoc-in [:renderable :index] (node-order (:node-id scene)))
    (update :children (partial mapv (partial sort-children node-order)))))

(defn- sort-scene [scene]
  (let [node-order (->> scene
                     (tree-seq (constantly true) sorted-children)
                     (group-by (fn [n] (get-in n [:renderable :layer-index] 0)))
                     (sort-by first) ; sort by layer-index
                     (map second)
                     (reduce into) ; unify the vectors
                     (map-indexed (fn [index s] [(:node-id s) index]))
                     (into {}))]
    (sort-children node-order scene)))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(defn- ->color-vtx-vb [vs colors vcount]
  (let [vb (->color-vtx vcount)
        vs (mapv (comp vec concat) vs colors)]
    (doseq [v vs]
      (conj! vb v))
    (persistent! vb)))

(def outline-color (scene/select-color pass/outline false [1.0 1.0 1.0 1.0]))
(def selected-outline-color (scene/select-color pass/outline true [1.0 1.0 1.0 1.0]))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (let [[vs colors] (reduce (fn [[vs colors] renderable]
                              (let [world-transform (:world-transform renderable)
                                    user-data (:user-data renderable)
                                    line-data (:line-data user-data)
                                    vcount (count line-data)
                                    color (get user-data :line-color (if (:selected renderable) selected-outline-color outline-color))]
                                [(into vs (geom/transf-p world-transform (:line-data user-data)))
                                 (into colors (repeat vcount color))]))
                      [[] []] renderables)
        vcount (count vs)]
    (when (> vcount 0)
      (let [vertex-binding (vtx/use-with ::lines (->color-vtx-vb vs colors vcount) line-shader)]
        (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))))

(g/defnk produce-scene [_node-id scene-dims aabb child-scenes]
  (let [w (:width scene-dims)
        h (:height scene-dims)
        scene {:node-id _node-id
               :aabb aabb
               :renderable {:render-fn render-lines
                            :passes [pass/transparent]
                            :batch-key []
                            :user-data {:line-data [[0 0 0] [w 0 0] [w 0 0] [w h 0] [w h 0] [0 h 0] [0 h 0] [0 0 0]]
                                        :line-color colors/defold-white}}
               :children (mapv (partial apply-alpha 1.0) child-scenes)}]
    (sort-scene scene)))

(g/defnode GuiSceneNode
  (inherits project/ResourceNode)

  (property script resource/Resource
            (value (gu/passthrough script-resource))
            (set (project/gen-resource-setter [[:resource :script-resource]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource script)))


  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (project/gen-resource-setter [[:resource :material-resource]
                                               [:shader :material-shader]
                                               [:samplers :samplers]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource material)))

  (property adjust-reference g/Keyword (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property pb g/Any (dynamic visible (g/fnk [] false)))
  (property def g/Any (dynamic visible (g/fnk [] false)))
  (property background-color types/Color (dynamic visible (g/fnk [] false)) (default [1 1 1 1]))

  (input script-resource resource/Resource)

  (input nodes-node g/NodeID)
  (input fonts-node g/NodeID)
  (input textures-node g/NodeID)
  (input layers-node g/NodeID)
  (input layouts-node g/NodeID)
  (input dep-build-targets g/Any :array)
  (input project-settings g/Any)
  (input node-msgs g/Any :array)
  (input node-rt-msgs g/Any :array)
  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides] (into {} node-overrides)))
  (input font-msgs g/Any :array)
  (input texture-msgs g/Any :array)
  (input layer-msgs g/Any :array)
  (input layout-msgs g/Any :array)
  (input child-scenes g/Any :array)
  (input node-ids gutil/IDMap :array)
  (input texture-names g/Str :array)
  (input font-names g/Str :array)
  (input layer-names g/Str :array)
  (input layout-names g/Str :array)

  (input texture-ids {g/Str g/NodeID} :array)
  (input font-ids {g/Str g/NodeID} :array)
  (input layer-ids {g/Str g/NodeID} :array)

  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (g/fnk [material-shader] material-shader))
  (input samplers [{g/Keyword g/Any}])
  (output samplers [{g/Keyword g/Any}] (g/fnk [samplers] samplers))
  (output aabb AABB :cached (g/fnk [scene-dims child-scenes]
                                   (let [w (:width scene-dims)
                                         h (:height scene-dims)
                                         scene-aabb (-> (geom/null-aabb)
                                                        (geom/aabb-incorporate 0 0 0)
                                                        (geom/aabb-incorporate w h 0))]
                                     (reduce geom/aabb-union scene-aabb (map :aabb child-scenes)))))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output rt-pb-msg g/Any :cached produce-rt-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (input template-build-targets g/Any :array)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output node-outline outline/OutlineData :cached produce-outline)
  (output scene-dims g/Any :cached (g/fnk [project-settings]
                                          (let [w (get project-settings ["display" "width"])
                                                h (get project-settings ["display" "height"])]
                                            {:width w :height h})))
  (output layers [g/Str] :cached (g/fnk [layers] layers))
  (output texture-ids {g/Str g/NodeID} :cached (g/fnk [texture-ids] (into {} texture-ids)))
  (output node-ids {g/Str g/NodeID} :cached (g/fnk [node-ids] (into {} node-ids)))
  (output font-ids {g/Str g/NodeID} :cached (g/fnk [font-ids] (into {} font-ids)))
  (output layer-ids {g/Str g/NodeID} :cached (g/fnk [layer-ids] (into {} layer-ids)))
  (input id-prefix g/Str)
  (output id-prefix g/Str (g/fnk [id-prefix] id-prefix)))

(defmacro gen-outline-fnk-body [_node-id child-outlines label order sort-children? child-reqs]
  `{:node-id ~_node-id
   :label ~label
   :icon icons/virtual-icon
   :order ~order
   :read-only true
   :child-reqs ~child-reqs
   :children ~(if sort-children?
               `(vec (sort-by :index ~child-outlines))
               child-outlines)})
