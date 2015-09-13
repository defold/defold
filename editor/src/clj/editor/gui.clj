(ns editor.gui
  (:require [clojure.string :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.colors :as colors]
            [internal.render.pass :as pass]
            [editor.types :as types]
            [editor.resource :as resource]
            [editor.properties :as properties])
  (:import [com.dynamo.gui.proto Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor
            Gui$NodeDesc$Pivot Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds]
           [editor.types AABB]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(def ^:private texture-icon "icons/32/Icons_25-AT-Image.png")
(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private gui-icon "icons/32/Icons_38-GUI.png")
(def ^:private text-icon "icons/32/Icons_39-GUI-Text-node.png")
(def ^:private box-icon "icons/32/Icons_40-GUI-Box-node.png")
(def ^:private pie-icon "icons/32/Icons_41-GUI-Pie-node.png")
(def ^:private virtual-icon "icons/32/Icons_01-Folder-closed.png")
(def ^:private layer-icon "icons/32/Icons_42-Layers.png")
(def ^:private layout-icon "icons/32/Icons_29-AT-Unkown.png")
(def ^:private template-icon "icons/32/Icons_29-AT-Unkown.png")

(def pb-def {:ext "gui"
             :label "Gui"
             :icon gui-icon
             :pb-class Gui$SceneDesc
             :resource-fields [:script :material [:fonts :font] [:textures :texture]]
             :tags #{:component}})

; Line shader

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

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

(def color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(defn- ->vb [vs vcount color]
  (let [vb (->color-vtx vcount)]
    (doseq [v vs]
      (conj! vb (into v color)))
    (persistent! vb)))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (doseq [renderable renderables
          :let [vs (get-in renderable [:user-data :geom-data] [])
                vcount (count vs)]
          :when (> vcount 0)]
    (let [world-transform (:world-transform renderable)
          color colors/defold-white
          #_vs #_(transf-p world-transform vs)
          vertex-binding (vtx/use-with ::lines (->vb vs vcount color) line-shader)]
      (gl/with-gl-bindings gl [line-shader vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount)))))

(defn- proj-path [resource]
  (if resource
    (resource/proj-path resource)
    ""))

(g/defnk box-pie-text? [type] (not= type :type-template))
(g/defnk box-pie? [type] (or (= :type-box type) (= :type-pie type)))
(g/defnk box? [type] (= :type-box type))
(g/defnk text? [type] (= :type-text type))
(g/defnk pie? [type] (= :type-pie type))
(g/defnk template? [type] (= :type-template type))

(defn- v3->v4 [v3]
  (conj v3 1.0))

(g/defnk produce-node-msg [type _declared-properties]
  (let [pb-renames {:x-anchor :xanchor
                    :y-anchor :yanchor}
        v3-fields [:position :rotation :scale :size]
        props (:properties _declared-properties)
        msg (-> (into {:type type} (map (fn [[k v]] [k (:value v)])
                                        (filter (fn [[k v]] (and (get v :visible true)
                                                                 (not (contains? (set (keys pb-renames)) k))))
                                                props)))
              (into (map (fn [[k v]] [v (get-in props [k :value])]) pb-renames)))
        msg (reduce (fn [msg k] (update msg k v3->v4)) msg v3-fields)]
    msg))

(g/defnode GuiNode
  (inherits scene/ScalableSceneNode)

  (property type g/Keyword (dynamic visible (g/always false)))

  (property id g/Str)
  (property size types/Vec3 (dynamic visible box-pie-text?))
  (property color types/Color (dynamic visible box-pie-text?))
  (property template g/Str (dynamic visible template?))
  (property alpha g/Num (dynamic visible template?))
  (property inherit-alpha g/Bool)

  ; Text
  (property text g/Str (dynamic visible text?))
  (property line-break g/Bool (dynamic visible text?))
  (property font g/Str
            (dynamic edit-type (g/fnk [fonts] (properties/->choicebox fonts)))
            (dynamic visible text?))
  (property outline types/Color (dynamic visible text?))
  (property shadow types/Color (dynamic visible text?))

  (property texture g/Str
            (dynamic edit-type (g/fnk [textures] (properties/->choicebox textures)))
            (dynamic visible box-pie?))
  (property slice9 types/Vec4 (dynamic visible box?))

  ; Pie
  (property outer-bounds g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$PieBounds)))
            (dynamic visible pie?))
  (property inner-radius g/Num (dynamic visible pie?))
  (property perimeter-vertices g/Num (dynamic visible pie?))
  (property pie-fill-angle g/Num (dynamic visible pie?))

  (property layer g/Str (dynamic edit-type (g/fnk [layers] (properties/->choicebox layers))))
  (property blend-mode g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$BlendMode)))
            (dynamic visible box-pie-text?))

  ; Box/Pie/Text
  (property adjust-mode g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$AdjustMode)))
            (dynamic visible box-pie-text?))
  (property pivot g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$Pivot)))
            (dynamic visible box-pie-text?))
  (property x-anchor g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$XAnchor)))
            (dynamic visible box-pie-text?))
  (property y-anchor g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$YAnchor)))
            (dynamic visible box-pie-text?))

  ; Box/Pie
  (property clipping-mode g/Keyword
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$ClippingMode)))
            (dynamic visible box-pie?))
  (property clipping-visible g/Bool (dynamic visible box-pie?))
  (property clipping-inverted g/Bool (dynamic visible box-pie?))

  (display-order [:id scene/ScalableSceneNode])

  (input node-outlines g/Any :array)
  (input layers [g/Str])
  (input textures [g/Str])
  (input fonts [g/Str])
  (output outline g/Any (g/fnk [_node-id id node-outlines type]
                               {:node-id _node-id
                                :label id
                                :icon (case type
                                        :type-box box-icon
                                        :type-text text-icon
                                        :type-pie pie-icon
                                        :type-template template-icon)
                                :children node-outlines}))
  (output pb-msg g/Any produce-node-msg))

