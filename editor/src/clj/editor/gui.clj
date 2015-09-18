(ns editor.gui
  (:require [clojure.string :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.colors :as colors]
            [internal.render.pass :as pass]
            [editor.types :as types]
            [editor.resource :as resource]
            [editor.properties :as properties]
            [editor.handler :as handler]
            [editor.ui :as ui])
  (:import [com.dynamo.gui.proto Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor
            Gui$NodeDesc$Pivot Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds]
           [editor.types AABB]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.vecmath Matrix4d Point3d Quat4d]
           [java.awt.image BufferedImage]
           [com.defold.editor.pipeline TextureSetGenerator$UVTransform]))

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

(vtx/defvertex uv-color-vtx
  (vec3 position)
  (vec2 texcoord0)
  (vec4 color))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (* var_color (texture2D texture var_texcoord0.xy)))))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

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

(defn- ->color-vtx-vb [vs colors vcount]
  (let [vb (->color-vtx vcount)
        vs (mapv (comp vec concat) vs colors)]
    (doseq [v vs]
      (conj! vb v))
    (persistent! vb)))

(defn- ->uv-color-vtx-vb [vs uvs colors vcount]
  (let [vb (->uv-color-vtx vcount)
        vs (mapv (comp vec concat) vs uvs colors)]
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
        (gl/with-gl-bindings gl [line-shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [gpu-texture (get-in (first renderables) [:user-data :gpu-texture])
        [vs uvs colors] (reduce (fn [[vs uvs colors] renderable]
                                  (let [user-data (:user-data renderable)
                                        world-transform (:world-transform renderable)
                                        vcount (count (:geom-data user-data))]
                                    [(into vs (geom/transf-p world-transform (:geom-data user-data)))
                                     (into uvs (:uv-data user-data))
                                     (into colors (repeat vcount (:color user-data)))]))
                          [[] [] []] renderables)
        vcount (count vs)]
    (when (> vcount 0)
      (let [shader (if gpu-texture shader line-shader)
            vertex-binding (vtx/use-with ::tris (->uv-color-vtx-vb vs uvs colors vcount) shader)]
        (gl/with-gl-bindings gl [shader vertex-binding gpu-texture]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))))

(defn- pivot-offset [pivot size]
  (let [xs (case pivot
             (:pivot-e :pivot-ne :pivot-se) -1.0
             (:pivot-center :pivot-n :pivot-s) -0.5
             (:pivot-w :pivot-nw :pivot-sw) 0.0)
        ys (case pivot
             (:pivot-ne :pivot-n :pivot-nw) -1.0
             (:pivot-e :pivot-center :pivot-w) -0.5
             (:pivot-se :pivot-s :pivot-sw) 0.0)]
    (mapv * size [xs ys 1])))

(defn render-nodes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (if (= pass pass/outline)
      (render-lines gl render-args renderables rcount)
      (render-tris gl render-args renderables rcount))))

(g/defnk produce-node-scene [_node-id type aabb transform pivot size color texture gpu-texture anim-data child-scenes]
  (let [[geom-data uv-data line-data] (if (= :type-box type)
                                        (let [[w h _] size
                                              order [0 1 3 3 1 2]
                                              corners (geom/transl (pivot-offset pivot size) [[0 0 0] [0 h 0] [w h 0] [w 0 0]])
                                              vs (mapv (partial nth corners) order)
                                              uvs (get-in anim-data [texture :frames 0] [[0 1] [0 0] [1 0] [1 1]])
                                              uvs (mapv (partial nth uvs) order)
                                              lines (interleave corners (drop 1 (cycle corners)))]
                                          [vs uvs lines])
                                        [[] [] []])]
    {:node-id _node-id
     :aabb aabb
     :transform transform
     :renderable {:render-fn render-nodes
                  :passes [pass/transparent pass/selection pass/outline]
                  :user-data {:geom-data geom-data
                              :line-data line-data
                              :uv-data uv-data
                              :color color
                              :gpu-texture gpu-texture}
                  :batch-key [gpu-texture]
                  :select-batch-key _node-id}
     :children child-scenes}))

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

(g/defnk produce-node-msg [type parent _declared-properties]
  (let [pb-renames {:x-anchor :xanchor
                    :y-anchor :yanchor}
        v3-fields [:position :rotation :scale :size]
        props (:properties _declared-properties)
        msg (-> (into {:parent parent
                       :type type}
                      (map (fn [[k v]] [k (:value v)])
                           (filter (fn [[k v]] (and (get v :visible true)
                                                    (not (contains? (set (keys pb-renames)) k))))
                                   props)))
              (into (map (fn [[k v]] [v (get-in props [k :value])]) pb-renames)))
        msg (reduce (fn [msg k] (update msg k v3->v4)) msg v3-fields)]
    msg))

(defn- disconnect-all [node-id label]
  (for [[src-node-id src-label] (g/sources-of node-id label)]
    (g/disconnect src-node-id src-label node-id label)))

(g/defnode GuiNode
  (inherits scene/ScalableSceneNode)

  (property index g/Int (dynamic visible (g/always false)))
  (property type g/Keyword (dynamic visible (g/always false)))

  (property id g/Str)
  (property size types/Vec3 (dynamic visible box-pie-text?))
  (property color types/Color (dynamic visible box-pie-text?))
  (property alpha g/Num
    (value (g/fnk [color] (get color 3)))
    (set (fn [basis this new-value]
          (g/update-property (g/node-id this) :color (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  (property template g/Str (dynamic visible template?))
  (property inherit-alpha g/Bool)

  ; Text
  (property text g/Str (dynamic visible text?))
  (property line-break g/Bool (dynamic visible text?))
  (property font g/Str
            (dynamic edit-type (g/fnk [fonts] (properties/->choicebox fonts)))
            (dynamic visible text?))
  (property outline types/Color (dynamic visible text?))
  (property outline-alpha g/Num (dynamic visible text?)
    (value (g/fnk [outline] (get outline 3)))
    (set (fn [basis this new-value]
          (g/update-property (g/node-id this) :outline (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  (property shadow types/Color (dynamic visible text?))
  (property shadow-alpha g/Num (dynamic visible text?)
    (value (g/fnk [shadow] (get shadow 3)))
    (set (fn [basis this new-value]
          (g/update-property (g/node-id this) :shadow (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))

  (property texture g/Str
            (dynamic edit-type (g/fnk [textures] (properties/->choicebox (cons "" (keys textures)))))
            (dynamic visible box-pie?)
            (value (g/fnk [texture-input] texture-input))
            (set (fn [basis this new-value]
                   (prn "set" new-value)
                   (let [self (g/node-id this)
                         textures (g/node-value basis self :textures)]
                     (prn "set-tex" textures)
                     (concat
                       (for [label [:texture-input :gpu-texture :anim-data]]
                         (disconnect-all self label))
                       (if (contains? textures new-value)
                         (let [tex-node (textures new-value)]
                           (concat
                             (g/connect tex-node :name self :texture-input)
                             (g/connect tex-node :gpu-texture self :gpu-texture)
                             (g/connect tex-node :anim-data self :anim-data)))
                         []))))))
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
  (input parent g/Str)
  (input layers [g/Str])
  (input texture-input g/Str)
  (input gpu-texture g/Any)
  (input anim-data g/Any)
  (input textures {g/Str g/NodeID})
  (input fonts [g/Str])
  (input child-scenes g/Any :array)
  (output node-outline g/Any (g/fnk [_node-id id index node-outlines type]
                                    {:node-id _node-id
                                     :label id
                                     :index index
                                     :icon (case type
                                             :type-box box-icon
                                             :type-text text-icon
                                             :type-pie pie-icon
                                             :type-template template-icon)
                                     :children (sort-by :index node-outlines)}))
  (output pb-msg g/Any produce-node-msg)
  (output aabb g/Any (g/fnk [pivot size] (let [offset-fn (partial mapv + (pivot-offset pivot size))
                                               [min-x min-y _] (offset-fn [0 0 0])
                                               [max-x max-y _] (offset-fn size)]
                                           (-> (geom/null-aabb)
                                             (geom/aabb-incorporate min-x min-y 0)
                                             (geom/aabb-incorporate max-x max-y 0)))))
  (output scene g/Any :cached produce-node-scene))

(g/defnode ImageTextureNode
  (input image BufferedImage)
  (output gpu-texture g/Any :cached (g/fnk [_node-id image] (texture/image-texture _node-id image)))
  (output anim-data g/Any (g/fnk [image]
                            {:width (.getWidth image)
                             :height (.getHeight image)
                             :frames [[[0 1] [0 0] [1 0] [1 1]]]
                             :uv-transforms [(TextureSetGenerator$UVTransform.)]})))

(g/defnode TextureNode
  (property name g/Str)
  (input texture (g/protocol resource/Resource))
  (input gpu-texture g/Any)
  (input anim-data g/Any)
  (input image-texture g/NodeID :cascade-delete)
  (output anim-data g/Any :cached (g/fnk [_node-id name anim-data]
                                    ; Messy since we have to deal with plain images and atlas/tilesources
                                    (if-let [src-node (g/node-feeding-into _node-id :anim-data)]
                                      (if (g/node-instance? ImageTextureNode src-node)
                                        {name anim-data}
                                        (into {} (map (fn [[id data]] [(format "%s/%s" name id) data]) anim-data)))
                                      {})))
  (output outline g/Any (g/fnk [_node-id name]
                          {:node-id _node-id
                           :label name
                           :icon texture-icon}))
  (output pb-msg g/Any (g/fnk [name texture]
                         {:name name
                          :texture (proj-path texture)}))
  (output texture-data g/Any (g/fnk [_node-id anim-data]
                               [_node-id (keys anim-data)]))
  (output gpu-texture g/Any (g/fnk [gpu-texture] gpu-texture)))

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
  (property id g/Str (default "") (dynamic visible (g/always false)))
  (input node-outlines g/Any :array)
  (input child-scenes g/Any :array)
  (output nodes-outline g/Any :cached (g/fnk [_node-id node-outlines]
                                             {:node-id _node-id
                                              :label "Nodes"
                                              :icon virtual-icon
                                              :children (vec (sort-by :index node-outlines))}))
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (map :aabb child-scenes))
                                      :children child-scenes})))

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

(g/defnk produce-scene [scene-dims aabb child-scenes]
  (let [w (:width scene-dims)
        h (:height scene-dims)]
    {:aabb aabb
     :renderable {:render-fn render-lines
                  :passes [pass/transparent]
                  :batch-key []
                  :user-data {:line-data [[0 0 0] [w 0 0] [w 0 0] [w h 0] [w h 0] [0 h 0] [0 h 0] [0 0 0]]
                              :line-color colors/defold-white}}
     :children child-scenes}))

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

  (property script (g/protocol resource/Resource))
  (property material (g/protocol resource/Resource))
  (property adjust-reference g/Keyword (dynamic edit-type (g/always (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))

  (input nodes-node g/NodeID)
  (input fonts-node g/NodeID)
  (input textures-node g/NodeID)
  (input layers-node g/NodeID)
  (input layouts-node g/NodeID)
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
  (input child-scenes g/Any :array)

  (input layers g/Any :array)
  (input texture-data g/Any :array)
  (input fonts g/Any :array)

  (output aabb AABB (g/fnk [scene-dims child-scenes]
                           (let [w (:width scene-dims)
                                 h (:height scene-dims)
                                 scene-aabb (-> (geom/null-aabb)
                                              (geom/aabb-incorporate 0 0 0)
                                              (geom/aabb-incorporate w h 0))]
                             (reduce geom/aabb-union scene-aabb (map :aabb child-scenes)))))
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
  (output textures {g/Str g/NodeID} (g/fnk [texture-data]
                                      (prn "td" texture-data)
                                      (prn "texs" (into {} (mapcat (fn [[_node-id anims]] (mapv #(vector % _node-id) anims)) texture-data))) (into {} (mapcat (fn [[_node-id anims]] (map #(vector % _node-id) anims)) texture-data))))
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
    (g/connect parent :id gui-node :parent)
    (g/connect gui-node :_node-id self :nodes)
    (g/connect gui-node :node-outline parent :node-outlines)
    (g/connect gui-node :scene parent :child-scenes)
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
    (g/connect texture :texture-data self :texture-data)
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

(defn make-texture-node [self parent name resource]
  (g/make-nodes (g/node-id->graph-id self) [texture [TextureNode
                                                     :name name
                                                     :texture resource]]
    (attach-texture self parent texture)))

(defn- color-alpha [node-desc color-field alpha-field]
  (let [color (get node-desc color-field)
        alpha (if (protobuf/field-set? node-desc alpha-field) (get node-desc alpha-field) (get color 3))]
    (conj (subvec color 0 3) alpha)))

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
      (g/make-nodes graph-id [fonts-node FontsNode]
                    (g/connect fonts-node :_node-id self :fonts-node)
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :fonts-outline self :fonts-outline)
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [FontNode
                                                    :name (:name font-desc)
                                                    :font (workspace/resolve-resource resource (:font font-desc))]]
                                    (attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node TexturesNode]
                    (g/connect textures-node :_node-id self :textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :textures-outline self :textures-outline)
                    (for [texture-desc (:textures scene)]
                      (let [resource (workspace/resolve-resource resource (:texture texture-desc))]
                        ; Messy because we need to deal with legacy standalone image files
                        (let [type (resource/resource-type resource)
                              outputs (g/declared-outputs (:node-type type))]
                          (if (outputs :anim-data)
                            (g/make-nodes graph-id [texture [TextureNode :name (:name texture-desc)]]
                              (attach-texture self textures-node texture)
                              (project/connect-resource-node project resource texture [[:resource :texture]
                                                                                       [:gpu-texture :gpu-texture]
                                                                                       [:anim-data :anim-data]]))
                            (g/make-nodes graph-id [img-texture [ImageTextureNode]
                                                   texture [TextureNode :name (:name texture-desc)]]
                              (g/connect img-texture :_node-id texture :image-texture)
                              (g/connect img-texture :gpu-texture texture :gpu-texture)
                              (g/connect img-texture :anim-data texture :anim-data)
                              (project/connect-resource-node project resource img-texture [[:content :image]])
                              (project/connect-resource-node project resource texture [[:resource :texture]])
                              (attach-texture self textures-node texture)))))))
      (g/make-nodes graph-id [layers-node LayersNode]
                    (g/connect layers-node :_node-id self :layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :layers-outline self :layers-outline)
                    (for [layer-desc (:layers scene)]
                      (g/make-nodes graph-id [layer [LayerNode
                                                     :name (:name layer-desc)]]
                                    (attach-layer self layers-node layer))))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                    (g/connect layouts-node :_node-id self :layouts-node)
                    (g/connect layouts-node :_node-id self :nodes)
                    (g/connect layouts-node :layouts-outline self :layouts-outline)
                    (for [layout-desc (:layouts scene)]
                      (g/make-nodes graph-id [layout [LayoutNode
                                                      :name (:name layout-desc)
                                                      :nodes (:nodes layout-desc)]]
                        (attach-layout self layouts-node layout))))
      (g/make-nodes graph-id [nodes-node NodesNode]
        (g/connect nodes-node :_node-id self :nodes-node)
        (g/connect nodes-node :_node-id self :nodes)
        (g/connect nodes-node :nodes-outline self :nodes-outline)
        (g/connect nodes-node :scene self :child-scenes)
        (loop [node-descs (:nodes scene)
               id->node {}
               all-tx-data []
               index 0]
          (if-let [node-desc (first node-descs)]
            (let [color (color-alpha node-desc :color :alpha)
                  outline (color-alpha node-desc :outline :outline-alpha)
                  shadow (color-alpha node-desc :shadow :shadow-alpha)
                  tx-data (g/make-nodes graph-id [gui-node [GuiNode
                                                           :index index
                                                           :type (:type node-desc)
                                                           :id (:id node-desc)
                                                           :position (v4->v3 (:position node-desc))
                                                           :rotation (v4->v3 (:rotation node-desc))
                                                           :scale (v4->v3 (:scale node-desc))
                                                           :size (v4->v3 (:size node-desc))
                                                           :color color
                                                           :blend-mode (:blend-mode node-desc)
                                                           :text (:text node-desc)
                                                           :font (:font node-desc)
                                                           :x-anchor (:xanchor node-desc)
                                                           :y-anchor (:yanchor node-desc)
                                                           :pivot (:pivot node-desc)
                                                           :outline outline
                                                           :shadow shadow
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
                             (attach-gui-node self parent gui-node (:type node-desc)))
                           ; Needs to be done after attaching so textures etc can be fetched
                           (g/set-property gui-node :texture (:texture node-desc)))
                 node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
             (recur (rest node-descs) (assoc id->node (:id node-desc) node-id) (into all-tx-data tx-data) (inc index)))
            all-tx-data))))))

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

(defn- outline-parent [child]
  (first (filter some? (map (fn [[_ output target input]]
                              (when (= output :outline) [target input]))
                            (g/outputs child)))))

(defn- outline-move! [outline node-id offset]
  (let [outline (sort-by :index outline)
        new-order (sort-by second (map-indexed (fn [i entry]
                                                 (let [nid (:node-id entry)
                                                       new-index (+ (* 2 i) (if (= node-id nid) (* offset 3) 0))]
                                                   [nid new-index]))
                                               outline))
        packed-order (map-indexed (fn [i v] [(first v) i]) new-order)]
    (g/transact
      (for [[node-id index] packed-order]
        (g/set-property node-id :index index)))))

(handler/defhandler :move-up :global
  (enabled? [selection] (and (= 1 (count selection))
                             (let [selected (first selection)]
                               (g/node-instance? GuiNode selected))))
  (run [selection] (let [selected (first selection)
                         [target input] (outline-parent selected)]
                     (outline-move! (g/node-value target input) selected -1))))

(handler/defhandler :move-down :global
  (enabled? [selection] (and (= 1 (count selection))
                             (let [selected (first selection)]
                               (g/node-instance? GuiNode selected))))
  (run [selection] (let [selected (first selection)
                         [target input] (outline-parent selected)]
                     (outline-move! (g/node-value target input) selected 1))))

(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Gui"
                  :id ::gui
                  :children [{:label "Move Up"
                              :acc "Alt+UP"
                              :command :move-up}
                             {:label "Move Down"
                              :acc "Alt+DOWN"
                              :command :move-down}]}])