(g/defnode TextureNode
  (property name g/Str)
  (property texture (g/protocol resource/Resource))
  (output outline g/Any (g/fnk [_node-id name]
                               {:node-id _node-id
                                :label name
                                :icon texture-icon}))
  (output pb-msg g/Any (g/fnk [name texture]
                              {:name name
                               :texture (proj-path texture)})))

(g/defnode FontNode
  (property name g/Str)
  (property font (g/protocol resource/Resource))
  (output outline g/Any (g/fnk [_node-id name]
                               {:node-id _node-id
                                :label name
                                :icon font-icon}))
  (output pb-msg g/Any (g/fnk [name font]
                              {:name name
                               :font (proj-path font)})))

(g/defnode LayerNode
  (property name g/Str)
  (output outline g/Any (g/fnk [_node-id name]
                               {:node-id _node-id
                                :label name
                                :icon layer-icon}))
  (output pb-msg g/Any (g/fnk [name]
                              {:name name})))

(g/defnode LayoutNode
  (property name g/Str)
  (property nodes g/Any)
  (output outline g/Any (g/fnk [_node-id name]
                               {:node-id _node-id
                                :label name
                                :icon layout-icon}))
  (output pb-msg g/Any
          (g/fnk [name nodes]
                 {:name name
                  :nodes nodes})))

(g/defnode NodesNode
  (input node-outlines g/Any :array)
  (output nodes-outline g/Any (g/fnk [_node-id node-outlines]
                                     {:node-id _node-id
                                      :label "Nodes"
                                      :icon virtual-icon
                                      :children node-outlines})))

(g/defnode FontsNode
  (input font-outlines g/Any :array)
  (output fonts-outline g/Any (g/fnk [_node-id font-outlines]
                                     {:node-id _node-id
                                      :label "Fonts"
                                      :icon virtual-icon
                                      :children font-outlines})))

(g/defnode TexturesNode
  (input texture-outlines g/Any :array)
  (output textures-outline g/Any (g/fnk [_node-id texture-outlines]
                                        {:node-id _node-id
                                         :label "Textures"
                                         :icon virtual-icon
                                         :children texture-outlines})))

(g/defnode LayersNode
  (input layer-outlines g/Any :array)
  (output layers-outline g/Any (g/fnk [_node-id layer-outlines]
                                      {:node-id _node-id
                                       :label "Layers"
                                       :icon virtual-icon
                                       :children layer-outlines})))

(g/defnode LayoutsNode
  (input layout-outlines g/Any :array)
  (output layouts-outline g/Any (g/fnk [_node-id layout-outlines]
                                       {:node-id _node-id
                                        :label "Layouts"
                                        :icon virtual-icon
                                        :children layout-outlines})))

(g/defnk produce-scene [scene-dims aabb]
  (let [w (:width scene-dims)
        h (:height scene-dims)]
    {:aabb aabb
     :renderable {:render-fn render-lines
                  :passes [pass/transparent]
                  :user-data {:geom-data [[0 0 0] [w 0 0] [w 0 0] [w h 0] [w h 0] [0 h 0] [0 h 0] [0 0 0]]}}}))

(g/defnk produce-pb-msg [script material adjust-reference node-msgs layer-msgs font-msgs texture-msgs layout-msgs]
  {:script (proj-path script)
   :material (proj-path material)
   :adjust-reference adjust-reference
   :nodes node-msgs
   :layers layer-msgs
   :fonts font-msgs
   :textures texture-msgs
   :layouts layout-msgs})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str (:pb-class pb-def) pb-msg)})

(defn- build-pb [self basis resource dep-resources user-data]
  (let [def (:def user-data)
        pb  (:pb user-data)
        pb  (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(g/defnk produce-build-targets [_node-id project-id resource pb dep-build-targets]
  (let [def pb-def
        dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(proj-path (:resource res)) res]) dep-build-targets))
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

(g/defnk produce-outline [_node-id nodes-outline textures-outline fonts-outline layers-outline layouts-outline]
  {:node-id _node-id
   :label (:label pb-def)
   :icon (:icon pb-def)
   :children [nodes-outline
              textures-outline
              fonts-outline
              layers-outline
              layouts-outline]})

(g/defnode GuiSceneNode
  (inherits project/ResourceNode)

  (property nodes-node g/NodeID (dynamic visible (g/always false)))
  (property fonts-node g/NodeID (dynamic visible (g/always false)))
  (property textures-node g/NodeID (dynamic visible (g/always false)))
  (property layers-node g/NodeID (dynamic visible (g/always false)))
  (property layouts-node g/NodeID (dynamic visible (g/always false)))
  (property script (g/protocol resource/Resource))
  (property material (g/protocol resource/Resource))
  (property adjust-reference g/Keyword (dynamic edit-type (g/always (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))

  (input dep-build-targets g/Any :array)
  (input project-settings g/Any)
  (input node-msgs g/Any :array)
  (input font-msgs g/Any :array)
  (input texture-msgs g/Any :array)
  (input layer-msgs g/Any :array)
  (input layout-msgs g/Any :array)
  (input nodes-outline g/Any)
  (input fonts-outline g/Any)
  (input textures-outline g/Any)
  (input layers-outline g/Any)
  (input layouts-outline g/Any)

  (input layers g/Any :array)
  (input textures g/Any :array)
  (input fonts g/Any :array)

  (output aabb AABB (g/fnk [scene-dims]
                           (let [w (:width scene-dims)
                                 h (:height scene-dims)]
                             (-> (geom/null-aabb)
                               (geom/aabb-incorporate 0 0 0)
                               (geom/aabb-incorporate w h 0)))))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output scene g/Any :cached produce-scene)
  (output outline g/Any :cached produce-outline)
  (output scene-dims g/Any :cached (g/fnk [project-settings]
                                          (let [w (get project-settings ["display" "width"])
                                                h (get project-settings ["display" "height"])]
                                            {:width w :height h})))
  (output layers [g/Str] (g/fnk [layers] layers))
  (output textures [g/Str] (g/fnk [textures] textures))
  (output fonts [g/Str] (g/fnk [fonts] fonts)))

(defn- connect-build-targets [self project path]
  (let [resource (workspace/resolve-resource (g/node-value self :resource) path)]
    (project/connect-resource-node project resource self [[:build-targets :dep-build-targets]])))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

(defn- attach-gui-node [self parent gui-node type]
  (concat
    (g/connect gui-node :_node-id self :nodes)
    (g/connect gui-node :outline parent :node-outlines)
    (g/connect gui-node :pb-msg self :node-msgs)
    (g/connect self :layers gui-node :layers)
    (case type
      (:type-box :type-pie) (g/connect self :textures gui-node :textures)
      :type-text (g/connect self :fonts gui-node :fonts)
      [])))

(defn- attach-font [self fonts-node font]
  (concat
    (g/connect font :_node-id self :nodes)
    (g/connect font :name self :fonts)
    (g/connect font :pb-msg self :font-msgs)
    (g/connect font :outline fonts-node :font-outlines)))

(defn- attach-texture [self textures-node texture]
  (concat
    (g/connect texture :_node-id self :nodes)
    (g/connect texture :name self :textures)
    (g/connect texture :pb-msg self :texture-msgs)
    (g/connect texture :outline textures-node :texture-outlines)))

(defn- attach-layer [self layers-node layer]
  (concat
    (g/connect layer :_node-id self :nodes)
    (g/connect layer :name self :layers)
    (g/connect layer :pb-msg self :layer-msgs)
    (g/connect layer :outline layers-node :layer-outlines)))

(defn- attach-layout [self layouts-node layout]
  (concat
    (g/connect layout :_node-id self :nodes)
    (g/connect layout :outline layouts-node :layout-outlines)
    (g/connect layout :pb-msg self :layout-msgs)))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

(defn load-gui-scene [project self input]
  (let [def pb-def
        scene (protobuf/read-text (:pb-class def) input)
        resource (g/node-value self :resource)
        workspace (project/workspace project)
        graph-id (g/node-id->graph-id self)]
    (concat
      (g/set-property self :script (workspace/resolve-resource resource (:script scene)))
      (g/set-property self :material (workspace/resolve-resource resource (:material scene)))
      (g/set-property self :adjust-reference (:adjust-reference scene))
      (g/set-property self :pb scene)
      (g/set-property self :def def)
      (g/connect project :settings self :project-settings)
      (for [res (:resource-fields def)]
        (if (vector? res)
          (for [v (get scene (first res))]
            (let [path (if (second res) (get v (second res)) v)]
              (connect-build-targets self project path)))
          (connect-build-targets self project (get scene res))))
      (g/make-nodes graph-id [nodes-node NodesNode]
                    (g/set-property self :nodes-node nodes-node)
                    (g/connect nodes-node :_node-id self :nodes)
                    (g/connect nodes-node :nodes-outline self :nodes-outline)
                    (loop [node-descs (:nodes scene)
                           id->node {}
                           all-tx-data []]
                      (if-let [node-desc (first node-descs)]
                        (let [tx-data (g/make-nodes graph-id [gui-node [GuiNode
                                                                        :type (:type node-desc)
                                                                        :id (:id node-desc)
                                                                        :position (v4->v3 (:position node-desc))
                                                                        :rotation (v4->v3 (:rotation node-desc))
                                                                        :scale (v4->v3 (:scale node-desc))
                                                                        :size (v4->v3 (:size node-desc))
                                                                        :color (:color node-desc)
                                                                        :blend-mode (:blend-mode node-desc)
                                                                        :text (:text node-desc)
                                                                        :texture (:texture node-desc)
                                                                        :font (:font node-desc)
                                                                        :x-anchor (:xanchor node-desc)
                                                                        :y-anchor (:yanchor node-desc)
                                                                        :pivot (:pivot node-desc)
                                                                        :outline (:oultine node-desc)
                                                                        :shadow (:shadow node-desc)
                                                                        :adjust-mode (:adjust-mode node-desc)
                                                                        :line-break (:line-break node-desc)
                                                                        :layer (:layer node-desc)
                                                                        :alpha (:alpha node-desc)
                                                                        :inherit-alpha (:inherit-alpha node-desc)
                                                                        :slice9 (:slice9 node-desc)
                                                                        :outer-bounds (:outer-bounds node-desc)
                                                                        :inner-radius (:inner-radius node-desc)
                                                                        :perimeter-vertices (:perimeter-vertices node-desc)
                                                                        :pie-fill-angle (:pie-fill-angle node-desc)
                                                                        :clipping-mode (:clipping-mode node-desc)
                                                                        :clipping-visible (:clipping-visible node-desc)
                                                                        :clipping-inverted (:clipping-inverted node-desc)
                                                                        :alpha (:alpha node-desc)
                                                                        :template (:template node-desc)]]
                                                    (let [parent (if (empty? (:parent node-desc)) nodes-node (id->node (:parent node-desc)))]
                                                      (attach-gui-node self parent gui-node (:type node-desc))))
                              node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
                          (recur (rest node-descs) (assoc id->node (:id node-desc) node-id) (into all-tx-data tx-data)))
                        all-tx-data)))
      (g/make-nodes graph-id [fonts-node FontsNode]
                    (g/set-property self :fonts-node fonts-node)
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :fonts-outline self :fonts-outline)
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [FontNode
                                                    :name (:name font-desc)
                                                    :font (workspace/resolve-resource resource (:font font-desc))]]
                                    (attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node TexturesNode]
                    (g/set-property self :textures-node textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :textures-outline self :textures-outline)
                    (for [texture-desc (:textures scene)]
                      (g/make-nodes graph-id [texture [TextureNode
                                                       :name (:name texture-desc)
                                                       :texture (workspace/resolve-resource resource (:texture texture-desc))]]
                                    (attach-texture self textures-node texture))))
      (g/make-nodes graph-id [layers-node LayersNode]
                    (g/set-property self :layers-node layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :layers-outline self :layers-outline)
                    (for [layer-desc (:layers scene)]
                      (g/make-nodes graph-id [layer [LayerNode
                                                     :name (:name layer-desc)]]
                                    (attach-layer self layers-node layer))))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                    (g/set-property self :layouts-node layouts-node)
                    (g/connect layouts-node :_node-id self :nodes)
                    (g/connect layouts-node :layouts-outline self :layouts-outline)
                    (for [layout-desc (:layouts scene)]
                      (g/make-nodes graph-id [layout [LayoutNode
                                                      :name (:name layout-desc)
                                                      :nodes (:nodes layout-desc)]]
                                    (attach-layout self layouts-node layout)))))))

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
                                     :template (:template def)
                                     :view-types [:scene]
                                     :view-opts {:scene {:grid true}}))))

(defn register-resource-types [workspace]
  (register workspace pb-def))
