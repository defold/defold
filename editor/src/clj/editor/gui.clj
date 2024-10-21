;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.gui
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.gl.vertex2 :as vtx2]
            [editor.graph-util :as gu]
            [editor.gui-clipping :as clipping]
            [editor.handler :as handler]
            [editor.material :as material]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.particlefx :as particlefx]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.util :as eutil]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [com.dynamo.gamesys.proto Gui$NodeDesc Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds Gui$NodeDesc$Pivot Gui$NodeDesc$SizeMode Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$SceneDesc$FontDesc Gui$SceneDesc$LayerDesc Gui$SceneDesc$LayoutDesc Gui$SceneDesc$MaterialDesc Gui$SceneDesc$ParticleFXDesc Gui$SceneDesc$ResourceDesc Gui$SceneDesc$TextureDesc]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.texture TextureLifecycle]
           [editor.pose Pose]
           [internal.graph.types Arc]
           [java.awt.image BufferedImage]
           [javax.vecmath Quat4d]))

(set! *warn-on-reflection* true)

(def ^:private texture-icon "icons/32/Icons_25-AT-Image.png")
(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private gui-icon "icons/32/Icons_38-GUI.png")
(def ^:private virtual-icon "icons/32/Icons_01-Folder-closed.png")
(def ^:private layer-icon "icons/32/Icons_42-Layers.png")
(def ^:private layout-icon "icons/32/Icons_50-Display-profiles.png")
(def ^:private template-icon gui-icon)
(def ^:private text-icon "icons/32/Icons_39-GUI-Text-node.png")
(def ^:private box-icon "icons/32/Icons_40-GUI-Box-node.png")
(def ^:private pie-icon "icons/32/Icons_41-GUI-Pie-node.png")
(def ^:private material-icon "icons/32/Icons_25-AT-Image.png")

(def pb-def {:ext "gui"
             :label "Gui"
             :icon gui-icon
             :icon-class :design
             :pb-class Gui$SceneDesc
             :resource-fields [:script :material [:fonts :font] [:textures :texture] [:materials :material] [:particlefxs :particlefx] [:resources :path]]
             :tags #{:component :non-embeddable}
             :tag-opts {:component {:transform-properties #{}}}})

; Fallback shader

(vtx2/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(vtx2/defvertex uv-color-vtx
  (vec3 position)
  (vec2 texcoord0)
  (vec4 color)
  (vec1 page_index))

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
  (uniform sampler2D texture_sampler)
  (defn void main []
    (setq gl_FragColor (* var_color (texture2D texture_sampler var_texcoord0.xy)))))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

(shader/defshader gui-id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader gui-id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D texture_sampler var_texcoord0.xy))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def id-shader (shader/make-shader ::id-shader gui-id-vertex-shader gui-id-fragment-shader {"id" :id}))

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

(defn- ->color-vtx-vb [vs colors vcount]
  (let [vb (->color-vtx vcount)
        vs (mapv (comp vec concat) vs colors)]
    (persistent! (reduce conj! vb vs))))

(defn- gen-lines-vb
  [renderables]
  (let [vcount (transduce (map (comp count :line-data :user-data)) + renderables)]
    (when (pos? vcount)
      (vtx2/flip! (reduce (fn [vb {:keys [world-transform user-data selected] :as renderable}]
                            (let [{:keys [line-data line-color]} user-data
                                  [r g b a] (or line-color (colors/renderable-outline-color renderable))]
                              (reduce (fn [vb [x y z]]
                                        (color-vtx-put! vb x y z r g b a))
                                      vb
                                      (geom/transf-p world-transform line-data))))
                          (->color-vtx vcount)
                          renderables)))))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (when-let [vb (gen-lines-vb renderables)]
    (let [vertex-binding (vtx2/use-with ::lines vb line-shader)]
      (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

(defn- premul [color]
  (let [[r g b a] color]
    [(* r a) (* g a) (* b a) a]))

(defn- gen-geom-vb
  [renderables]
  (let [vcount (transduce (map (comp count :geom-data :user-data)) + renderables)]
    (when (pos? vcount)
      (vtx2/flip! (reduce (fn [vb {:keys [world-transform user-data] :as renderable}]
                            (if-let [geom-data (seq (:geom-data user-data))]
                              (let [[r g b a] (premul (:color user-data))
                                    page-index (:page-index user-data)]
                                (loop [vb vb
                                       [[x y z :as vtx] & vs] (geom/transf-p world-transform geom-data)
                                       [[u v :as uv] & uvs] (:uv-data user-data)]
                                  (if vtx
                                    (recur (uv-color-vtx-put! vb x y z u v r g b a page-index) vs uvs)
                                    vb)))
                              vb))
                          (->uv-color-vtx vcount)
                          renderables)))))

(defn- gen-font-vb
  [gl user-data renderables]
  (let [font-data (get-in user-data [:text-data :font-data])
        text-entries (mapv (fn [r] (let [text-data (get-in r [:user-data :text-data])
                                         alpha (get-in (:user-data r) [:color 3])]
                                     (-> text-data
                                         (assoc :world-transform (:world-transform r))
                                         (update-in [:color 3] * alpha)
                                         (update-in [:outline 3] * alpha)
                                         (update-in [:shadow 3] * alpha))))
                           renderables)
        node-ids (into #{} (map :node-id) renderables)]
    (font/request-vertex-buffer gl node-ids font-data text-entries)))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])]
    (cond
      (contains? user-data :geom-data)
      (gen-geom-vb renderables)

      (get-in user-data [:text-data :font-data])
      (gen-font-vb gl user-data renderables)

      (contains? user-data :gen-vb)
      ((:gen-vb user-data) user-data renderables))))

(defn render-tris [^GL2 gl render-args renderables _rcount]
  (let [user-data (get-in renderables [0 :user-data])
        clipping-state (:clipping-state user-data)
        gpu-texture (or (get user-data :gpu-texture) @texture/white-pixel)
        material-shader (get user-data :material-shader)
        blend-mode (get user-data :blend-mode)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (condp = (:pass render-args)
        pass/transparent
        (let [shader (or material-shader shader)
              vertex-binding (if (instance? editor.gl.vertex2.VertexBuffer vb)
                               (vtx2/use-with ::tris vb shader)
                               (vtx/use-with ::tris vb shader))]
          (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
            (shader/set-samplers-by-index shader gl 0 (:texture-units gpu-texture))
            (clipping/setup-gl gl clipping-state)
            (gl/set-blend-mode gl blend-mode)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
            (clipping/restore-gl gl clipping-state)))

        pass/selection
        (let [vertex-binding (if (instance? editor.gl.vertex2.VertexBuffer vb)
                               (vtx2/use-with ::tris vb id-shader)
                               (vtx/use-with ::tris vb id-shader))]
          (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [id-shader vertex-binding gpu-texture]
            (shader/set-samplers-by-index shader gl 0 (:texture-units gpu-texture))
            (clipping/setup-gl gl clipping-state)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
            (clipping/restore-gl gl clipping-state)))))))

(defn- pivot->h-align [pivot]
  (case pivot
    (:pivot-e :pivot-ne :pivot-se) :right
    (:pivot-center :pivot-n :pivot-s) :center
    (:pivot-w :pivot-nw :pivot-sw) :left))

(defn- pivot->v-align [pivot]
  (case pivot
    (:pivot-ne :pivot-n :pivot-nw) :top
    (:pivot-e :pivot-center :pivot-w) :middle
    (:pivot-se :pivot-s :pivot-sw) :bottom))

(defn- pivot-offset [pivot size]
  (let [h-align (pivot->h-align pivot)
        v-align (pivot->v-align pivot)
        xs (case h-align
             :right -1.0
             :center -0.5
             :left 0.0)
        ys (case v-align
             :top -1.0
             :middle -0.5
             :bottom 0.0)]
    (mapv * size [xs ys 1])))

(defn- calc-aabb [pivot size]
  (let [offset-fn (partial mapv + (pivot-offset pivot size))
        [min-x min-y _] (offset-fn [0.0 0.0 0.0])
        [max-x max-y _] (offset-fn size)]
    (geom/coords->aabb [min-x min-y 0.0]
                       [max-x max-y 0.0])))

(defn- sort-by-angle [ps max-angle]
  (-> (sort-by (fn [[x y _]] (let [a (* (Math/atan2 y x) (if (< max-angle 0) -1.0 1.0))]
                              (cond-> a
                                (< a 0) (+ (* 2.0 Math/PI))))) ps)
    (vec)))

(defn- cornify [ps ^double max-angle]
  (let [corner-count (int (/ (+ (Math/abs max-angle) 45) 90))]
    (if (> corner-count 0)
      (let [right (if (< max-angle 0) -90 90)
            half-right (if (< max-angle 0) -45 45)]
        (-> ps
         (into (geom/chain (dec corner-count) (partial geom/rotate [0 0 right]) (geom/rotate [0 0 half-right] [[1 0 0]])))
         (sort-by-angle max-angle)))
      ps)))

(defn- pie-circling [segments ^double max-angle corners? ps]
  (let [cut-off? (< (Math/abs max-angle) 360)
        angle (* (/ 360 segments) (if (< max-angle 0) -1.0 1.0))
        segments (if cut-off?
                   (int (/ max-angle angle))
                   segments)]
    (cond-> (geom/chain segments (partial geom/rotate [0 0 angle]) ps)
      corners? (cornify max-angle)
      cut-off? (into (geom/rotate [0 0 max-angle] ps)))))

(def ^:private euler-v4->clj-quat (comp math/vecmath->clj math/euler->quat))

(defn- clj-quat->euler-v4 [clj-quat]
  (-> (doto (Quat4d.)
        (math/clj->vecmath clj-quat))
      math/quat->euler
      properties/round-vec-coarse
      (conj protobuf/float-zero)))

(def ^:private property-conversions
  [{:pb-field :position
    :prop-key :position
    :pb->prop protobuf/vector4->vector3
    :prop->pb protobuf/vector3->vector4-zero}
   {:pb-field :rotation
    :prop-key :rotation
    :pb->prop euler-v4->clj-quat
    :prop->pb clj-quat->euler-v4}
   {:pb-field :scale
    :prop-key :scale
    :pb->prop protobuf/vector4->vector3
    :prop->pb protobuf/vector3->vector4-one}
   {:pb-field :size
    :prop-key :manual-size
    :pb->prop protobuf/vector4->vector3
    :prop->pb protobuf/vector3->vector4-zero}
   {:pb-field :xanchor
    :prop-key :x-anchor}
   {:pb-field :yanchor
    :prop-key :y-anchor}])

(def ^:private pb-field-to-node-property-conversions
  (into {}
        (map (fn [{:keys [pb-field prop-key pb->prop]
                   :or {pb->prop identity}}]
               (pair pb-field (pair prop-key pb->prop))))
        property-conversions))

(def ^:private pb-field-index->pb-field (protobuf/fields-by-indices Gui$NodeDesc))

(def ^:private pb-field-index->prop-key
  (persistent!
    (reduce-kv (fn [pb-field-index->prop-key pb-field-index pb-field]
                 (if-some [[prop-key] (pb-field-to-node-property-conversions pb-field)]
                   (assoc! pb-field-index->prop-key pb-field-index prop-key)
                   pb-field-index->prop-key))
               (transient pb-field-index->pb-field)
               pb-field-index->pb-field)))

(def ^:private prop-key->pb-field-index
  (persistent!
    (reduce-kv (fn [prop-key->pb-field-index pb-field-index prop-key]
                 (assoc! prop-key->pb-field-index prop-key pb-field-index))
               (transient {})
               pb-field-index->prop-key)))

(declare get-registered-node-type-info get-registered-node-type-infos)

;; /////////////////////////////////////////////////////////////////////////////////////////////////////////

(def gui-node-parent-attachments
  [[:id :parent]
   [:texture-gpu-textures :texture-gpu-textures]
   [:texture-infos :texture-infos]
   [:material-infos :material-infos]
   [:material-shaders :material-shaders]
   [:font-shaders :font-shaders]
   [:font-datas :font-datas]
   [:font-names :font-names]
   [:layer-names :layer-names]
   [:layer->index :layer->index]

   [:spine-scene-element-ids :spine-scene-element-ids]
   [:spine-scene-infos :spine-scene-infos]
   [:spine-scene-names :spine-scene-names]

   [:particlefx-infos :particlefx-infos]
   [:particlefx-resource-names :particlefx-resource-names]
   [:resource-names :resource-names]
   [:id-prefix :id-prefix]
   [:current-layout :current-layout]])

(def gui-node-attachments
  [[:_node-id :nodes]
   [:node-ids :node-ids]
   [:node-id+child-index :child-indices]
   [:node-outline :child-outlines]
   [:scene :child-scenes]
   [:node-msgs :node-msgs]
   [:node-overrides :node-overrides]
   [:build-errors :child-build-errors]
   [:template-build-targets :template-build-targets]])

(defn- attach-gui-node [node-tree parent gui-node type]
  (concat
    (g/connect gui-node :id node-tree :ids)
    (g/connect node-tree :id-counts gui-node :id-counts)
    (for [[source target] gui-node-parent-attachments]
      (g/connect parent source gui-node target))
    (for [[source target] gui-node-attachments]
      (g/connect gui-node source parent target))))

(declare GuiSceneNode GuiNode NodeTree LayoutsNode LayoutNode BoxNode PieNode
         TextNode TemplateNode ParticleFXNode)

(declare get-registered-node-type-cls)

(def ^:private default-pb-node-type (protobuf/default Gui$NodeDesc :type)) ; E.g. :type-box.

(defn- node-desc->node-type-info [node-desc]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  (let [type (:type node-desc default-pb-node-type)
        custom-type (:custom-type node-desc 0)]
    (get-registered-node-type-info type custom-type)))

(def ^:private node-desc->node-type (comp :node-cls node-desc->node-type-info))

(defn- node->node-tree
  ([node]
   (node->node-tree (g/now) node))
  ([basis node]
   (if (g/node-instance? basis NodeTree node)
     node
     (recur basis (core/scope basis node)))))

(defn- node->gui-scene
  ([node]
   (node->gui-scene (g/now) node))
  ([basis node]
   (cond
     (g/node-instance? basis GuiSceneNode node)
     node

     (g/node-instance? basis GuiNode node)
     (node->gui-scene basis (node->node-tree basis node))

     :else
     (core/scope basis node))))

(defn- next-child-index [child-indices]
  (inc (reduce max -1 (map second child-indices))))

(defn- gen-gui-node-attach-fn [type]
  (fn [target source]
    (let [node-tree (node->node-tree target)
          taken-ids (g/node-value node-tree :id-counts)
          child-indices (g/node-value target :child-indices)
          next-index (next-child-index child-indices)]
      (concat
        (g/update-property source :id outline/resolve-id taken-ids)
        (g/set-property source :child-index next-index)
        (attach-gui-node node-tree target source type)))))

;; SDK api
(defn gen-outline-node-tx-attach-fn
  ([attach-fn]
   (gen-outline-node-tx-attach-fn attach-fn :names))
  ([attach-fn target-name-key]
   (fn [target source]
     (let [node-tree (node->gui-scene target)
           taken-id-names (g/node-value target target-name-key)]
       (concat
         (g/update-property source :name outline/resolve-id taken-id-names)
         (attach-fn node-tree target source))))))

;; Schema validation is disabled because it severely affects project load times.
;; You might want to enable these before making drastic changes to Gui nodes.

;; SDK api
(g/deftype GuiResourceNames s/Any #_(s/constrained #{s/Str} sorted?))

(s/def ^:private MaterialInfo {(s/optional-key :max-page-count) s/Int})

(s/def ^:private TextureInfo {(s/optional-key :anim-data) {s/Keyword s/Any}
                              (s/optional-key :page-count) s/Int})

(g/deftype ^:private GuiResourceTextures s/Any #_{s/Str TextureLifecycle})
(g/deftype ^:private GuiResourceShaders s/Any #_{s/Str ShaderLifecycle})
(g/deftype ^:private GuiResourceMaterialInfos s/Any #_{s/Str MaterialInfo})
(g/deftype ^:private GuiResourceTextureInfos s/Any #_{s/Str TextureInfo})
(g/deftype ^:private FontDatas s/Any #_{s/Str {s/Keyword s/Any}})
(g/deftype ^:private SpineSceneElementIds s/Any #_{s/Str {:spine-anim-ids (s/constrained #{s/Str} sorted?)
                                                          :spine-skin-ids (s/constrained #{s/Str} sorted?)}})
(g/deftype ^:private SpineSceneInfos s/Any #_{s/Str {:spine-data-handle (s/maybe {s/Int s/Any})
                                                     :spine-bones (s/maybe {s/Keyword s/Any})
                                                     :spine-scene-scene (s/maybe {s/Keyword s/Any})
                                                     :spine-scene-pb (s/maybe {s/Keyword s/Any})}})
(g/deftype ^:private ParticleFXInfos s/Any #_{s/Str {:particlefx-scene (s/maybe {s/Keyword s/Any})}})
(g/deftype NameCounts {s/Str s/Int}) ;; SDK api
(g/deftype ^:private IDMap {s/Str s/Int})
(g/deftype ^:private TemplateData {:resource  (s/maybe (s/protocol resource/Resource))
                                   :overrides {s/Str s/Any}})

(g/deftype ^:private NodeIndex [(s/one s/Int "node-id") (s/one s/Int "index")])
(g/deftype ^:private NameIndex [(s/one s/Str "name") (s/one s/Int "index")])

(g/defnk override-node? [_this] (g/node-override? _this))
(g/defnk not-override-node? [_this] (not (g/node-override? _this)))

(defn- validate-contains [severity fmt prop-kw node-id coll key]
  (validation/prop-error severity node-id
                         prop-kw (fn [id ids]
                                   (when-not (contains? ids id)
                                     (format fmt id)))
                         key coll))

(defn- validate-gui-resource [severity fmt prop-kw node-id coll key]
  (validate-contains severity fmt prop-kw node-id coll key))

;; SDK api
(defn validate-required-gui-resource [fmt prop-kw node-id coll key]
  (or (validation/prop-error :fatal node-id prop-kw validation/prop-empty? key (properties/keyword->name prop-kw))
      (validate-gui-resource :fatal fmt prop-kw node-id coll key)))

;; SDK api
(defn validate-optional-gui-resource [fmt prop-kw node-id coll key]
  (when-not (empty? key)
    (validate-gui-resource :fatal fmt prop-kw node-id coll key)))

;; SDK api
(defn required-gui-resource-choicebox [coll]
  ;; The coll will contain a "" entry representing "No Selection". This is used
  ;; to lookup rendering resources in case the value has not been assigned.
  ;; We don't want any of these as an option in the dropdown.
  (properties/->choicebox (sort (remove empty? coll))))

;; SDK api
(defn optional-gui-resource-choicebox
  ;; The coll will contain a "" entry representing "No Selection". Remove this
  ;; before sorting the collection. We then provide the "" entry at the top.
  ([coll]
   (optional-gui-resource-choicebox coll sort))
  ([coll custom-sort-fn]
   (properties/->choicebox (cons "" (custom-sort-fn (remove empty? coll))) false)))

;; SDK api
(defn prop-unique-id-error [node-id prop-kw prop-value id-counts prop-name]
  (or (validation/prop-error :fatal node-id prop-kw validation/prop-empty? prop-value prop-name)
      (validation/prop-error :fatal node-id prop-kw (partial validation/prop-id-duplicate? id-counts) prop-value)))

;; SDK api
(defn prop-resource-error
  ([node-id prop-kw prop-value prop-name]
   (or (validation/prop-error :fatal node-id prop-kw validation/prop-nil? prop-value prop-name)
       (validation/prop-error :fatal node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))
  ([node-id prop-kw prop-value prop-name resource-ext]
   (or (prop-resource-error node-id prop-kw prop-value prop-name)
       (validation/prop-error :fatal node-id prop-kw validation/prop-resource-ext? prop-value resource-ext prop-name))))

;; SDK api
(defn references-gui-resource? [evaluation-context node-id prop-kw gui-resource-name]
  (and (g/property-value-origin? (:basis evaluation-context) node-id prop-kw)
       (= gui-resource-name (g/node-value node-id prop-kw evaluation-context))))

(defmulti update-gui-resource-reference (fn [gui-resource-type evaluation-context node-id _old-name _new-name]
                                          [(g/node-type-kw (:basis evaluation-context) node-id) gui-resource-type]))
(defmethod update-gui-resource-reference :default [_ _evaluation-context _node-id _old-name _new-name] nil)

;; used by (property x (set (partial ...)), thus evaluation-context in signature
;; SDK api
(defn update-gui-resource-references [gui-resource-type evaluation-context gui-resource-node-id old-name new-name]
  (assert (keyword? gui-resource-type))
  (assert (or (nil? old-name) (string? old-name)))
  (assert (string? new-name))
  (when (and (not (empty? old-name))
             (not (empty? new-name)))
    (when-some [scene (core/scope-of-type (:basis evaluation-context) gui-resource-node-id GuiSceneNode)]
      (let [node-ids-by-id (g/node-value scene :node-ids evaluation-context)]
        (when (and (not (g/error-value? node-ids-by-id))
                   (coll/not-empty node-ids-by-id))
          (into []
                (comp (map val)
                      (keep (fn [node-id]
                              (update-gui-resource-reference gui-resource-type evaluation-context node-id old-name new-name))))
                node-ids-by-id))))))

;; Base nodes

(def base-display-order [:id :generated-id scene/SceneNode])

(defn- validate-layer [emit-warnings? node-id layer-names layer]
  (when-not (empty? layer)
    ;; Layers are not brought in from template sources. The brought in nodes act
    ;; as if they belong to no layer if the layer does not exist in the scene,
    ;; but a warning is emitted.
    (if (g/property-value-origin? node-id :layer)
      (validate-contains :fatal "layer '%s' does not exist in the scene" :layer node-id layer-names layer)
      (when emit-warnings?
        (validate-contains :warning "layer '%s' from template scene does not exist in the scene - will use layer of parent" :layer node-id layer-names layer)))))

(defn- overridden-pb-field-indices [gui-node]
  (->> gui-node
       (gt/overridden-properties)
       (keys)
       (keep prop-key->pb-field-index)
       (sort)
       (vec)))

(def ^:private override-retained-pb-fields
  ;; These pb-fields will always be kept on override nodes. Other fields will be
  ;; stripped out unless their values deviate from the original node.
  [:type ; Not overridden or necessary, but improves readability in files.
   :custom-type ; Not overridden or necessary, but improves readability in files.
   :id ; Possibly overridden to reflect new position in scene hierarchy, but not listed among :overridden-fields. Used to locate the original node.
   :parent ; Possibly overridden to reflect new position in scene hierarchy, but not listed among :overridden-fields. No property exists for field.
   :template-node-child ; Signals that the node is overriding a node from a template scene. No property exists for field.
   :child-index ; Extra key sneaked into the node-desc to control order. No field exists for property.
   :overridden-fields]) ; Controls which pb-fields we apply override properties from when loading. No property exists for field.

(defn- strip-unused-overridden-fields-from-node-desc [node-desc]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  (into (coll/empty-with-meta node-desc)
        (keep (fn [pb-field]
                (some->> (node-desc pb-field)
                         (pair pb-field))))
        (into override-retained-pb-fields
              (map pb-field-index->pb-field)
              (:overridden-fields node-desc))))

(g/defnk produce-gui-base-node-msg [_this type custom-type child-index position rotation scale id generated-id color alpha inherit-alpha enabled layer parent]
  ;; Warning: This base output or any of the base outputs that derive from it
  ;; must not be cached due to overridden-fields reliance on _this. Only the
  ;; node-msg outputs of concrete nodes may be cached. In that case caching is
  ;; fine since such an output will have explicit dependencies on all the
  ;; property values it requires.
  (-> (protobuf/make-map-without-defaults Gui$NodeDesc
        :custom-type custom-type
        :template-node-child false
        :position (protobuf/vector3->vector4-zero position)
        :rotation (clj-quat->euler-v4 rotation)
        :scale (protobuf/vector3->vector4-one scale)
        :id (if (g/node-override? _this) generated-id id)
        :color color ; TODO: Not used by template (forced to [1.0 1.0 1.0 1.0]). Move?
        :alpha alpha
        :inherit-alpha inherit-alpha
        :enabled enabled
        :layer layer
        :parent parent
        :overridden-fields (overridden-pb-field-indices _this))
      (assoc
        :type type ; Explicitly include the type (pb-field is optional, so :type-box would be stripped otherwise).
        :child-index child-index))) ; Used to order sibling nodes in the SceneDesc.

(g/defnode GuiNode
  (inherits core/Scope)
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property child-index g/Int (dynamic visible (g/constantly false)) (default 0)) ; No protobuf counterpart.
  (property type g/Keyword (dynamic visible (g/constantly false))) ; Always assigned in load-fn.
  (property custom-type g/Int (dynamic visible (g/constantly false)) (default (protobuf/default Gui$NodeDesc :custom-type)))

  (input id-counts NameCounts)
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))

  (output node-id+child-index NodeIndex (g/fnk [_node-id child-index] [_node-id child-index]))

  (property id g/Str (default (protobuf/default Gui$NodeDesc :id))
            (dynamic error (g/fnk [_node-id id id-counts] (prop-unique-id-error _node-id :id id id-counts "Id")))
            (dynamic visible not-override-node?))
  (property generated-id g/Str ; Just for presentation.
            (dynamic label (g/constantly "Id"))
            (value (gu/passthrough id)) ; see (output id ...) below
            (dynamic read-only? (g/constantly true))
            (dynamic visible override-node?))
  (property color types/Color (default (protobuf/default Gui$NodeDesc :color))
            (dynamic visible (g/fnk [type] (not= type :type-template)))
            (dynamic edit-type (g/constantly {:type types/Color
                                              :ignore-alpha? true})))
  (property alpha g/Num (default (protobuf/default Gui$NodeDesc :alpha))
            (dynamic edit-type (g/constantly {:type :slider
                                              :min 0.0
                                              :max 1.0
                                              :precision 0.01})))
  (property inherit-alpha g/Bool (default (protobuf/default Gui$NodeDesc :inherit-alpha)))
  (property enabled g/Bool (default (protobuf/default Gui$NodeDesc :enabled)))

  (property layer g/Str (default (protobuf/default Gui$NodeDesc :layer))
            (dynamic edit-type (g/fnk [layer-names layer->index]
                                 (optional-gui-resource-choicebox layer-names (partial sort-by layer->index))))
            (dynamic error (g/fnk [_node-id layer layer-names] (validate-layer true _node-id layer-names layer))))
  (output layer-index g/Any :cached
          (g/fnk [layer layer->index] (layer->index layer)))

  (input parent g/Str)

  (input texture-gpu-textures GuiResourceTextures)
  (output texture-gpu-textures GuiResourceTextures (gu/passthrough texture-gpu-textures))
  (input texture-infos GuiResourceTextureInfos)
  (output texture-infos GuiResourceTextureInfos (gu/passthrough texture-infos))
  (input material-infos GuiResourceMaterialInfos)
  (output material-infos GuiResourceMaterialInfos (gu/passthrough material-infos))

  (input material-shaders GuiResourceShaders)
  (output material-shaders GuiResourceShaders (gu/passthrough material-shaders))

  (input font-shaders GuiResourceShaders)
  (output font-shaders GuiResourceShaders (gu/passthrough font-shaders))
  (input font-datas FontDatas)
  (output font-datas FontDatas (gu/passthrough font-datas))
  (input font-names GuiResourceNames)
  (output font-names GuiResourceNames (gu/passthrough font-names))
  (input layer-names GuiResourceNames)
  (output layer-names GuiResourceNames (gu/passthrough layer-names))
  (input layer->index g/Any)
  (output layer->index g/Any (gu/passthrough layer->index))

  (input spine-scene-element-ids SpineSceneElementIds)
  (output spine-scene-element-ids SpineSceneElementIds (gu/passthrough spine-scene-element-ids))
  (input spine-scene-infos SpineSceneInfos)
  (output spine-scene-infos SpineSceneInfos (gu/passthrough spine-scene-infos))
  (input spine-scene-names GuiResourceNames)
  (output spine-scene-names GuiResourceNames (gu/passthrough spine-scene-names))

  (input particlefx-infos ParticleFXInfos)
  (output particlefx-infos ParticleFXInfos (gu/passthrough particlefx-infos))
  (input particlefx-resource-names GuiResourceNames)
  (output particlefx-resource-names GuiResourceNames (gu/passthrough particlefx-resource-names))
  (input resource-names GuiResourceNames)
  (output resource-names GuiResourceNames (gu/passthrough resource-names))
  (input child-scenes g/Any :array)
  (input child-indices NodeIndex :array)
  (output node-outline-link resource/Resource (g/constantly nil))
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [child-outlines]
                                                                     (vec (sort-by :child-index child-outlines))))
  (output node-outline-reqs g/Any :cached (g/fnk []
                                            (mapv (fn [type-info]
                                                    {:node-type (:node-cls type-info)
                                                     :tx-attach-fn (gen-gui-node-attach-fn (:node-type type-info))})
                                                  (get-registered-node-type-infos))))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id child-index node-outline-link node-outline-children node-outline-reqs type custom-type own-build-errors _overridden-properties]
            (cond-> {:node-id _node-id
                     :node-outline-key id
                     :label id
                     :child-index child-index
                     :icon (:icon (get-registered-node-type-info type custom-type))
                     :child-reqs node-outline-reqs
                     :copy-include-fn (fn [node]
                                        (let [node-id (g/node-id node)]
                                          (and (g/node-instance? GuiNode node-id)
                                               (not= node-id (g/node-value node-id :parent)))))
                     :children node-outline-children
                     :outline-error? (g/error-fatal? own-build-errors)
                     :outline-overridden? (not (empty? _overridden-properties))}
                    (resource/openable-resource? node-outline-link) (assoc :link node-outline-link :outline-reference? true))))

  (output transform-properties g/Any scene/produce-scalable-transform-properties)
  (output gui-base-node-msg g/Any produce-gui-base-node-msg)
  (output node-msg g/Any :abstract)
  (input node-msgs g/Any :array)
  (output node-msgs g/Any (g/fnk [node-msgs node-msg]
                            (into [node-msg]
                                  (map #(dissoc % :child-index))
                                  (flatten
                                    (sort-by #(get-in % [0 :child-index])
                                             node-msgs)))))
  (output aabb g/Any :abstract)
  (output scene-children g/Any (g/fnk [child-scenes] (vec (sort-by (comp :child-index :renderable) child-scenes))))
  (output scene-updatable g/Any (g/constantly nil))
  (output scene-renderable g/Any (g/constantly nil))
  (output scene-outline-renderable g/Any (g/constantly nil))
  (output color+alpha types/Color (g/fnk [color alpha] (assoc color 3 alpha)))
  (output scene g/Any :cached (g/fnk [_node-id id aabb transform scene-children scene-renderable scene-outline-renderable scene-updatable]
                                     (cond-> {:node-id _node-id
                                              :node-outline-key id
                                              :aabb aabb
                                              :transform transform
                                              :renderable scene-renderable}

                                       scene-outline-renderable
                                       (assoc :children [{:node-id _node-id
                                                          :node-outline-key id
                                                          :aabb aabb
                                                          :renderable scene-outline-renderable}])

                                       scene-updatable
                                       (assoc :updatable scene-updatable)

                                       (seq scene-children)
                                       (update :children coll/into-vector scene-children))))

  (input node-ids IDMap :array)
  (output id g/Str (g/fnk [id-prefix id] (str id-prefix id)))
  (output node-ids IDMap (g/fnk [_node-id id node-ids] (reduce merge {id _node-id} node-ids)))

  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides id _overridden-properties]
                                         (into {id _overridden-properties}
                                               node-overrides)))
  (input current-layout g/Str)
  (output current-layout g/Str (gu/passthrough current-layout))
  (input child-build-errors g/Any :array)
  (output build-errors-gui-node g/Any (g/fnk [_node-id id id-counts layer layer-names]
                                        (g/package-errors _node-id
                                                          (prop-unique-id-error _node-id :id id id-counts "Id")
                                                          (validate-layer false _node-id layer-names layer))))
  (output own-build-errors g/Any (gu/passthrough build-errors-gui-node))
  (output build-errors g/Any (g/fnk [_node-id own-build-errors child-build-errors]
                               (g/package-errors _node-id
                                                 child-build-errors
                                                 own-build-errors)))
  (input template-build-targets g/Any :array)
  (output template-build-targets g/Any (gu/passthrough template-build-targets)))

(defmethod g/node-key ::GuiNode [node-id evaluation-context]
  ;; By implementing this multi-method, overridden property values will be
  ;; restored on OverrideNode GuiNodes when a template gui scene is reloaded
  ;; from disk during resource-sync. The id must be unique within the gui scene.
  (g/node-value node-id :id evaluation-context))

;; SDK api
(defmethod update-gui-resource-reference [::GuiNode :layer]
  [_ evaluation-context node-id old-name new-name]
  (when (references-gui-resource? evaluation-context node-id :layer old-name)
    (g/set-property node-id :layer new-name)))

(defn- validate-particlefx-adjust-mode [node-id adjust-mode]
  (validation/prop-error :warning
                         node-id
                         :adjust-mode
                         (fn [adjust-mode]
                           (when (= adjust-mode :adjust-mode-stretch)
                             "Adjust mode \"Stretch\" not supported for particlefx nodes."))
                         adjust-mode))

(def ^:private validate-material-resource (partial validate-optional-gui-resource "Material '%s' does not exist in the scene" :material))

(defn- validate-material-capabilities [_node-id material-infos material material-shader texture-infos texture]
  (let [is-paged-material (shader/is-using-array-samplers? material-shader)
        material-info (or (material-infos material)
                          (material-infos ""))
        material-max-page-count (:max-page-count material-info)
        texture-page-count (:page-count (texture-infos texture))]
    (validation/prop-error :fatal _node-id :material shader/page-count-mismatch-error-message is-paged-material texture-page-count material-max-page-count "Texture")))

(g/defnk produce-visual-base-node-msg [gui-base-node-msg visible blend-mode adjust-mode material pivot x-anchor y-anchor]
  (merge gui-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :visible visible
           :blend-mode blend-mode ; TODO: Not used by particlefx (forced to :blend-mode-alpha). Move?
           :adjust-mode adjust-mode
           :material material
           :pivot pivot ; TODO: Not used by particlefx (forced to :pivot-center). Move?
           :xanchor x-anchor
           :yanchor y-anchor)))

(g/defnode VisualNode
  (inherits GuiNode)

  (property visible g/Bool (default (protobuf/default Gui$NodeDesc :visible)))

  (property blend-mode g/Keyword (default (protobuf/default Gui$NodeDesc :blend-mode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$BlendMode))))
  (property adjust-mode g/Keyword (default (protobuf/default Gui$NodeDesc :adjust-mode))
            (dynamic error (g/fnk [_node-id adjust-mode type]
                                  (when (= type :type-particlefx)
                                    (validate-particlefx-adjust-mode _node-id adjust-mode))))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$AdjustMode))))
  (property pivot g/Keyword (default (protobuf/default Gui$NodeDesc :pivot))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$Pivot))))
  (property x-anchor g/Keyword (default (protobuf/default Gui$NodeDesc :xanchor))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$XAnchor))))
  (property y-anchor g/Keyword (default (protobuf/default Gui$NodeDesc :yanchor))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$YAnchor))))

  (property material g/Str
            (default "")
            (dynamic edit-type (g/fnk [material-infos] (optional-gui-resource-choicebox (keys material-infos))))
            (dynamic error (g/fnk [_node-id material-infos material]
                             (validate-material-resource _node-id material-infos material))))

  (output visual-base-node-msg g/Any produce-visual-base-node-msg)
  (output gui-scene g/Any (g/fnk [_node-id]
                                 (node->gui-scene _node-id)))

  (output material-shader ShaderLifecycle (g/fnk [material-shaders material]
                                            (or (material-shaders material)
                                                (material-shaders ""))))
  (output gpu-texture TextureLifecycle (g/constantly nil))
  (output scene-renderable-user-data g/Any (g/constantly nil))
  (output scene-renderable g/Any :cached
          (g/fnk [_node-id child-index layer-index blend-mode inherit-alpha gpu-texture material-shader scene-renderable-user-data visible enabled]
                 (let [clipping-state (:clipping-state scene-renderable-user-data)
                       page-index (or (:page-index scene-renderable-user-data) 0)
                       gpu-texture (or gpu-texture (:gpu-texture scene-renderable-user-data))
                       material-shader (or (:override-material-shader scene-renderable-user-data)
                                           material-shader)]
                   {:render-fn render-tris
                    :tags (set/union #{:gui} (:renderable-tags scene-renderable-user-data))
                    :passes [pass/transparent pass/selection]
                    :user-data (assoc scene-renderable-user-data
                                      :blend-mode blend-mode
                                      :gpu-texture gpu-texture
                                      :inherit-alpha inherit-alpha
                                      :material-shader material-shader
                                      :page-index page-index)
                    :batch-key {:clipping-state clipping-state
                                :blend-mode blend-mode
                                :gpu-texture gpu-texture
                                :material-shader material-shader}
                    :select-batch-key _node-id
                    :child-index child-index
                    :layer-index layer-index
                    :topmost? true
                    :visible-self? (and visible enabled)
                    :visible-children? enabled
                    :pass-overrides {pass/outline {:batch-key ::outline}}})))

  (output scene-outline-renderable g/Any :cached
          (g/fnk [_node-id child-index layer-index scene-renderable-user-data visible enabled]
                 {:render-fn render-lines
                  :tags (set/union #{:gui :outline} (:renderable-tags scene-renderable-user-data))
                  :passes [pass/outline]
                  :user-data (select-keys scene-renderable-user-data [:line-data])
                  :batch-key nil
                  :select-batch-key _node-id
                  :child-index child-index
                  :layer-index layer-index
                  :topmost? true
                  :visible-self? (and visible enabled)
                  :visible-children? enabled}))

  (output build-errors-visual-node g/Any :cached
          (g/fnk [_node-id build-errors-gui-node material material-infos]
            (g/package-errors _node-id
                              build-errors-gui-node
                              (validate-material-resource _node-id material-infos material))))

  (output own-build-errors g/Any (gu/passthrough build-errors-visual-node)))

(def ^:private validate-texture-resource (partial validate-optional-gui-resource "Texture '%s' does not exist in the scene" :texture))

(def ^:private size-pb-field-index (prop-key->pb-field-index :manual-size))

(def ^:private is-size-pb-field-index? (partial = size-pb-field-index))

(def ^:private is-size-mode-pb-field-index? (partial = (prop-key->pb-field-index :size-mode)))

(def ^:private default-size-mode (protobuf/default Gui$NodeDesc :size-mode))

(def ^:private default-manual-size [(float 200.0) (float 100.0) protobuf/float-zero])

(defn- strip-size-from-overridden-fields [overridden-fields]
  (util/removev is-size-pb-field-index? overridden-fields))

(defn- strip-redundant-size-from-node-desc [{:keys [type] :as node-desc}]
  ;; We don't want to write image sizes to the files, as it results in a lot of
  ;; changed files whenever an image changes dimensions. We also used to write a
  ;; dummy size value for auto-sized nodes to avoid a runtime culling issue that
  ;; has since been fixed in the engine.
  (let [effective-size-mode
        (case type
          (:type-particlefx :type-template) :size-mode-auto
          (:type-text) :size-mode-manual
          (:size-mode node-desc default-size-mode))

        size-is-valuable
        (case effective-size-mode
          :size-mode-manual true
          (case type
            (:type-box :type-pie) (coll/empty? (:texture node-desc)) ; If no texture is assigned, these use the :size value.
            false))]

    (if size-is-valuable
      node-desc ; Return unaltered.
      (-> node-desc
          (dissoc :size)
          (protobuf/sanitize :overridden-fields strip-size-from-overridden-fields)))))

(defn- add-size-to-overridden-fields [overridden-fields]
  (vec (sort (conj overridden-fields size-pb-field-index))))

(defn- add-size-to-overridden-fields-in-node-desc [node-desc]
  (update node-desc :overridden-fields add-size-to-overridden-fields))

(defn- fixup-shape-base-node-size-overrides [node-desc]
  (let [size-mode (:size-mode node-desc)
        overridden-fields (:overridden-fields node-desc)]
    (cond-> node-desc

            ;; Previously, image sizes were written to the files, but if a node
            ;; overrode the size-mode of its original, its size field would not
            ;; be flagged as overridden unless it had diverged from the image
            ;; size. This was clearly a bug, but now unless we also flag the
            ;; size field as overridden, the manual size stored in the file will
            ;; be overwritten by the size of its auto-sized original (which will
            ;; be zero now that we strip away auto-sized node sizes).
            (and (not= :size-mode-auto size-mode) ; May be nil, which means :size-mode-manual.
                 (some is-size-mode-pb-field-index? overridden-fields)
                 (not-any? is-size-pb-field-index? overridden-fields))
            (add-size-to-overridden-fields-in-node-desc))))

(g/defnk produce-shape-base-node-msg [visual-base-node-msg manual-size size-mode texture clipping-mode clipping-visible clipping-inverted]
  (-> visual-base-node-msg
      (merge (protobuf/make-map-without-defaults Gui$NodeDesc
               :size (protobuf/vector3->vector4-zero manual-size)
               :size-mode size-mode
               :texture texture
               :clipping-mode clipping-mode
               :clipping-visible clipping-visible
               :clipping-inverted clipping-inverted))
      (fixup-shape-base-node-size-overrides)))

(defn- visible-size-property-label [size-mode texture]
  (if (and (= :size-mode-auto size-mode)
           (coll/not-empty texture))
    :texture-size
    :manual-size))

(g/defnode ShapeNode
  (inherits VisualNode)

  (property manual-size types/Vec3 (default (protobuf/vector4->vector3 (protobuf/default Gui$NodeDesc :size)))
            (dynamic label (g/constantly "Size"))
            (dynamic visible (g/fnk [size-mode texture]
                               (= :manual-size (visible-size-property-label size-mode texture)))))
  (property texture-size types/Vec3 ; Just for presentation.
            (value (g/fnk [anim-data]
                     (if (some? anim-data)
                       [(float (:width anim-data)) (float (:height anim-data)) protobuf/float-zero]
                       protobuf/vector3-zero)))
            (dynamic label (g/constantly "Size"))
            (dynamic read-only? (g/constantly true))
            (dynamic visible (g/fnk [size-mode texture]
                               (= :texture-size (visible-size-property-label size-mode texture)))))
  (property size-mode g/Keyword (default (protobuf/default Gui$NodeDesc :size-mode))
            (set (fn [{:keys [basis] :as evaluation-context} self old-value new-value]
                   (when-some [texture (coll/not-empty (g/node-value self :texture evaluation-context))]
                     (cond
                       ;; Clear existing :manual-size override when switching
                       ;; from :size-mode-manual to :size-mode-auto with a
                       ;; texture assigned.
                       (and (= :manual-size (visible-size-property-label old-value texture))
                            (g/property-overridden? basis self :manual-size)
                            (let [effective-new-value
                                  (or new-value
                                      (let [original-node-id (g/override-original basis self)]
                                        (g/node-value original-node-id :size-mode evaluation-context)))]
                              (= :texture-size (visible-size-property-label effective-new-value texture))))
                       (g/clear-property self :manual-size)

                       ;; Use the size of the assigned texture as :manual-size
                       ;; when the user switches from :size-mode-auto to
                       ;; :size-mode-manual.
                       (and (= :size-mode-auto old-value)
                            (= :size-mode-manual new-value)
                            (properties/user-edit? self :size-mode evaluation-context))
                       (when-some [texture-infos (not-empty (g/node-value self :texture-infos evaluation-context))]
                         (when-some [anim-data (:anim-data (texture-infos texture))]
                           (let [texture-size [(float (:width anim-data)) (float (:height anim-data)) protobuf/float-zero]]
                             (g/set-property self :manual-size texture-size))))))))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$SizeMode))))
  (property material g/Str (default (protobuf/default Gui$NodeDesc :material))
            (dynamic edit-type (g/fnk [material-infos] (optional-gui-resource-choicebox (keys material-infos))))
            (dynamic error (g/fnk [_node-id material material-infos material-shader texture texture-infos]
                             (or (validate-material-resource _node-id material-infos material)
                                 (validate-material-capabilities _node-id material-infos material material-shader texture-infos texture)))))
  (property texture g/Str (default (protobuf/default Gui$NodeDesc :texture))
            (set (fn [{:keys [basis] :as evaluation-context} self old-value new-value]
                   ;; Clear any existing :manual-size override when assigning a
                   ;; texture will hide the :manual-size property.
                   (let [size-mode (g/node-value self :size-mode evaluation-context)]
                     (when (and (= :manual-size (visible-size-property-label size-mode old-value))
                                (g/property-overridden? basis self :manual-size)
                                (let [effective-new-value
                                      (or new-value
                                          (let [original-node-id (g/override-original basis self)]
                                            (g/node-value original-node-id :texture evaluation-context)))]
                                  (= :texture-size (visible-size-property-label size-mode effective-new-value))))
                       (g/clear-property self :manual-size)))))
            (dynamic edit-type (g/fnk [texture-infos] (optional-gui-resource-choicebox (keys texture-infos))))
            (dynamic error (g/fnk [_node-id texture-infos texture]
                             (validate-texture-resource _node-id texture-infos texture))))

  (property clipping-mode g/Keyword (default (protobuf/default Gui$NodeDesc :clipping-mode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$ClippingMode))))
  (property clipping-visible g/Bool (default (protobuf/default Gui$NodeDesc :clipping-visible)))
  (property clipping-inverted g/Bool (default (protobuf/default Gui$NodeDesc :clipping-inverted)))

  (output shape-base-node-msg g/Any produce-shape-base-node-msg)
  (output aabb g/Any :cached (g/fnk [pivot size] (calc-aabb pivot size)))
  (output anim-data g/Any (g/fnk [texture-infos texture]
                            (let [texture-info (or (texture-infos texture)
                                                   (texture-infos ""))]
                              (:anim-data texture-info))))
  (output gpu-texture TextureLifecycle (g/fnk [texture-gpu-textures texture]
                                         (or (texture-gpu-textures texture)
                                             (texture-gpu-textures ""))))
  (output size types/Vec3 (g/fnk [manual-size size-mode texture texture-size]
                            (if (or (= :size-mode-manual size-mode)
                                    (coll/empty? texture))
                              manual-size
                              texture-size)))
  (output build-errors-shape-node g/Any (g/fnk [build-errors-visual-node _node-id material material-infos material-shader texture texture-infos]
                                          (g/package-errors _node-id
                                                            build-errors-visual-node ; Validates material name.
                                                            (validate-texture-resource _node-id texture-infos texture)
                                                            (validate-material-capabilities _node-id material-infos material material-shader texture-infos texture))))
  (output own-build-errors g/Any (gu/passthrough build-errors-shape-node)))

(defmethod update-gui-resource-reference [::ShapeNode :texture]
  [_ evaluation-context node-id old-name new-name]
  (when (g/property-value-origin? (:basis evaluation-context) node-id :texture)
    (let [old-value (g/node-value node-id :texture evaluation-context)]
      (if (= old-name old-value)
        (g/set-property node-id :texture new-name)
        (let [[old-texture animation] (str/split old-value #"/")]
          (when (and (= old-name old-texture)
                     (not (empty? animation)))
            (g/set-property node-id :texture (str new-name "/" animation))))))))

(defmethod update-gui-resource-reference [::VisualNode :material]
  [_ evaluation-context node-id old-name new-name]
  (when (g/property-value-origin? (:basis evaluation-context) node-id :material)
    (let [old-value (g/node-value node-id :material evaluation-context)]
      (if (= old-name old-value)
        (g/set-property node-id :material new-name)))))

;; Box nodes

(g/defnk produce-box-node-msg [shape-base-node-msg slice9]
  (merge shape-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :slice9 slice9)))

(g/defnode BoxNode
  (inherits ShapeNode)

  (property slice9 types/Vec4 (default (protobuf/default Gui$NodeDesc :slice9))
            (dynamic read-only? (g/fnk [size-mode] (not= :size-mode-manual size-mode)))
            (dynamic edit-type (g/constantly {:type types/Vec4 :labels ["L" "T" "R" "B"]})))

  (display-order (into base-display-order
                       [:manual-size :texture-size :size-mode :enabled :visible :texture :material :slice9 :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  ;; Overloaded outputs
  (output node-msg g/Any :cached produce-box-node-msg)
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size-mode size color+alpha slice9 anim-data clipping-mode clipping-visible clipping-inverted]
            (let [frame (get-in anim-data [:frames 0])
                  vertex-data (texture-set/vertex-data frame size-mode size slice9 pivot)
                  user-data {:geom-data (:position-data vertex-data)
                             :line-data (:line-data vertex-data)
                             :uv-data (:uv-data vertex-data)
                             :color color+alpha
                             :page-index (:page-index frame 0)
                             :renderable-tags #{:gui-shape}}]
              (cond-> user-data
                      (not= :clipping-mode-none clipping-mode)
                      (assoc :clipping {:mode clipping-mode :inverted clipping-inverted :visible clipping-visible}))))))

;; Pie nodes

(def ^:private perimeter-vertices-min 4)
(def ^:private perimeter-vertices-max 1000)

(defn- validate-perimeter-vertices [node-id perimeter-vertices]
  (validation/prop-error :fatal node-id :perimeter-vertices validation/prop-outside-range? [perimeter-vertices-min perimeter-vertices-max] perimeter-vertices "Perimeter Vertices"))

(g/defnk produce-pie-node-msg [shape-base-node-msg outer-bounds inner-radius perimeter-vertices pie-fill-angle]
  (merge shape-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :outer-bounds outer-bounds
           :inner-radius inner-radius
           :perimeter-vertices perimeter-vertices
           :pie-fill-angle pie-fill-angle)))

(g/defnode PieNode
  (inherits ShapeNode)

  (property outer-bounds g/Keyword (default (protobuf/default Gui$NodeDesc :outer-bounds))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$PieBounds))))
  (property inner-radius g/Num (default (protobuf/default Gui$NodeDesc :inner-radius)))
  (property perimeter-vertices g/Int (default (protobuf/default Gui$NodeDesc :perimeter-vertices))
            (dynamic error (g/fnk [_node-id perimeter-vertices] (validate-perimeter-vertices _node-id perimeter-vertices))))

  (property pie-fill-angle g/Num (default (protobuf/default Gui$NodeDesc :pie-fill-angle)))

  (display-order (into base-display-order
                       [:manual-size :texture-size :size-mode :enabled :visible :texture :material :inner-radius :outer-bounds :perimeter-vertices :pie-fill-angle
                        :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output pie-data g/KeywordMap (g/fnk [_node-id outer-bounds inner-radius perimeter-vertices pie-fill-angle]
                                  (let [perimeter-vertices (max perimeter-vertices-min (min perimeter-vertices perimeter-vertices-max))]
                                    {:outer-bounds outer-bounds :inner-radius inner-radius
                                     :perimeter-vertices perimeter-vertices :pie-fill-angle pie-fill-angle})))

  ;; Overloaded outputs
  (output node-msg g/Any :cached produce-pie-node-msg)
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size color+alpha pie-data anim-data clipping-mode clipping-visible clipping-inverted]
                 (let [[w h _] size
                       anim-frame (get-in anim-data [:frames 0])
                       offset (mapv + (pivot-offset pivot size) [(* 0.5 w) (* 0.5 h) 0])
                       {:keys [outer-bounds inner-radius perimeter-vertices ^double pie-fill-angle]} pie-data
                       outer-rect? (= :piebounds-rectangle outer-bounds)
                       cut-off? (< (Math/abs pie-fill-angle) 360)
                       hole? (> inner-radius 0)
                       vs (pie-circling perimeter-vertices pie-fill-angle outer-rect? [[1 0 0]])
                       vs-outer (if outer-rect?
                                  (mapv (fn [[x y z]]
                                          (let [abs-x (Math/abs (double x))
                                                abs-y (Math/abs (double y))]
                                            (if (< abs-x abs-y)
                                              [(/ x abs-y) (/ y abs-y) z]
                                              [(/ x abs-x) (/ y abs-x) z]))) vs)
                                  vs)
                       vs-inner (if hole?
                                  (let [xs (/ inner-radius w 0.5)
                                        ys (* xs (/ h w))]
                                    (geom/scale [xs ys 1] vs))
                                  [[0 0 0]])
                       lines (->> (cond-> (vec (apply concat (partition 2 1 vs-outer)))
                                    hole? (into (apply concat (partition 2 1 vs-inner)))
                                    cut-off? (into [(first vs-outer) (first vs-inner) (last vs-outer) (last vs-inner)]))
                               (geom/scale [(* 0.5 w) (* 0.5 h) 1])
                               (geom/transl offset))
                       vs (if hole?
                            (reduce into []
                                    (concat
                                      (map #(do [%1 (second %2) (first %2)]) vs-outer (partition 2 1 vs-inner))
                                      (map #(do [%1 (first %2) (second %2)]) (drop 1 (cycle vs-inner)) (partition 2 1 vs-outer))))
                            (vec (mapcat into (repeat vs-inner) (partition 2 1 vs-outer))))
                       uvs (->> vs
                             (map #(subvec % 0 2))
                             (geom/transl [1 1])
                             (geom/scale [0.5 -0.5])
                             (geom/transl [0 1])
                             (geom/uv-trans (get-in anim-data [:uv-transforms 0])))
                       vs (->> vs
                            (geom/scale [(* 0.5 w) (* 0.5 h) 1])
                            (geom/transl offset))
                       user-data {:geom-data vs
                                  :line-data lines
                                  :uv-data uvs
                                  :color color+alpha
                                  :page-index (:page-index anim-frame 0)
                                  :renderable-tags #{:gui-shape}}]
                   (cond-> user-data
                     (not= :clipping-mode-none clipping-mode)
                     (assoc :clipping {:mode clipping-mode :inverted clipping-inverted :visible clipping-visible})))))
  (output own-build-errors g/Any (g/fnk [_node-id build-errors-shape-node perimeter-vertices]
                                   (g/package-errors _node-id
                                                     build-errors-shape-node
                                                     (validate-perimeter-vertices _node-id perimeter-vertices)))))

;; Text nodes

(def ^:private validate-font (partial validate-required-gui-resource "font '%s' does not exist in the scene" :font))

(g/defnk produce-text-node-msg [visual-base-node-msg manual-size text line-break font text-leading text-tracking outline outline-alpha shadow shadow-alpha]
  (merge visual-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :size (protobuf/vector3->vector4-zero manual-size)
           :text text
           :line-break line-break
           :font font
           :text-leading text-leading
           :text-tracking text-tracking
           :outline outline
           :outline-alpha outline-alpha
           :shadow shadow
           :shadow-alpha shadow-alpha)))

(g/defnode TextNode
  (inherits VisualNode)

  ; Text
  (property manual-size types/Vec3 (default (protobuf/vector4->vector3 (protobuf/default Gui$NodeDesc :size)))
            (dynamic label (g/constantly "Size")))
  (property text g/Str (default (protobuf/default Gui$NodeDesc :text))
            (dynamic edit-type (g/constantly {:type :multi-line-text})))
  (property line-break g/Bool (default (protobuf/default Gui$NodeDesc :line-break)))
  (property font g/Str (default (protobuf/default Gui$NodeDesc :font))
    (dynamic edit-type (g/fnk [font-names] (required-gui-resource-choicebox font-names)))
    (dynamic error (g/fnk [_node-id font-names font]
                     (validate-font _node-id font-names font))))
  (property text-leading g/Num (default (protobuf/default Gui$NodeDesc :text-leading)))
  (property text-tracking g/Num (default (protobuf/default Gui$NodeDesc :text-tracking)))
  (property outline types/Color (default (protobuf/default Gui$NodeDesc :outline))
            (dynamic edit-type (g/constantly {:type types/Color
                                              :ignore-alpha? true})))
  (property outline-alpha g/Num (default (protobuf/default Gui$NodeDesc :outline-alpha))
    (dynamic edit-type (g/constantly {:type :slider
                                      :min 0.0
                                      :max 1.0
                                      :precision 0.01})))
  (property shadow types/Color (default (protobuf/default Gui$NodeDesc :shadow))
            (dynamic edit-type (g/constantly {:type types/Color
                                              :ignore-alpha? true})))
  (property shadow-alpha g/Num (default (protobuf/default Gui$NodeDesc :shadow-alpha))
    (dynamic edit-type (g/constantly {:type :slider
                                      :min 0.0
                                      :max 1.0
                                      :precision 0.01})))

  (display-order (into base-display-order [:manual-size :enabled :visible :text :line-break :font :material :color :alpha :inherit-alpha :text-leading :text-tracking :outline :outline-alpha :shadow :shadow-alpha :layer]))

  (output font-data font/FontData (g/fnk [font-datas font]
                                    (or (font-datas font)
                                        (font-datas ""))))

  ;; Overloaded outputs
  (output node-msg g/Any :cached produce-text-node-msg)
  (output gpu-texture TextureLifecycle (g/fnk [font-data] (:texture font-data)))
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [manual-size font font-shaders material material-shaders pivot text-data color+alpha]
                 (let [[w h] manual-size
                       offset (pivot-offset pivot manual-size)
                       lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))
                       font-map (get-in text-data [:font-data :font-map])
                       texture-recip-uniform (font/get-texture-recip-uniform font-map)
                       material-shader (when (not (empty? material)) (material-shaders material))
                       font-shader (or material-shader (font-shaders font) (font-shaders ""))
                       font-shader (assoc-in font-shader [:uniforms "texture_size_recip"] texture-recip-uniform)]
                   ;; The material-shader output is used to propagate the shader
                   ;; from the GuiSceneNode to our child nodes. Thus, we cannot
                   ;; simply overload the material-shader output on this node.
                   ;; Instead, the base VisualNode will pick it up from here.
                   {:line-data lines
                    :text-data text-data
                    :color color+alpha
                    :override-material-shader font-shader
                    :renderable-tags #{:gui-text}})))
  (output text-layout g/Any :cached (g/fnk [manual-size font-data text line-break text-leading text-tracking]
                                           (font/layout-text (:font-map font-data) text line-break (first manual-size) text-tracking text-leading)))
  (output aabb g/Any :cached (g/fnk [pivot manual-size] (calc-aabb pivot manual-size)))
  (output aabb-size g/Any :cached (g/fnk [text-layout]
                                         [(:width text-layout) (:height text-layout) 0]))
  (output text-data g/KeywordMap (g/fnk [text-layout font-data color alpha outline outline-alpha shadow shadow-alpha aabb-size pivot]
                                   (cond-> {:text-layout text-layout
                                            :font-data font-data
                                            :color (assoc color 3 alpha)
                                            :outline (assoc outline 3 outline-alpha)
                                            :shadow (assoc shadow 3 shadow-alpha)
                                            :align (pivot->h-align pivot)}
                                           font-data (assoc :offset (let [[x y] (pivot-offset pivot aabb-size)
                                                                          h (second aabb-size)]
                                                                      [x (+ y (- h (get-in font-data [:font-map :max-ascent])))])))))
  (output own-build-errors g/Any (g/fnk [_node-id build-errors-visual-node font font-names]
                                   (g/package-errors _node-id
                                                     build-errors-visual-node
                                                     (validate-font _node-id font-names font)))))

(defmethod update-gui-resource-reference [::TextNode :font]
  [_ evaluation-context node-id old-name new-name]
  (when (references-gui-resource? evaluation-context node-id :font old-name)
    (g/set-property node-id :font new-name)))

;; Template nodes

(defn- template-outline-subst [err]
  ;; TODO: embed error so can warn in outline
  ;; outline content not really used, only children if any.
  {:node-id 0
   :node-outline-key ""
   :icon ""
   :label ""})

(defn- update-scene-tree [scene pred transform]
  (cond-> scene
    (contains? scene :children)
    (update :children (partial mapv (fn [child] (update-scene-tree child pred transform))))

    (pred scene)
    (transform)))

(defn- add-renderable-tags [scene tags]
  (update-scene-tree scene
                     #(contains? % :renderable)
                     #(update-in % [:renderable :tags] set/union tags)))

(defn- replace-renderable-tags [scene replacements]
  (update-scene-tree scene
                     #(contains? % :renderable)
                     #(update-in % [:renderable :tags] (fn [tags] (set (map (fn [tag] (get replacements tag tag)) tags))))))

(g/defnk produce-template-node-msg [gui-base-node-msg template-resource]
  (merge gui-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :template (resource/resource->proj-path template-resource)

           ;; TODO: We should not have to overwrite the base properties here. Refactor?
           :color [1.0 1.0 1.0 1.0])))

(g/defnode TemplateNode
  (inherits GuiNode)

  (property template TemplateData
            (dynamic read-only? override-node?)
            (dynamic edit-type (g/fnk [_node-id] {:type resource/Resource
                                                  :ext "gui"
                                                  :dialog-accept-fn (fn [r] (not= r (g/node-value (node->gui-scene _node-id) :resource)))
                                                  :to-type (fn [v] (:resource v))
                                                  :from-type (fn [r] {:resource r :overrides {}})}))
            (dynamic error (g/fnk [_node-id template-resource]
                             (prop-resource-error _node-id :template template-resource "Template")))
            (value (g/fnk [_node-id id template-resource template-overrides]
                     {:resource template-resource
                      :overrides (into {}
                                       (map (fn [[k v]]
                                              (pair (subs k (inc (count id)))
                                                    v)))
                                       template-overrides)}))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         current-scene (g/node-feeding-into basis self :template-resource)]
                     (concat
                       (when current-scene
                         (g/delete-node current-scene))
                       (when (and new-value (:resource new-value))
                         (when-some [{connect-tx-data :tx-data
                                      scene-node :node-id
                                      created-in-tx :created-in-tx} (project/connect-resource-node evaluation-context project (:resource new-value) self [])]
                           ;; Note: If the node was created in this transaction,
                           ;; it means it was a missing resource node. Such a
                           ;; node is marked defective, and we will not try to
                           ;; override its properties.
                           (concat
                             connect-tx-data
                             (let [properties-by-node-id
                                   (or (when-not created-in-tx
                                         (when-let [properties-by-id (coll/not-empty (:overrides new-value))]
                                           (let [node-ids-by-id (g/node-value scene-node :node-ids evaluation-context)]
                                             (when (and (not (g/error-value? node-ids-by-id))
                                                        (coll/not-empty node-ids-by-id))
                                               (comp properties-by-id
                                                     (into {}
                                                           (map (fn [[id node-id]]
                                                                  (pair node-id id)))
                                                           node-ids-by-id))))))
                                       {})]
                               ;; TODO: Check if we can filter based on connection label instead of source-node-id to be able to share :traverse-fn with other overrides.
                               (g/override scene-node {:traverse-fn (g/make-override-traverse-fn
                                                                      (fn [basis ^Arc arc]
                                                                        (let [source-node-id (.source-id arc)]
                                                                          (and (not= source-node-id current-scene)
                                                                               (g/node-instance-match basis source-node-id
                                                                                                      [GuiNode
                                                                                                       NodeTree
                                                                                                       GuiSceneNode
                                                                                                       LayoutsNode
                                                                                                       LayoutNode])))))
                                                       :properties-by-node-id properties-by-node-id}
                                           (fn [evaluation-context id-mapping]
                                             (let [or-scene (get id-mapping scene-node)]
                                               (concat
                                                 (for [[from to] [[:node-ids :node-ids]
                                                                  [:node-outline :template-outline]
                                                                  [:scene :template-scene]
                                                                  [:build-targets :template-build-targets]
                                                                  [:build-errors :child-build-errors]
                                                                  [:resource :template-resource]
                                                                  [:pb-msg :scene-pb-msg]
                                                                  [:node-overrides :template-overrides]]]
                                                   (g/connect or-scene from self to))
                                                 (for [[from to] [[:layer-names :layer-names]
                                                                  [:texture-gpu-textures :aux-texture-gpu-textures]
                                                                  [:texture-infos :aux-texture-infos]
                                                                  [:material-infos :aux-material-infos]
                                                                  [:material-shaders :aux-material-shaders]
                                                                  [:font-shaders :aux-font-shaders]
                                                                  [:font-datas :aux-font-datas]
                                                                  [:font-names :aux-font-names]

                                                                  [:spine-scene-element-ids :aux-spine-scene-element-ids]
                                                                  [:spine-scene-infos :aux-spine-scene-infos]
                                                                  [:spine-scene-names :aux-spine-scene-names]

                                                                  [:particlefx-infos :aux-particlefx-infos]
                                                                  [:particlefx-resource-names :aux-particlefx-resource-names]
                                                                  [:resource-names :aux-resource-names]
                                                                  [:template-prefix :id-prefix]
                                                                  [:current-layout :current-layout]]]
                                                   (g/connect self from or-scene to)))))))))))))))

  (display-order (into base-display-order [:enabled :template]))

  (input scene-pb-msg g/Any :substitute (fn [_] {:nodes []}))
  (input scene-build-targets g/Any)
  (output scene-build-targets g/Any (gu/passthrough scene-build-targets))

  (input template-resource resource/Resource :cascade-delete)
  (input template-outline outline/OutlineData :substitute template-outline-subst)
  (input template-scene g/Any)
  (input template-overrides g/Any)
  (output template-prefix g/Str (g/fnk [id] (str id "/")))

  ; Overloaded outputs
  (output node-outline-link resource/Resource (gu/passthrough template-resource))
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [template-outline current-layout]
                                                                     (get-in template-outline [:children 0 :children])))
  (output node-outline-reqs g/Any :cached (g/constantly []))
  (output node-msg g/Any :cached produce-template-node-msg)
  (output node-msgs g/Any :cached (g/fnk [id node-msg scene-pb-msg]
                                    (into [node-msg]
                                          (map #(-> %
                                                    (vary-meta update :templates (fnil conj []) node-msg)
                                                    (assoc :template-node-child true)
                                                    (cond-> (empty? (:parent %)) (assoc :parent id))))
                                          (:nodes scene-pb-msg))))
  (output node-overrides g/Any :cached (g/fnk [id _overridden-properties template-overrides]
                                              (-> {id _overridden-properties}
                                                (merge template-overrides))))
  (output aabb g/Any (g/fnk [template-scene]
                       (if (some? template-scene)
                         (:aabb template-scene)
                         geom/empty-bounding-box)))
  (output scene-children g/Any (g/fnk [_node-id id template-scene]
                                 (when (seq (:children template-scene))
                                   (-> template-scene
                                       (scene/claim-scene _node-id id)
                                       (add-renderable-tags #{:gui})
                                       :children))))
  (output scene-renderable g/Any :cached (g/fnk [color+alpha child-index layer-index inherit-alpha enabled]
                                                {:passes [pass/selection]
                                                 :child-index child-index
                                                 :layer-index layer-index
                                                 :visible-self? enabled
                                                 :visible-children? enabled
                                                 :user-data {:color color+alpha :inherit-alpha inherit-alpha}}))
  (output own-build-errors g/Any (g/fnk [_node-id build-errors-gui-node template-resource]
                                   (g/package-errors _node-id
                                                     build-errors-gui-node
                                                     (prop-resource-error _node-id :template template-resource "Template")))))


;; Particle FX

(def ^:private validate-particlefx-resource (partial validate-required-gui-resource "particlefx '%s' does not exist in the scene" :particlefx))

(defn- move-topmost [scene]
  (cond-> scene
    (contains? scene :children)
    (update :children (partial mapv move-topmost))

    (contains? scene :renderable)
    (assoc-in [:renderable :topmost?] true)))

(g/defnk produce-particlefx-node-msg [visual-base-node-msg particlefx]
  (-> visual-base-node-msg
      (merge (protobuf/make-map-without-defaults Gui$NodeDesc
               :particlefx particlefx
               :size-mode :size-mode-auto))))

(g/defnode ParticleFXNode
  (inherits VisualNode)

  (property particlefx g/Str (default (protobuf/default Gui$NodeDesc :particlefx))
    (dynamic edit-type (g/fnk [particlefx-resource-names] (required-gui-resource-choicebox particlefx-resource-names)))
    (dynamic error (g/fnk [_node-id particlefx particlefx-resource-names]
                     (validate-particlefx-resource _node-id particlefx-resource-names particlefx))))

  (property blend-mode g/Keyword (default (protobuf/default Gui$NodeDesc :blend-mode))
    (dynamic visible (g/constantly false)))
  (property pivot g/Keyword (default (protobuf/default Gui$NodeDesc :pivot))
    (dynamic visible (g/constantly false)))

  (display-order (into base-display-order
                       [:enabled :visible :particlefx :material :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output node-msg g/Any :cached produce-particlefx-node-msg)
  (output source-scene g/Any :cached (g/fnk [_node-id id particlefx-infos particlefx child-index layer-index material-shader color+alpha]
                                            (when-let [source-scene (get-in particlefx-infos [particlefx :particlefx-scene])]
                                              (-> source-scene
                                                  (scene/claim-scene _node-id id)
                                                  (move-topmost)
                                                  (add-renderable-tags #{:gui})
                                                  (replace-renderable-tags {:particlefx :gui-particlefx})
                                                  (update :renderable
                                                          (fn [r]
                                                            (-> r
                                                                (assoc :child-index child-index)
                                                                (update :user-data (fn [ud]
                                                                                     (-> ud
                                                                                         (update :emitter-sim-data (partial mapv (fn [d] (assoc d :shader material-shader))))
                                                                                         (assoc :inherit-alpha true)
                                                                                         (assoc :color color+alpha))))
                                                                (cond->
                                                                    layer-index (assoc :layer-index layer-index)))))))))
  (output gpu-texture TextureLifecycle (g/constantly nil))
  (output aabb g/Any (g/fnk [source-scene]
                       (if (some? source-scene)
                         (:aabb source-scene)
                         geom/empty-bounding-box)))
  (output scene g/Any :cached (g/fnk [_node-id aabb transform source-scene scene-children color+alpha inherit-alpha visible enabled]
                                (let [scene (if source-scene
                                              (if-some [updatable (some-> source-scene :updatable (assoc :node-id _node-id))]
                                                (scene/map-scene #(assoc % :updatable updatable) source-scene)
                                                source-scene)
                                              {:renderable {:passes [pass/selection]
                                                            :user-data {:color color+alpha :inherit-alpha inherit-alpha}}})]
                                  (-> scene
                                      (assoc
                                        :node-id _node-id
                                        :aabb aabb
                                        :transform transform
                                        :visible-self? (and visible enabled)
                                        :visible-children? enabled)
                                      (update :children coll/into-vector scene-children)))))
  (output own-build-errors g/Any (g/fnk [_node-id build-errors-visual-node particlefx particlefx-resource-names]
                                   (g/package-errors _node-id
                                                     build-errors-visual-node
                                                     (validate-particlefx-resource _node-id particlefx-resource-names particlefx)))))

(defmethod update-gui-resource-reference [::ParticleFXNode :particlefx]
  [_ evaluation-context node-id old-name new-name]
  (when (references-gui-resource? evaluation-context node-id :particlefx old-name)
    (g/set-property node-id :particlefx new-name)))

;; This InternalTextureNode is a drop-in replacement for TextureNode below.
;; It can be used in place of TextureNode when you already have a gpu-texture.
(g/defnode InternalTextureNode
  (property name g/Str)
  (property gpu-texture TextureLifecycle)
  (output texture-infos GuiResourceTextureInfos (g/fnk [name] (sorted-map name {:page-count texture/non-paged-page-count})))
  (output texture-gpu-textures GuiResourceTextures (g/fnk [name gpu-texture] {name gpu-texture})))

(g/defnk produce-texture-gpu-textures [_node-id anim-data name gpu-texture default-tex-params samplers]
  ;; If the referenced texture-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-texture entry for "".
  (when (and (some? anim-data) (some? gpu-texture))
    (let [gpu-texture (let [params (material/sampler->tex-params (first samplers) default-tex-params)]
                        (texture/set-params gpu-texture params))]
      ;; If the texture does not contain animations, we emit an entry for the "texture" name only.
      (if (empty? anim-data)
        {name gpu-texture}
        (into {}
              (map (fn [id] [(if id (format "%s/%s" name id) name) gpu-texture]))
              (keys anim-data))))))

(g/defnk produce-texture-infos [anim-data name texture-page-count]
  ;; The anim-data and the texture-page-count will be nil if the referenced
  ;; texture is missing or defective.
  (let [has-animations (not (coll/empty? anim-data))
        texture-info (cond-> {}
                             (some? texture-page-count) (assoc :page-count texture-page-count)
                             has-animations (assoc :anim-data anim-data))]
    ;; If the texture does not contain animations, we emit an entry for the
    ;; "texture" name only.
    (if-not has-animations
      (sorted-map name texture-info)
      (into (sorted-map)
            (map (fn [[anim-id anim-data]]
                   (let [texture-id (if anim-id (format "%s/%s" name anim-id) name)
                         texture-info (assoc texture-info :anim-data anim-data)]
                     (pair texture-id texture-info))))
            anim-data))))

(g/defnode TextureNode
  (inherits outline/OutlineNode)

  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :texture)))
  (property texture resource/Resource ; Required protobuf field.
            (value (gu/passthrough texture-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :texture-resource]
                                            [:gpu-texture :gpu-texture]
                                            [:texture-page-count :texture-page-count]
                                            [:anim-data :anim-data]
                                            [:anim-ids :anim-ids]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id texture]
                             (prop-resource-error _node-id :texture texture "Texture")))
            (dynamic edit-type (g/fnk [_node-id]
                                 {:type resource/Resource
                                  :ext (workspace/resource-kind-extensions (project/workspace (project/get-project _node-id)) :atlas)})))

  (input name-counts NameCounts)
  (input default-tex-params g/Any)
  (input texture-resource resource/Resource)
  (input image BufferedImage :substitute (constantly nil))
  (input gpu-texture g/Any :substitute nil)
  (input texture-page-count g/Int :substitute nil)
  (input anim-data g/Any :substitute (constantly nil))
  (input anim-ids g/Any :substitute (constantly []))
  (input samplers [g/KeywordMap] :substitute (constantly []))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name texture-resource build-errors]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key name
                                                              :label name
                                                              :icon texture-icon
                                                              :outline-error? (g/error-fatal? build-errors)}
                                                             (resource/openable-resource? texture-resource) (assoc :link texture-resource :outline-show-link? true))))
  (output pb-msg g/Any (g/fnk [name texture-resource]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$TextureDesc
                           :name name
                           :texture (resource/resource->proj-path texture-resource))))
  (output texture-gpu-textures GuiResourceTextures :cached produce-texture-gpu-textures)
  (output texture-infos GuiResourceTextureInfos :cached produce-texture-infos)
  (output build-errors g/Any (g/fnk [_node-id name name-counts texture]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")
                                                 (prop-resource-error _node-id :texture texture "Texture")))))

(g/defnode FontNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :font)))
  (property font resource/Resource ; Required protobuf field.
            (value (gu/passthrough font-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter
                     evaluation-context self old-value new-value
                     [:resource :font-resource]
                     [:font-data :font-data]
                     [:material-shader :font-shader]
                     [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id font]
                                  (prop-resource-error _node-id :font font "Font")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["font"]})))

  (input name-counts NameCounts)
  (input font-resource resource/Resource)
  (input font-data font/FontData :substitute (constantly nil))
  (input font-shader ShaderLifecycle :substitute (constantly nil))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name font-resource build-errors]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key name
                                                              :label name
                                                              :icon font-icon
                                                              :outline-error? (g/error-fatal? build-errors)}
                                                             (resource/openable-resource? font-resource) (assoc :link font-resource :outline-show-link? true))))
  (output pb-msg g/Any (g/fnk [name font-resource]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$FontDesc
                           :name name
                           :font (resource/resource->proj-path font-resource))))
  (output font-shaders GuiResourceShaders :cached (g/fnk [font-shader name]
                                                    ;; If the referenced font-resource is missing, we don't return an entry.
                                                    ;; This will cause every usage to fall back on the no-font entry for "".
                                                    (when (some? font-shader)
                                                      {name font-shader})))
  (output font-datas FontDatas :cached (g/fnk [font-data name]
                                         ;; If the referenced font-resource is missing, we don't return an entry.
                                         ;; This will cause every usage to fall back on the no-font entry for "".
                                         (when (some? font-data)
                                           {name font-data})))
  (output font-names GuiResourceNames (g/fnk [name] (sorted-set name)))
  (output build-errors g/Any (g/fnk [_node-id name name-counts font]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")
                                                 (prop-resource-error _node-id :font font "Font")))))

(g/defnode MaterialNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :material)))
  (property child-index g/Int (dynamic visible (g/constantly false)) (default 0))

  (property material resource/Resource ; Required protobuf field.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter
                     evaluation-context self old-value new-value
                     [:resource :material-resource]
                     [:shader :material-shader]
                     [:max-page-count :material-max-page-count]
                     [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id material]
                             (prop-resource-error _node-id :material material "Material")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]})))

  (input name-counts NameCounts)

  (input material-max-page-count g/Int :substitute nil)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle :substitute nil)

  (output node-id+child-index NodeIndex (g/fnk [_node-id child-index] [_node-id child-index]))
  (output name+child-index NameIndex (g/fnk [name child-index] [name child-index]))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name child-index build-errors]
                                                     {:node-id _node-id
                                                      :node-outline-key name
                                                      :label name
                                                      :icon layer-icon
                                                      :child-index child-index
                                                      :outline-error? (g/error-fatal? build-errors)}))
  (output resource-names GuiResourceNames (g/fnk [name] (sorted-set name)))
  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))
  (output pb-msg g/Any (g/fnk [name material-resource]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$MaterialDesc
                           :name name
                           :material (resource/resource->proj-path material-resource))))
  (output material-shaders GuiResourceShaders :cached (g/fnk [material-shader name]
                                                        ;; If the referenced material-resource is missing, we don't return an entry.
                                                        (when (some? material-shader)
                                                          {name material-shader})))
  (output material-infos GuiResourceMaterialInfos (g/fnk [name material-max-page-count]
                                                    ;; If the referenced material-resource is missing, material-max-page-count will be nil.
                                                    ;; We still want an entry in the map, but we will not attempt to validate the page count against the texture.
                                                    (let [material-info (if material-max-page-count
                                                                          {:max-page-count material-max-page-count}
                                                                          {})]
                                                      (sorted-map name material-info))))
  (output build-errors g/Any (g/fnk [_node-id name name-counts material]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")
                                                 (prop-resource-error _node-id :material material "Material")))))

(g/defnode LayerNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :layer)))
  (property child-index g/Int (dynamic visible (g/constantly false)) (default 0)) ; No protobuf counterpart.
  (input name-counts NameCounts)
  (output node-id+child-index NodeIndex (g/fnk [_node-id child-index] [_node-id child-index]))
  (output name+child-index NameIndex (g/fnk [name child-index] [name child-index]))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name child-index build-errors]
                                                          {:node-id _node-id
                                                           :node-outline-key name
                                                           :label name
                                                           :icon layer-icon
                                                           :child-index child-index
                                                           :outline-error? (g/error-fatal? build-errors)}))
  (output pb-msg g/Any (g/fnk [name child-index]
                         (-> (protobuf/make-map-without-defaults Gui$SceneDesc$LayerDesc :name name)
                             (assoc :child-index child-index)))) ; Used to order layers in the SceneDesc.
  (output build-errors g/Any (g/fnk [_node-id name name-counts]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")))))

(g/defnode ResourceNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :path)))
  (property path resource/Resource ; Required protobuf field.
            (value (gu/passthrough resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter
                    evaluation-context self old-value new-value
                    [:resource :resource]
                    [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id path]
                                  (prop-resource-error _node-id :path path "Path")))
            (dynamic edit-type (g/constantly
                                {:type resource/Resource})))

  (input resource resource/Resource)
  (output resource-path g/Str (g/fnk [resource] (resource/resource->proj-path resource)))

  (input name-counts NameCounts)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name resource build-errors]
                                                          (cond-> {:node-id _node-id
                                                                   :node-outline-key name
                                                                   :label name
                                                                   :icon layer-icon
                                                                   :outline-error? (g/error-fatal? build-errors)}
                                                            (resource/openable-resource? resource) (assoc :link resource :outline-show-link? true))))

  (output resource-names GuiResourceNames (g/fnk [name] (sorted-set name)))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))

  (output pb-msg g/Any (g/fnk [name resource-path]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$ResourceDesc
                           :name name
                           :path resource-path)))
  (output build-errors g/Any (g/fnk [_node-id name name-counts path]
                                    (g/package-errors _node-id
                                                      (prop-unique-id-error _node-id :name name name-counts "Name")
                                                      (prop-resource-error _node-id :path path "Path")))))

(g/defnode ParticleFXResource
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name")))
            (set (partial update-gui-resource-references :particlefx)))
  (property particlefx resource/Resource ; Required protobuf field.
            (value (gu/passthrough particlefx-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter
                     evaluation-context self old-value new-value
                     [:resource :particlefx-resource]
                     [:build-targets :dep-build-targets]
                     [:scene :particlefx-scene])))
            (dynamic error (g/fnk [_node-id particlefx]
                                  (prop-resource-error _node-id :particlefx particlefx "Particle FX")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext [particlefx/particlefx-ext]})))

  (input name-counts NameCounts)
  (input particlefx-resource resource/Resource)
  (input particlefx-scene g/Any :substitute (g/constantly nil))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name particlefx-resource build-errors]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key name
                                                              :label name
                                                              :icon particlefx/particle-fx-icon
                                                              :outline-error? (g/error-fatal? build-errors)}
                                                             (resource/openable-resource? particlefx-resource) (assoc :link particlefx-resource :outline-show-link? true))))
  (output pb-msg g/Any (g/fnk [name particlefx]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$ParticleFXDesc
                           :name name
                           :particlefx (resource/resource->proj-path particlefx))))
  (output particlefx-resource-names GuiResourceNames (g/fnk [name] (sorted-set name)))
  (output particlefx-infos ParticleFXInfos (g/fnk [name particlefx-scene]
                                             {name {:particlefx-scene particlefx-scene}}))
  (output build-errors g/Any (g/fnk [_node-id name name-counts particlefx]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")
                                                 (prop-resource-error _node-id :particlefx particlefx "Particle FX")))))

(defn- layout-pb-msg [name node-msgs]
  (let [node-msgs (filterv (comp not-empty :overridden-fields) node-msgs)]
    (protobuf/make-map-without-defaults Gui$SceneDesc$LayoutDesc
      :name name
      :nodes node-msgs)))

(def ^:private layout-node-traverse-fn
  (g/make-override-traverse-fn
    (fn layout-node-traverse-fn [basis ^Arc arc]
      (g/node-instance-match basis (.source-id arc)
                             [GuiNode
                              NodeTree
                              GuiSceneNode]))))

(g/defnode LayoutNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic read-only? (g/constantly true))
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name"))))
  (property nodes g/Any ; No protobuf counterpart.
            (dynamic visible (g/constantly false))
            (value (gu/passthrough layout-overrides))
            (set (fn [evaluation-context self _ new-value]
                   (let [basis (:basis evaluation-context)
                         scene (ffirst (g/targets-of basis self :_node-id))
                         node-tree (g/node-value scene :node-tree evaluation-context)
                         or-data new-value]
                     (g/override node-tree {:traverse-fn layout-node-traverse-fn}
                                 (fn [evaluation-context id-mapping]
                                   (let [or-node-tree (get id-mapping node-tree)
                                         node-ids-by-id (g/node-value node-tree :node-ids evaluation-context)
                                         node-mapping (when-not (g/error-value? node-ids-by-id)
                                                        (comp id-mapping node-ids-by-id))]
                                     (concat
                                       (for [[from to] [[:node-overrides :layout-overrides]
                                                        [:node-msgs :node-msgs]
                                                        [:node-outline :node-tree-node-outline]
                                                        [:scene :node-tree-scene]]]
                                         (g/connect or-node-tree from self to))
                                       (for [[from to] [[:id-prefix :id-prefix]]]
                                         (g/connect self from or-node-tree to))
                                       (when node-mapping
                                         (for [[id data] or-data
                                               :let [node-id (node-mapping id)]
                                               :when node-id
                                               [label value] data]
                                           (g/set-property node-id label value)))))))))))
  (input name-counts NameCounts)
  (input layout-overrides g/Any :cascade-delete)
  (input node-msgs g/Any)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name build-errors]
                                                          {:node-id _node-id
                                                           :node-outline-key name
                                                           :label name
                                                           :icon layout-icon
                                                           :outline-error? (g/error-fatal? build-errors)}))
  (output pb-msg g/Any :cached (g/fnk [name node-msgs] (layout-pb-msg name node-msgs)))
  (input node-tree-node-outline g/Any)
  (output layout-node-outline g/Any (g/fnk [name node-tree-node-outline] [name node-tree-node-outline]))
  (input node-tree-scene g/Any)
  (output layout-scene g/Any (g/fnk [name node-tree-scene] [name node-tree-scene]))
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))
  (output build-errors g/Any (g/fnk [_node-id name name-counts]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")))))

(defmacro gen-outline-fnk [label node-outline-key order sort-children? child-reqs]
  `(g/fnk [~'_node-id ~'child-outlines]
          {:node-id ~'_node-id
           :node-outline-key ~node-outline-key
           :label ~label
           :icon ~virtual-icon
           :order ~order
           :read-only true
           :child-reqs ~child-reqs
           :children ~(if sort-children?
                       `(vec (sort-by :child-index ~'child-outlines))
                       'child-outlines)}))

(g/defnode NodeTree
  (inherits core/Scope)
  (inherits outline/OutlineNode)

  (property id g/Str (default "") ; No protobuf counterpart.
            (dynamic visible (g/constantly false)))

  (input child-scenes g/Any :array)
  (input child-indices NodeIndex :array)
  (output child-scenes g/Any (g/fnk [child-scenes] (vec (sort-by (comp :child-index :renderable) child-scenes))))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Nodes" nil 0 true (mapv (fn [type-info] {:node-type (:node-cls type-info)
                                                                     :tx-attach-fn (gen-gui-node-attach-fn (:node-type type-info))})
                                                    (get-registered-node-type-infos))))

  (output scene g/Any (g/fnk [_node-id child-scenes]
                        {:node-id _node-id
                         :aabb geom/null-aabb
                         :children child-scenes}))
  (input ids g/Str :array)
  (output id-counts NameCounts :cached (g/fnk [ids] (frequencies ids)))
  (input node-msgs g/Any :array)
  (output node-msgs g/Any :cached (g/fnk [node-msgs]
                                         (->> node-msgs
                                              (sort-by #(get-in % [0 :child-index]))
                                              flatten
                                              (mapv #(dissoc % :child-index)))))
  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides] (into {} node-overrides)))
  (input node-ids IDMap :array)
  (output node-ids IDMap :cached (g/fnk [node-ids] (reduce merge {} node-ids)))
  (input texture-gpu-textures GuiResourceTextures)
  (output texture-gpu-textures GuiResourceTextures (gu/passthrough texture-gpu-textures))
  (input texture-infos GuiResourceTextureInfos)
  (output texture-infos GuiResourceTextureInfos (gu/passthrough texture-infos))
  (input material-infos GuiResourceMaterialInfos)
  (output material-infos GuiResourceMaterialInfos (gu/passthrough material-infos))

  (input material-shaders GuiResourceShaders)
  (output material-shaders GuiResourceShaders (gu/passthrough material-shaders))

  (input font-shaders GuiResourceShaders)
  (output font-shaders GuiResourceShaders (gu/passthrough font-shaders))

  (input font-datas FontDatas)
  (output font-datas FontDatas (gu/passthrough font-datas))
  (input font-names GuiResourceNames)
  (output font-names GuiResourceNames (gu/passthrough font-names))
  (input layer-names GuiResourceNames)
  (output layer-names GuiResourceNames (gu/passthrough layer-names))
  (input layer->index g/Any)
  (output layer->index g/Any (gu/passthrough layer->index))
  (input spine-scene-element-ids SpineSceneElementIds)
  (output spine-scene-element-ids SpineSceneElementIds (gu/passthrough spine-scene-element-ids))
  (input spine-scene-infos SpineSceneInfos)
  (output spine-scene-infos SpineSceneInfos (gu/passthrough spine-scene-infos))
  (input spine-scene-names GuiResourceNames)
  (output spine-scene-names GuiResourceNames (gu/passthrough spine-scene-names))
  (input particlefx-infos ParticleFXInfos)
  (output particlefx-infos ParticleFXInfos (gu/passthrough particlefx-infos))
  (input particlefx-resource-names GuiResourceNames)
  (output particlefx-resource-names GuiResourceNames (gu/passthrough particlefx-resource-names))
  (input resource-names GuiResourceNames)
  (output resource-names GuiResourceNames (gu/passthrough resource-names))
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))
  (input current-layout g/Str)
  (output current-layout g/Str (gu/passthrough current-layout))
  (input child-build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough child-build-errors))
  (input template-build-targets g/Any :array)
  (output template-build-targets g/Any (gu/passthrough template-build-targets)))


(defn- browse [title project exts]
  (seq (resource-dialog/make (project/workspace project) project {:ext exts :title title :selection :multiple})))

(defn- resource->id [resource]
  (resource/base-name resource))

;; SDK api
(defn query-and-add-resources! [resources-type-label resource-exts taken-ids project select-fn make-node-fn]
  (when-let [resources (browse (str "Select " resources-type-label) project resource-exts)]
    (let [names (outline/resolve-ids (map resource->id resources) taken-ids)
          pairs (map vector resources names)
          op-seq (gensym)
          op-label (str "Add " resources-type-label)
          new-nodes (g/tx-nodes-added
                     (g/transact
                      (concat
                       (g/operation-sequence op-seq)
                       (g/operation-label op-label)
                       (for [[resource name] pairs]
                         (make-node-fn resource name)))))]
      (when (some? select-fn)
        (g/transact
         (concat
          (g/operation-sequence op-seq)
          (g/operation-label op-label)
          (select-fn new-nodes)))))))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-texture
  ([self textures-node texture]
   (attach-texture self textures-node texture false))
  ([self textures-node texture internal?]
   (concat
    (g/connect texture :_node-id self :nodes)
    (g/connect texture :texture-gpu-textures self :texture-gpu-textures)
    (g/connect texture :texture-infos self :texture-infos)
    (when (not internal?)
      (concat
       (g/connect texture :dep-build-targets self :dep-build-targets)
       (g/connect texture :pb-msg self :texture-msgs)
       (g/connect texture :build-errors textures-node :build-errors)
       (g/connect texture :node-outline textures-node :child-outlines)
       (g/connect texture :name textures-node :names)
       (g/connect textures-node :name-counts texture :name-counts)
       (g/connect self :samplers texture :samplers)
       (g/connect self :default-tex-params texture :default-tex-params))))))

(defn add-texture [scene textures-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [TextureNode :name name :texture resource]]
                (attach-texture scene textures-node node)))

(defn- add-textures-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
   "Textures" (workspace/resource-kind-extensions (project/workspace project) :atlas) (g/node-value parent :name-counts) project select-fn
   (partial add-texture scene parent)))

(g/defnode TexturesNode
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Textures" "Textures" 1 false [{:node-type TextureNode
                                                           :tx-attach-fn (gen-outline-node-tx-attach-fn attach-texture)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Textures..." texture-icon add-textures-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-material
  ([self materials-node material]
   (attach-material self materials-node material false))
  ([self materials-node material internal?]
   (concat
     (g/connect material :_node-id self :nodes)
     (g/connect material :material-shaders self :material-shaders)
     (when (not internal?)
       (concat
         (g/connect material :material-infos self :material-infos)
         (g/connect material :dep-build-targets self :dep-build-targets)
         (g/connect material :pb-msg self :material-msgs)
         (g/connect material :build-errors materials-node :build-errors)
         (g/connect material :node-outline materials-node :child-outlines)
         (g/connect material :name materials-node :names)
         (g/connect materials-node :name-counts material :name-counts))))))

(defn add-material [scene materials-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [MaterialNode :name name :material resource]]
    (attach-material scene materials-node node)))

(defn- add-materials-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
    "Materials" ["material"] (g/node-value parent :name-counts) project select-fn
    (partial add-material scene parent)))

(g/defnode MaterialsNode
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Materials" "Materials" 1 false [{:node-type MaterialNode
                                                             :tx-attach-fn (gen-outline-node-tx-attach-fn attach-material)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
            [_node-id "Materials..." material-icon add-materials-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-font
  ([self fonts-node font]
   (attach-font self fonts-node font false))
  ([self fonts-node font internal?]
   (concat
    (g/connect font :_node-id self :nodes)
    (g/connect font :font-shaders self :font-shaders)
    (g/connect font :font-datas self :font-datas)
    (when (not internal?)
      (concat
       (g/connect font :font-names self :font-names)
       (g/connect font :dep-build-targets self :dep-build-targets)
       (g/connect font :pb-msg self :font-msgs)
       (g/connect font :build-errors fonts-node :build-errors)
       (g/connect font :node-outline fonts-node :child-outlines)
       (g/connect font :name fonts-node :names)
       (g/connect fonts-node :name-counts font :name-counts))))))

(defn add-font [scene fonts-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [FontNode :name name :font resource]]
                (attach-font scene fonts-node node)))

(defn- add-fonts-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
   "Fonts" ["font"] (g/node-value parent :name-counts) project select-fn
   (partial add-font scene parent)))

(g/defnode FontsNode
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Fonts" "Fonts" 2 false [{:node-type FontNode
                                                     :tx-attach-fn (gen-outline-node-tx-attach-fn attach-font)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Fonts..." font-icon add-fonts-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-layer
  ([self layers-node layer]
   (attach-layer self layers-node layer false))
  ;; Self is not used here but added to conform all attach-*** functions
  ([_self layers-node layer internal?]
   (concat
    (g/connect layer :_node-id layers-node :nodes)
    (when (not internal?)
      (concat
       (g/connect layer :pb-msg layers-node :layer-msgs)
       (g/connect layer :build-errors layers-node :build-errors)
       (g/connect layer :node-outline layers-node :child-outlines)
       (g/connect layer :node-id+child-index layers-node :child-indices)
       (g/connect layer :name+child-index layers-node :indexed-layer-names)
       (g/connect layers-node :name-counts layer :name-counts))))))

(defn add-layer [project scene parent name child-index select-fn]
  (g/make-nodes (g/node-id->graph-id scene) [node [LayerNode :name name :child-index child-index]]
    ;; Self is not used when attaching layers
    (attach-layer nil parent node)
    (when select-fn
      (select-fn [node]))))

(defn- add-layer-handler [project {:keys [scene parent]} select-fn]
  (let [name (outline/resolve-id "layer" (g/node-value parent :name-counts))
        child-indices (g/node-value parent :child-indices)
        next-index (next-child-index child-indices)]
    (g/transact
     (concat
      (g/operation-label "Add Layer")
      (add-layer project scene parent name next-index select-fn)))))

(g/defnode LayersNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (output name-counts NameCounts :cached (g/fnk [ordered-layer-names] (frequencies ordered-layer-names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))

  (input indexed-layer-names NameIndex :array)

  (output ordered-layer-names g/Any (g/fnk [indexed-layer-names] (map first (sort-by second indexed-layer-names))))
  (output layer-names GuiResourceNames :cached (g/fnk [ordered-layer-names] (into (sorted-set) ordered-layer-names)))

  (input layer-msgs g/Any :array)
  (output layer-msgs g/Any :cached (g/fnk [layer-msgs] (flatten layer-msgs)))

  (output layer->index g/Any :cached (g/fnk [ordered-layer-names]
                                       (zipmap ordered-layer-names (range))))
  (input child-indices NodeIndex :array)
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Layers" "Layers" 3 true [{:node-type LayerNode
                                                      :tx-attach-fn (gen-outline-node-tx-attach-fn attach-layer :ordered-layer-names)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Layer" layer-icon add-layer-handler {}])))


;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-layout [self layouts-node layout]
  (concat
   (g/connect layout :build-errors layouts-node :build-errors)
   (g/connect layout :node-outline layouts-node :child-outlines)
   (g/connect layout :name layouts-node :names)
   (g/connect layouts-node :name-counts layout :name-counts)
   (for [[from to] [[:_node-id :nodes]
                    [:name :layout-names]
                    [:pb-msg :layout-msgs]
                    [:layout-node-outline :layout-node-outlines]
                    [:layout-scene :layout-scenes]]]
     (g/connect layout from self to))
   (for [[from to] [[:id-prefix :id-prefix]]]
     (g/connect self from layout to))))

(defn add-layout-handler [project {:keys [scene parent display-profile]} select-fn]
  (g/transact
   (concat
    (g/operation-label "Add Layout")
    (g/make-nodes (g/node-id->graph-id scene) [node [LayoutNode :name display-profile]]
                  (attach-layout scene parent node)
                  (g/set-property node :nodes {})
                  (when select-fn
                    (select-fn [node]))))))

(g/defnode LayoutsNode
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (input unused-display-profiles g/Any)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          ;; Layouts don't have any child-reqs for the outline copy/pasting,
          ;; since there is essentially only one node that _can_ be supported
          ;; per "layout type".
          (gen-outline-fnk "Layouts" "Layouts" 4 false []))
  (output add-handler-info g/Any
          (g/fnk [_node-id unused-display-profiles]
            (mapv #(vector _node-id % layout-icon add-layout-handler {:display-profile %})
                  unused-display-profiles))))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-particlefx-resource
  ([self particlefx-resources-node particlefx-resource]
   (attach-particlefx-resource self particlefx-resources-node particlefx-resource false))
  ([self particlefx-resources-node particlefx-resource internal?]
   (concat
    (g/connect particlefx-resource :_node-id self :nodes)
    (g/connect particlefx-resource :particlefx-infos self :particlefx-infos)
    (when (not internal?)
      (concat
       (g/connect particlefx-resource :particlefx-resource-names self :particlefx-resource-names)
       (g/connect particlefx-resource :dep-build-targets         self :dep-build-targets)
       (g/connect particlefx-resource :pb-msg                    self :particlefx-resource-msgs)
       (g/connect particlefx-resource :build-errors particlefx-resources-node :build-errors)
       (g/connect particlefx-resource :node-outline particlefx-resources-node :child-outlines)
       (g/connect particlefx-resource :name         particlefx-resources-node :names)
       (g/connect particlefx-resources-node :name-counts particlefx-resource :name-counts))))))

(defn add-particlefx-resource [scene particlefx-resources-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [ParticleFXResource :name name :particlefx resource]]
                (attach-particlefx-resource scene particlefx-resources-node node)))

(defn- add-particlefx-resources-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
   "Particle FX" [particlefx/particlefx-ext] (g/node-value parent :name-counts) project select-fn
   (partial add-particlefx-resource scene parent)))

(g/defnode ParticleFXResources
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Particle FX" "Particle FX" 5 false [{:node-type ParticleFXResource
                                                                 :tx-attach-fn (gen-outline-node-tx-attach-fn attach-particlefx-resource)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Particle FX..." particlefx/particle-fx-icon add-particlefx-resources-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

(defn- sort-children [node-order scene]
  (let [key (clipping/scene-key scene)
        index (node-order key)]
    (cond-> scene
      index (assoc-in [:renderable :index] index)
      true (update :children (partial mapv (partial sort-children node-order))))))

(defn- sort-scene [scene]
  (if (g/error? scene)
    scene
    (let [node-order (->> scene
                       clipping/scene->render-keys
                       (sort-by second) ; sort by render-key
                       (map first) ; keep scene keys
                       (map-indexed (fn [index scene-key] [scene-key index]))
                       (into {}))]
      (sort-children node-order scene))))

(g/defnk produce-scene [_node-id scene-dims child-scenes]
  (let [w (:width scene-dims)
        h (:height scene-dims)
        scene {:node-id _node-id
               :aabb geom/null-aabb
               :visibility-aabb (geom/coords->aabb [0 0 0] [w h 0])
               :renderable {:render-fn render-lines
                            :tags #{:gui :gui-bounds}
                            :passes [pass/transparent]
                            :batch-key nil
                            :user-data {:line-data [[0 0 0] [w 0 0] [w 0 0] [w h 0] [w h 0] [0 h 0] [0 h 0] [0 0 0]]
                                        :line-color colors/defold-white}}
               :children (mapv (partial apply-alpha 1.0) child-scenes)}]
    (-> scene
      clipping/setup-states
      sort-scene)))

(g/defnk produce-pb-msg [script-resource material-resource adjust-reference max-nodes max-dynamic-textures node-msgs layer-msgs font-msgs texture-msgs material-msgs layout-msgs particlefx-resource-msgs resource-msgs]
  (protobuf/make-map-without-defaults Gui$SceneDesc
    :script (resource/resource->proj-path script-resource)
    :material (resource/resource->proj-path material-resource)
    :adjust-reference adjust-reference
    :max-nodes max-nodes
    :max-dynamic-textures max-dynamic-textures
    :nodes node-msgs
    :layers layer-msgs
    :fonts font-msgs
    :textures texture-msgs
    :materials material-msgs
    :layouts layout-msgs
    :particlefxs particlefx-resource-msgs
    :resources resource-msgs))

(g/defnk produce-save-value [pb-msg]
  (-> pb-msg
      (protobuf/sanitize-repeated
        :nodes (fn [node-desc]
                 (if (:template-node-child node-desc)
                   (strip-unused-overridden-fields-from-node-desc node-desc)
                   (strip-redundant-size-from-node-desc node-desc))))
      (protobuf/sanitize-repeated
        :layouts (fn [layout-desc]
                   (protobuf/sanitize-repeated layout-desc :nodes strip-unused-overridden-fields-from-node-desc)))))

(defn- build-pb [resource dep-resources user-data]
  (let [def (:def user-data)
        pb  (:pb user-data)
        pb  (if (:transform-fn def) ((:transform-fn def) pb) pb)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb (map (fn [[label res]]
                              [label (resource/resource->proj-path (get dep-resources res))])
                            (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(defn- merge-rt-pb-msg [rt-pb-msg template-build-targets]
  (let [merge-fn! (fn [coll msg kw] (reduce conj! coll (map #(do [(:name %) %]) (get msg kw))))
        [textures materials fonts particlefx-resources resources]
        (loop [textures (transient {})
               materials (transient {})
               fonts (transient {})
               particlefx-resources (transient {})
               resources (transient {})
               msgs (conj (mapv #(get-in % [:user-data :pb]) template-build-targets) rt-pb-msg)]
          (if-let [msg (first msgs)]
            (recur
              (merge-fn! textures msg :textures)
              (merge-fn! materials msg :materials)
              (merge-fn! fonts msg :fonts)
              (merge-fn! particlefx-resources msg :particlefxs)
              (merge-fn! resources msg :resources)
              (next msgs))
            [(persistent! textures) (persistent! materials) (persistent! fonts) (persistent! particlefx-resources) (persistent! resources)]))]
    (assoc rt-pb-msg :textures (mapv second textures) :materials (mapv second materials) :fonts (mapv second fonts) :particlefxs (mapv second particlefx-resources) :resources (mapv second resources))))

(defn- node-desc->pose
  ^Pose [node-desc]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  (pose/make (some-> (:position node-desc) pose/seq-translation)
             (some-> (:rotation node-desc) pose/seq-euler-rotation)
             (some-> (:scale node-desc) pose/seq-scale)))

(defn- pre-multiply-pb-pose [child-node-desc parent-node-desc]
  (let [pose (pose/pre-multiply (node-desc->pose child-node-desc)
                                (node-desc->pose parent-node-desc))]
    (assoc child-node-desc
      :position (pose/translation-v4 pose 1.0)
      :rotation (pose/euler-rotation-v4 pose)
      :scale (pose/scale-v4 pose))))

(defn- node-desc->rt-node-desc [node-desc]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  (cond
    (= :type-template (:type node-desc))
    nil

    (:template-node-child node-desc)
    (reduce
      (fn [node template]
        (cond-> (assoc node :template-node-child false)

                (and (coll/empty? (:layer node))
                     (not (coll/empty? (:layer template))))
                (assoc :layer (:layer template))

                (:inherit-alpha node)
                (->
                  (assoc :alpha (* (:alpha node 1.0) (:alpha template 1.0)))
                  (assoc :inherit-alpha (:inherit-alpha template false)))

                (or (= (:id template) (:parent node))
                    (coll/empty? (:parent node)))
                (->
                  (assoc :parent (:parent template ""))
                  (assoc :enabled (and (:enabled node true) (:enabled template true)))
                  ;; In fact incorrect, but only possibility to retain rotation/scale separation.
                  (pre-multiply-pb-pose template))))
      node-desc
      (:templates (meta node-desc)))

    :else
    node-desc))

(defn- layout-desc->rt-layout-desc [layout-desc]
  {:pre [(map? layout-desc)]} ; Gui$SceneDesc$LayoutDesc in map format.
  (-> layout-desc
      (protobuf/sanitize-repeated
        :nodes (comp #(dissoc % :overridden-fields)
                     node-desc->rt-node-desc))))

(defn- scene-desc->rt-scene-desc [scene-desc]
  {:pre [(map? scene-desc)]} ; Gui$SceneDesc in map format.
  (-> scene-desc
      (protobuf/sanitize-repeated :nodes node-desc->rt-node-desc)
      (protobuf/sanitize-repeated :layouts layout-desc->rt-layout-desc)))

(g/defnk produce-build-targets [_node-id build-errors resource pb-msg dep-build-targets template-build-targets]
  (g/precluding-errors build-errors
    (let [def pb-def
          template-build-targets (flatten template-build-targets)
          rt-pb-msg (scene-desc->rt-scene-desc pb-msg)
          rt-pb-msg (merge-rt-pb-msg rt-pb-msg template-build-targets)
          dep-build-targets (concat (flatten dep-build-targets) (mapcat :deps (flatten template-build-targets)))
          deps-by-source (into {} (map #(let [res (:resource %)] [(resource/resource->proj-path (:resource res)) res]) dep-build-targets))
          resource-fields (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get rt-pb-msg (first field))))) [field])) (:resource-fields def))
          dep-resources (mapv (fn [label] [label (get deps-by-source (if (vector? label) (get-in rt-pb-msg label) (get rt-pb-msg label)))]) resource-fields)]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-pb
          :user-data {:pb rt-pb-msg
                      :pb-class (:pb-class def)
                      :def def
                      :dep-resources dep-resources}
          :deps dep-build-targets})])))

(defn- validate-template-build-targets [template-build-targets]
  (let [gui-resource-type+name->value->resource-proj-paths
        (persistent!
          (reduce
            (fn [acc [gui-resource-type gui-resource-name gui-resource-value template-resource]]
              (let [k (pair gui-resource-type gui-resource-name)
                    proj-path (resource/proj-path template-resource)]
                (assoc! acc k (-> (acc k)
                                  (or {})
                                  (update gui-resource-value (fnil conj []) proj-path)))))
            (transient {})
            (for [template-build-target (flatten template-build-targets)
                  :let [resource (-> template-build-target :resource :resource)
                        pb (-> template-build-target :user-data :pb)]
                  [gui-resource-pb-key gui-value-key] [[:textures :texture]
                                                       [:fonts :font]
                                                       [:materials :material]
                                                       [:particlefxs :particlefx]
                                                       [:resources :path]]
                  gui-resource (get pb gui-resource-pb-key)]
              [gui-resource-pb-key (:name gui-resource) (get gui-resource gui-value-key) resource])))
        errors (into []
                     (comp
                       (filter #(< 1 (-> % val count)))
                       (map (fn [[[gui-resource-type gui-resource-name] value->proj-paths]]
                              (format "%s \"%s\" has conflicting values in templates: %s"
                                      (case gui-resource-type
                                        :textures "Texture"
                                        :materials "Material"
                                        :fonts "Font"
                                        :particlefxs "Particle FX"
                                        :resources "Custom resource")
                                      gui-resource-name
                                      (->> (for [[value proj-paths] value->proj-paths
                                                 proj-path proj-paths]
                                             (format "%s (%s)" proj-path value))
                                           (sort eutil/natural-order)
                                           (eutil/join-words ", " " and "))))))
                     (sort-by key
                              (eutil/comparator-chain
                                (eutil/comparator-on key)
                                (eutil/comparator-on eutil/natural-order val))
                              gui-resource-type+name->value->resource-proj-paths))]
    (when (pos? (count errors))
      (str/join "\n" errors))))

(defn- validate-max-nodes [_node-id max-nodes node-ids]
    (or (validation/prop-error :fatal _node-id :max-nodes (partial validation/prop-outside-range? [1 8192]) max-nodes "Max Nodes")
        (validation/prop-error :fatal _node-id :max-nodes (fn [v] (let [c (count node-ids)]
                                                                    (when (> c max-nodes)
                                                                      (format "the actual number of nodes (%d) exceeds 'Max Nodes' (%d)" c max-nodes)))) max-nodes)))

(defn- validate-max-dynamic-textures [_node-id max-dynamic-textures]
  (validation/prop-error :fatal _node-id :max-dynamic-textures (partial validation/prop-outside-range? [0 8192]) max-dynamic-textures "Max Dynamic Textures"))

(g/defnk produce-own-build-errors [_node-id material max-dynamic-textures max-nodes ^:try node-ids script]
  (g/package-errors _node-id
                    (when script (prop-resource-error _node-id :script script "Script" "gui_script"))
                    (prop-resource-error _node-id :material material "Material")
                    (when-not (g/error-value? node-ids)
                      (validate-max-nodes _node-id max-nodes node-ids))
                    (validate-max-dynamic-textures _node-id max-dynamic-textures)))

(g/defnk produce-build-errors [_node-id build-errors own-build-errors ^:try template-build-targets]
  (g/package-errors _node-id
                    build-errors
                    own-build-errors
                    (validation/prop-error :fatal _node-id nil validate-template-build-targets (gu/array-subst-remove-errors template-build-targets))))

(defn- get-ids [outline]
  (map :label (tree-seq fn/constantly-true :children outline)))

(defn- one-or-many-handler-infos-to-vec [one-or-many-handler-infos]
  (if (g/node-id? (first one-or-many-handler-infos))
    [one-or-many-handler-infos]
    one-or-many-handler-infos))

(g/defnode GuiSceneNode
  (inherits resource-node/ResourceNode)

  (property script resource/Resource ; Required protobuf field.
            (value (gu/passthrough script-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter
                     evaluation-context self old-value new-value
                     [:resource :script-resource]
                     [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id script]
                             (when script
                               (prop-resource-error _node-id :script script "Script" "gui_script"))))
            (dynamic edit-type (g/fnk [] {:type resource/Resource
                                          :ext "gui_script"})))

  (property material resource/Resource ; Default assigned in load-fn.
    (value (gu/passthrough material-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter
             evaluation-context self old-value new-value
             [:resource :material-resource]
             [:shader :material-shader]
             [:max-page-count :material-max-page-count]
             [:samplers :samplers]
             [:build-targets :dep-build-targets])))
    (dynamic error (g/fnk [_node-id material]
                     (prop-resource-error _node-id :material material "Material")))
    (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]})))

  (property adjust-reference g/Keyword (default (protobuf/default Gui$SceneDesc :adjust-reference))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property visible-layout g/Str (default "") ; No protobuf counterpart.
            (dynamic visible (g/constantly false)))
  (property current-nodes g/Int
            (value (g/fnk [node-ids] (count node-ids)))
            (dynamic read-only? (g/constantly true)))
  (property max-nodes g/Int (default (protobuf/default Gui$SceneDesc :max-nodes))
            (dynamic error (g/fnk [_node-id max-nodes ^:try node-ids]
                             (when-not (g/error-value? node-ids)
                               (validate-max-nodes _node-id max-nodes node-ids)))))
  (property max-dynamic-textures g/Int (default (protobuf/default Gui$SceneDesc :max-dynamic-textures))
            (dynamic error (g/fnk [_node-id max-dynamic-textures]
                             (validate-max-dynamic-textures _node-id max-dynamic-textures))))

  (input script-resource resource/Resource)

  (input node-tree g/NodeID)
  (input layers-node g/NodeID) ; for tests
  (input layouts-node g/NodeID) ; for tests
  (input fonts-node g/NodeID) ; for tests
  (input textures-node g/NodeID) ; for tests
  (input particlefx-resources-node g/NodeID) ; for tests
  (input handler-infos g/Any :array)
  (output handler-infos g/Any (g/fnk [handler-infos]
                                (into [] (mapcat one-or-many-handler-infos-to-vec) handler-infos)))
  (input dep-build-targets g/Any :array)
  (input project-settings g/Any)
  (input default-tex-params g/Any)
  (output default-tex-params g/Any (gu/passthrough default-tex-params))
  (input display-profiles g/Any)
  (input current-layout g/Str)
  (output current-layout g/Str (g/fnk [current-layout visible-layout] (or current-layout visible-layout)))
  (input node-msgs g/Any)
  (output node-msgs g/Any (gu/passthrough node-msgs))
  (input node-overrides g/Any)
  (output node-overrides g/Any :cached (gu/passthrough node-overrides))
  (input font-msgs g/Any :array)
  (input texture-msgs g/Any :array)
  (input material-msgs g/Any :array)
  (input layer-msgs g/Any)
  (output layer-msgs g/Any (g/fnk [layer-msgs] (mapv #(dissoc % :child-index) (sort-by :child-index layer-msgs))))
  (input layout-msgs g/Any :array)
  (input particlefx-resource-msgs g/Any :array)
  (input resource-msgs g/Any :array)
  (input node-ids IDMap)
  (output node-ids IDMap (gu/passthrough node-ids))
  (input layout-names g/Str :array)

  (input aux-texture-gpu-textures GuiResourceTextures :array)
  (input texture-gpu-textures GuiResourceTextures :array)
  (output texture-gpu-textures GuiResourceTextures :cached (g/fnk [aux-texture-gpu-textures texture-gpu-textures] (into {} (concat aux-texture-gpu-textures texture-gpu-textures))))

  (input aux-texture-infos GuiResourceTextureInfos :array)
  (input texture-infos GuiResourceTextureInfos :array)
  (output texture-infos GuiResourceTextureInfos :cached (g/fnk [aux-texture-infos texture-infos] (into (sorted-map) (concat aux-texture-infos texture-infos))))

  (input aux-material-shaders GuiResourceShaders :array)
  (input material-shaders GuiResourceShaders :array)
  (output material-shaders GuiResourceShaders :cached (g/fnk [aux-material-shaders material-shaders material-shader]
                                                        (into {"" material-shader}
                                                              (concat aux-material-shaders material-shaders))))

  (input aux-material-infos GuiResourceMaterialInfos :array)
  (input material-infos GuiResourceMaterialInfos :array)
  (output material-infos GuiResourceMaterialInfos :cached (g/fnk [aux-material-infos material-infos material-max-page-count]
                                                            (into (sorted-map "" {:max-page-count material-max-page-count})
                                                                  (concat aux-material-infos material-infos))))

  (input aux-font-shaders GuiResourceShaders :array)
  (input font-shaders GuiResourceShaders :array)
  (output font-shaders GuiResourceShaders :cached (g/fnk [aux-font-shaders font-shaders] (into {} (concat aux-font-shaders font-shaders))))
  (input aux-font-datas FontDatas :array)
  (input font-datas FontDatas :array)
  (output font-datas FontDatas :cached (g/fnk [aux-font-datas font-datas] (into {} (concat aux-font-datas font-datas))))
  (input aux-font-names GuiResourceNames :array)
  (input font-names GuiResourceNames :array)
  (output font-names GuiResourceNames :cached (g/fnk [aux-font-names font-names] (into (sorted-set) cat (concat aux-font-names font-names))))

  (input layer-names GuiResourceNames :array)
  (output layer-names GuiResourceNames :cached (g/fnk [layer-names] (into (sorted-set) cat layer-names)))
  (input layer->index g/Any)
  (output layer->index g/Any (gu/passthrough layer->index))

  (input aux-spine-scene-element-ids SpineSceneElementIds :array)
  (input spine-scene-element-ids SpineSceneElementIds :array)
  (output spine-scene-element-ids SpineSceneElementIds :cached (g/fnk [aux-spine-scene-element-ids spine-scene-element-ids] (into {} (concat aux-spine-scene-element-ids spine-scene-element-ids))))
  (input aux-spine-scene-infos SpineSceneInfos :array)
  (input spine-scene-infos SpineSceneInfos :array)
  (output spine-scene-infos SpineSceneInfos :cached (g/fnk [aux-spine-scene-infos spine-scene-infos] (into {} (concat aux-spine-scene-infos spine-scene-infos))))
  (input aux-spine-scene-names GuiResourceNames :array)
  (input spine-scene-names GuiResourceNames :array)
  (output spine-scene-names GuiResourceNames :cached (g/fnk [aux-spine-scene-names spine-scene-names] (into (sorted-set) cat (concat aux-spine-scene-names spine-scene-names))))

  (input aux-particlefx-infos ParticleFXInfos :array)
  (input particlefx-infos ParticleFXInfos :array)
  (output particlefx-infos ParticleFXInfos :cached (g/fnk [aux-particlefx-infos particlefx-infos] (into {} (concat aux-particlefx-infos particlefx-infos))))
  (input aux-particlefx-resource-names GuiResourceNames :array)
  (input particlefx-resource-names GuiResourceNames :array)
  (output particlefx-resource-names GuiResourceNames :cached (g/fnk [aux-particlefx-resource-names particlefx-resource-names] (into (sorted-set) cat (concat aux-particlefx-resource-names particlefx-resource-names))))

  (input aux-resource-names GuiResourceNames :array)
  (input resource-names GuiResourceNames :array)
  (output resource-names GuiResourceNames :cached (g/fnk [aux-resource-names resource-names] (into (sorted-set) cat (concat aux-resource-names resource-names))))

  (input material-max-page-count g/Int :substitute nil)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (input samplers [g/KeywordMap])
  (output samplers [g/KeywordMap] (gu/passthrough samplers))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any :cached produce-save-value)

  (input template-build-targets g/Any :array)
  (output own-build-errors g/Any produce-own-build-errors)
  (input build-errors g/Any :array)
  (output build-errors g/Any produce-build-errors)
  (output build-targets g/Any :cached produce-build-targets)
  (input layout-node-outlines g/Any :array)
  (output layout-node-outlines g/Any :cached (g/fnk [layout-node-outlines] (into {} layout-node-outlines)))
  (input default-node-outline g/Any)
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id default-node-outline layout-node-outlines current-layout child-outlines own-build-errors]
                 (let [node-outline (get layout-node-outlines current-layout default-node-outline)
                       label (:label pb-def)
                       icon (:icon pb-def)]
                   {:node-id _node-id
                    :node-outline-key label
                    :label label
                    :icon icon
                    :children (vec (sort-by :order (conj child-outlines node-outline)))
                    :outline-error? (g/error-fatal? own-build-errors)})))
  (input default-scene g/Any)
  (input layout-scenes g/Any :array)
  (output layout-scenes g/Any :cached (g/fnk [layout-scenes] (into {} layout-scenes)))
  (output child-scenes g/Any (g/fnk [default-scene layout-scenes current-layout]
                               (let [node-tree-scene (get layout-scenes current-layout default-scene)]
                                 (:children node-tree-scene))))
  (output scene g/Any :cached produce-scene)
  (output scene-dims g/Any :cached (g/fnk [project-settings current-layout display-profiles]
                                          (or (some #(and (= current-layout (:name %)) (first (:qualifiers %))) display-profiles)
                                              (let [w (get project-settings ["display" "width"])
                                                    h (get project-settings ["display" "height"])]
                                                {:width w :height h}))))
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))
  (output unused-display-profiles g/Any (g/fnk [layout-msgs display-profiles]
                                          (let [layouts (into #{} (map :name) layout-msgs)]
                                            (into []
                                                  (comp
                                                    (map :name)
                                                    (remove layouts))
                                                  display-profiles)))))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

(defn- attach-resource
  ([self resources-node resource-node]
   (attach-resource self resources-node resource-node false))
  ([self resources-node resource-node internal?]
   (concat
    (g/connect resource-node :_node-id self :nodes)
    (when (not internal?)
      (concat
       (g/connect resource-node :dep-build-targets self :dep-build-targets)
       (g/connect resource-node :resource-names    self :resource-names)
       (g/connect resource-node :pb-msg            self :resource-msgs)
       (g/connect resource-node :build-errors      resources-node :build-errors)
       (g/connect resource-node :node-outline      resources-node :child-outlines)
       (g/connect resource-node :name              resources-node :names)
       (g/connect resource-node :resource-path     resources-node :paths)
       (g/connect resources-node :name-counts resource-node :name-counts))))))

(defn add-gui-node! [project scene parent node-type custom-type select-fn]
  ;; TODO: The project argument is unused. Remove.
  (let [node-tree (g/node-value scene :node-tree)
        taken-ids (g/node-value node-tree :id-counts)
        id (outline/resolve-id (subs (name node-type) 5) taken-ids)
        node-type-info (get-registered-node-type-info node-type custom-type)
        def-node-type (:node-cls node-type-info)
        child-indices (g/node-value parent :child-indices)
        next-index (next-child-index child-indices)
        node-properties (assoc (:defaults node-type-info)
                          :id id
                          :child-index next-index
                          :custom-type custom-type
                          :type node-type)]
    (-> (concat
          (g/operation-label "Add Gui Node")
          (g/make-nodes (g/node-id->graph-id scene) [gui-node [def-node-type node-properties]]
            (attach-gui-node node-tree parent gui-node node-type)
            (when select-fn
              (select-fn [gui-node]))))
        g/transact
        g/tx-nodes-added
        first)))

(defn add-gui-node-handler [project {:keys [scene parent node-type custom-type]} select-fn]
  (add-gui-node! project scene parent node-type custom-type select-fn))

(defn- make-add-handler [scene parent label icon handler-fn user-data]
  {:label label :icon icon :command :add
   :user-data (merge {:handler-fn handler-fn :scene scene :parent parent} user-data)})

(defn- add-handler-options [node]
  ;; TODO: We should probably use an evaluation-context here.
  (let [type-infos (get-registered-node-type-infos)
        node (g/override-root node)
        scene (node->gui-scene node)
        node-options (cond
                       (g/node-instance? TemplateNode node)
                       (if-some [template-scene (g/override-root (g/node-feeding-into node :template-resource))]
                         (let [parent (g/node-value template-scene :node-tree)]
                           (mapv (fn [info]
                                   (if-not (:deprecated info)
                                     (make-add-handler template-scene parent (:display-name info) (:icon info)
                                                       add-gui-node-handler (into {} info))))
                                 type-infos))
                         [])

                       (g/node-instance-match node [GuiSceneNode GuiNode NodeTree])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :node-tree)
                                      node)]
                         (mapv (fn [info]
                                 (if-not (:deprecated info)
                                   (make-add-handler scene parent (:display-name info) (:icon info)
                                                     add-gui-node-handler (into {} info))))
                               type-infos))

                       :else
                       [])
        handler-options (when (empty? node-options)
                          (when (g/has-output? (g/node-type* node) :add-handler-info)
                            (->> (g/node-value node :add-handler-info)
                                 one-or-many-handler-infos-to-vec
                                 (map (fn [[parent menu-label menu-icon add-fn opts]]
                                        (let [parent (if (= node scene) parent node)]
                                          (make-add-handler scene parent menu-label menu-icon add-fn opts)))))))]
    (filter some? (into node-options handler-options))))

(defn- add-layout-options [node user-data]
  (g/with-auto-evaluation-context evaluation-context
    (let [scene (node->gui-scene node)
          parent (if (= node scene)
                   (g/node-value scene :layouts-node evaluation-context)
                   node)]
      (mapv #(make-add-handler scene parent % layout-icon add-layout-handler {:display-profile %})
            (g/node-value scene :unused-display-profiles evaluation-context)))))

(handler/defhandler :add :workbench
  (active? [selection] (not-empty (some->> (handler/selection->node-id selection) add-handler-options)))
  (run [project user-data app-view] (when user-data ((:handler-fn user-data) project user-data (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
    (let [node-id (handler/selection->node-id selection)]
      (if (not user-data)
        (add-handler-options node-id)
        (when (:layout user-data)
          (add-layout-options node-id user-data))))))

(defn- node-desc->node-properties [node-desc]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  (persistent!
    (reduce (fn [node-properties [pb-field pb-value]]
              (if-some [[prop-key pb-value->prop-value] (pb-field-to-node-property-conversions pb-field)]
                (let [prop-value (pb-value->prop-value pb-value)]
                  (-> node-properties
                      (dissoc! pb-field)
                      (assoc! prop-key prop-value)))
                node-properties))
            (transient node-desc)
            node-desc)))

(defn- sort-node-descs
  [node-descs]
  (let [parent-id->children (group-by :parent (remove #(str/blank? (:id %)) node-descs))
        parent->children #(parent-id->children (:id %))
        root {:id nil}]
    (rest (tree-seq parent->children parent->children root))))

(defonce ^:private custom-gui-scene-loaders (atom (sorted-map)))

;; SDK api
(defn register-gui-scene-loader! [load-fn]
  (let [load-fn-sym (fn/declared-symbol load-fn)]
    (swap! custom-gui-scene-loaders assoc load-fn-sym load-fn)))

(defn- get-registered-gui-scene-loaders []
  (vals @custom-gui-scene-loaders))

(def ^:private non-overridable-properties #{:template :id :parent})

(def ^:private node-property-defaults
  (node-desc->node-properties (protobuf/default-value Gui$NodeDesc)))

(defn- extract-overrides [node-properties]
  (into {}
        (comp (map pb-field-index->prop-key)
              (remove non-overridable-properties)
              (map (fn [prop-key]
                     (let [prop-value (node-properties prop-key ::not-found)]
                       (pair prop-key
                             (if (= ::not-found prop-value)
                               (node-property-defaults prop-key)
                               prop-value))))))
        (:overridden-fields node-properties)))

(def ^:private default-material-proj-path (protobuf/default Gui$SceneDesc :material))

(defn load-gui-scene [project self resource scene]
  {:pre [(map? scene)]} ; Gui$SceneDesc in map format.
  (let [graph-id           (g/node-id->graph-id self)
        node-descs         (map node-desc->node-properties (:nodes scene)) ; TODO: These are really the properties of the GuiNode subtype. Rename to node-properties.
        tmpl-node-descs    (into {} (map (fn [n] [(:id n) {:template (:parent n) :data (extract-overrides n)}])
                                         (filter :template-node-child node-descs)))
        tmpl-node-descs    (into {} (map (fn [[id data]]
                                           [id (update data :template
                                                       (fn [parent] (if (contains? tmpl-node-descs parent)
                                                                      (recur (:template (get tmpl-node-descs parent)))
                                                                      parent)))])
                                         tmpl-node-descs))
        node-descs         (filter (complement :template-node-child) node-descs)
        tmpl-children      (group-by (comp :template second) tmpl-node-descs)
        tmpl-roots         (filter (complement tmpl-node-descs) (map first tmpl-children))
        template-data      (into {} (map (fn [r] [r (into {} (map (fn [[id tmpl]]
                                                                    [(subs id (inc (count r))) (:data tmpl)])
                                                                  (rest (tree-seq fn/constantly-true
                                                                                  (comp tmpl-children first)
                                                                                  [r nil]))))])
                                         tmpl-roots))
        custom-loader-fns  (get-registered-gui-scene-loaders)
        custom-data        (for [loader-fn custom-loader-fns
                                 :let [result (loader-fn project self scene graph-id resource)]]
                             result)
        resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      ;; TODO(save-value-cleanup): We could use set-properties-from-pb-map when setting Gui$NodeDesc properties as well.
      (gu/set-properties-from-pb-map self Gui$SceneDesc scene
        script (resolve-resource :script)
        material (resolve-resource (:material :or default-material-proj-path))
        adjust-reference :adjust-reference
        max-nodes :max-nodes
        max-dynamic-textures :max-dynamic-textures)
      (g/connect project :settings self :project-settings)
      (g/connect project :default-tex-params self :default-tex-params)
      (g/connect project :display-profiles self :display-profiles)
      (g/make-nodes graph-id [fonts-node FontsNode
                              no-font [FontNode
                                       :name ""
                                       :font (resolve-resource "/builtins/fonts/default.font")]]
                    (g/connect fonts-node :_node-id self :fonts-node) ; for the tests :/
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :build-errors self :build-errors)
                    (g/connect fonts-node :node-outline self :child-outlines)
                    (g/connect fonts-node :add-handler-info self :handler-infos)
                    (attach-font self fonts-node no-font true)
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [FontNode
                                                    :name (:name font-desc)
                                                    :font (resolve-resource (:font font-desc))]]
                                    (attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node TexturesNode
                              no-texture [InternalTextureNode
                                          :name ""
                                          :gpu-texture @texture/white-pixel]]
                    (g/connect textures-node :_node-id self :textures-node) ; for the tests :/
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :build-errors self :build-errors)
                    (g/connect textures-node :node-outline self :child-outlines)
                    (g/connect textures-node :add-handler-info self :handler-infos)
                    (attach-texture self textures-node no-texture true)
                    (for [texture-desc (:textures scene)
                          :let [resource (resolve-resource (:texture texture-desc))]]
                      (g/make-nodes graph-id [texture [TextureNode :name (:name texture-desc) :texture resource]]
                                    (attach-texture self textures-node texture))))

      ;; Materials list
      (g/make-nodes graph-id [materials-node MaterialsNode]
        (g/connect materials-node :_node-id self :nodes)
        (g/connect materials-node :build-errors self :build-errors)
        (g/connect materials-node :node-outline self :child-outlines)
        (g/connect materials-node :add-handler-info self :handler-infos)
        (for [materials-desc (:materials scene)
              :let [resource (workspace/resolve-resource resource (:material materials-desc))]]
          (g/make-nodes graph-id [material [MaterialNode :name (:name materials-desc) :material resource]]
            (attach-material self materials-node material))))

      (g/make-nodes graph-id [particlefx-resources-node ParticleFXResources
                              no-particlefx-resource [ParticleFXResource
                                                      :name ""]]
                    (g/connect particlefx-resources-node :_node-id self :particlefx-resources-node) ; for the tests :/
                    (g/connect particlefx-resources-node :_node-id self :nodes)
                    (g/connect particlefx-resources-node :build-errors self :build-errors)
                    (g/connect particlefx-resources-node :node-outline self :child-outlines)
                    (g/connect particlefx-resources-node :add-handler-info self :handler-infos)
                    (attach-particlefx-resource self particlefx-resources-node no-particlefx-resource true)
                    (let [prop-keys (g/declared-property-labels ParticleFXResource)]
                      (for [particlefx-desc (:particlefxs scene)
                            :let [particlefx-desc (select-keys particlefx-desc prop-keys)]]
                        (g/make-nodes graph-id [particlefx-resource [ParticleFXResource
                                                                     :name (:name particlefx-desc)
                                                                     :particlefx (resolve-resource (:particlefx particlefx-desc))]]
                                      (attach-particlefx-resource self particlefx-resources-node particlefx-resource)))))

      (g/make-nodes graph-id [layers-node LayersNode
                              no-layer [LayerNode
                                        :name ""]]
                    (g/connect layers-node :_node-id self :layers-node) ; for the tests :/
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :layer-msgs self :layer-msgs)
                    (g/connect layers-node :layer-names self :layer-names)
                    (g/connect layers-node :layer->index self :layer->index)
                    (g/connect layers-node :build-errors self :build-errors)
                    (g/connect layers-node :node-outline self :child-outlines)
                    (g/connect layers-node :add-handler-info self :handler-infos)
                    (attach-layer self layers-node no-layer true)
                    (loop [[layer-desc & more] (:layers scene)
                           tx-data []
                           child-index 0]
                      (if layer-desc
                        (let [layer-tx-data (g/make-nodes graph-id
                                                          [layer [LayerNode
                                                                  :name (:name layer-desc)
                                                                  :child-index child-index]]
                                                          (attach-layer self layers-node layer))]
                          (recur more (conj tx-data layer-tx-data) (inc child-index)))
                        tx-data)))
      (g/make-nodes graph-id [node-tree NodeTree]
                    (for [[from to] [[:_node-id :node-tree]
                                     [:_node-id :nodes]
                                     [:node-outline :default-node-outline]
                                     [:node-msgs :node-msgs]
                                     [:scene :default-scene]
                                     [:node-ids :node-ids]
                                     [:node-overrides :node-overrides]
                                     [:build-errors :build-errors]
                                     [:template-build-targets :template-build-targets]]]
                      (g/connect node-tree from self to))
                    (for [[from to] [[:texture-gpu-textures :texture-gpu-textures]
                                     [:texture-infos :texture-infos]
                                     [:material-infos :material-infos]
                                     [:material-shaders :material-shaders]
                                     [:font-shaders :font-shaders]
                                     [:font-datas :font-datas]
                                     [:font-names :font-names]
                                     [:layer-names :layer-names]
                                     [:layer->index :layer->index]

                                     [:spine-scene-element-ids :spine-scene-element-ids]
                                     [:spine-scene-infos :spine-scene-infos]
                                     [:spine-scene-names :spine-scene-names]

                                     [:particlefx-infos :particlefx-infos]
                                     [:particlefx-resource-names :particlefx-resource-names]
                                     [:resource-names :resource-names]
                                     [:id-prefix :id-prefix]
                                     [:current-layout :current-layout]]]
                      (g/connect self from node-tree to))
                    ;; Note that the child-index used below is
                    ;; not "local" to the parent but a global ordering of nodes.
                    ;; The indices used for sibling nodes may not be a
                    ;; continuous range. The order should still be
                    ;; correct.
                    (loop [[node-desc & more] (sort-node-descs node-descs)
                           id->node {}
                           all-tx-data []
                           child-index 0]
                      (if node-desc
                        (let [node-type (node-desc->node-type node-desc)
                              props (-> node-desc
                                        (assoc :child-index child-index)
                                        (select-keys (g/declared-property-labels node-type))
                                        (cond->
                                          (= :type-template (:type node-desc))
                                          (assoc :template {:resource (resolve-resource (:template node-desc))
                                                            :overrides (get template-data (:id node-desc) {})})))

                              tx-data (g/make-nodes graph-id [gui-node [node-type props]]
                                                    (let [parent (if (empty? (:parent node-desc))
                                                                   node-tree
                                                                   (id->node (:parent node-desc)))]
                                                      (attach-gui-node node-tree parent gui-node (:type node-desc))))
                              node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
                          (recur more (assoc id->node (:id node-desc) node-id) (into all-tx-data tx-data) (inc child-index)))
                        all-tx-data)))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                    (g/connect layouts-node :_node-id self :layouts-node) ; for the tests :/
                    (g/connect layouts-node :_node-id self :nodes)
                    (g/connect self :unused-display-profiles layouts-node :unused-display-profiles)
                    (g/connect layouts-node :build-errors self :build-errors)
                    (g/connect layouts-node :node-outline self :child-outlines)
                    (g/connect layouts-node :add-handler-info self :handler-infos)
                    (let [prop-keys (g/declared-property-labels LayoutNode)]
                      (for [layout-desc (:layouts scene)
                            :let [layout-desc (-> layout-desc
                                                  (select-keys prop-keys)
                                                  (update :nodes (fn [nodes]
                                                                   (into {}
                                                                         (map (fn [node-desc]
                                                                                (pair (:id node-desc)
                                                                                      (-> node-desc
                                                                                          node-desc->node-properties
                                                                                          extract-overrides))))
                                                                         nodes))))]]
                        (g/make-nodes graph-id [layout [LayoutNode (dissoc layout-desc :nodes)]]
                                      (attach-layout self layouts-node layout)
                                      (g/set-property layout :nodes (:nodes layout-desc))))))
      custom-data)))

(def default-pb-read-node-color (protobuf/default Gui$NodeDesc :color))
(def default-pb-read-node-alpha (protobuf/default Gui$NodeDesc :alpha))
(assert (= (float 1.0) default-pb-read-node-alpha))

(defn- sanitize-node-color [node-desc color-pb-field alpha-pb-field]
  {:pre [(map? node-desc)]} ; Gui$NodeDesc in map format.
  ;; In previous versions of the file format, alpha was stored in the fourth
  ;; color component. This was later moved to a separate alpha field to support
  ;; overriding of the alpha separately from the color.
  (let [color (get node-desc color-pb-field)]
    (if (or (nil? color)
            (= default-pb-read-node-color color))
      ;; The node does not specify color. Return unaltered.
      node-desc

      ;; The node does specify color.
      (let [color-alpha (get color 3)]
        (if (= default-pb-read-node-alpha color-alpha)
          ;; The color does not have significant alpha. Return unaltered.
          node-desc

          ;; The color has significant alpha.
          ;; Make it opaque in the color, and transfer the color alpha to the
          ;; node alpha if the node alpha has the protobuf default value.
          (-> node-desc
              (assoc color-pb-field (assoc color 3 default-pb-read-node-alpha))
              (update alpha-pb-field (fn [node-alpha]
                                       (if (or (nil? node-alpha)
                                               (= default-pb-read-node-alpha node-alpha))
                                         (or color-alpha default-pb-read-node-alpha)
                                         node-alpha)))))))))

(defn- sanitize-euler-v4 [euler-v4]
  (let [clj-quat (euler-v4->clj-quat euler-v4)
        euler-v4 (clj-quat->euler-v4 clj-quat)]
    (when (not-every? zero? (take 3 euler-v4))
      euler-v4)))

(defn- sanitize-node-geometry [node]
  (-> node
      (protobuf/sanitize :position protobuf/sanitize-optional-vector4-zero-as-vector3)
      (protobuf/sanitize :rotation sanitize-euler-v4)
      (protobuf/sanitize :scale protobuf/sanitize-optional-vector4-one-as-vector3)
      (protobuf/sanitize :size protobuf/sanitize-optional-vector4-zero-as-vector3)))

(defn- sanitize-node-specifics [node-desc]
  (case (:type node-desc)

    (:type-box :type-pie)
    (-> node-desc
        (fixup-shape-base-node-size-overrides))

    :type-text
    (-> node-desc
        (dissoc :size-mode)
        (sanitize-node-color :outline :outline-alpha)
        (sanitize-node-color :shadow :shadow-alpha))

    node-desc))

(defn- sanitize-node-fields [node-desc]
  (let [node-type (:type node-desc default-pb-node-type)
        custom-type (:custom-type node-desc 0)
        node-type-info (get-registered-node-type-info node-type custom-type)
        node-desc (assoc node-desc :type node-type) ; Explicitly include the type (pb-field is optional, so :type-box would be stripped otherwise).
        node-desc (if-some [convert-fn (:convert-fn node-type-info)]
                    (convert-fn node-type-info node-desc)
                    node-desc)]
    (-> node-desc
        (sanitize-node-color :color :alpha)
        (sanitize-node-geometry)
        (sanitize-node-specifics))))

(defn- sanitize-scene-node [node-desc]
  (let [node-desc' (sanitize-node-fields node-desc)]
    (if (:template-node-child node-desc)
      (strip-unused-overridden-fields-from-node-desc node-desc')
      (strip-redundant-size-from-node-desc node-desc'))))

(defn- sanitize-layout-node [node-desc]
  (-> node-desc
      (sanitize-node-fields)
      (strip-unused-overridden-fields-from-node-desc)))

(defn- sanitize-layout [layout]
  (protobuf/sanitize-repeated layout :nodes sanitize-layout-node))

(defn- spine-scene-desc->resource-desc [spine-scene-desc]
  (-> spine-scene-desc
      (dissoc :spine-scene)
      (assoc :path (:spine-scene spine-scene-desc))))

(defn- sanitize-scene [scene]
  (let [spine-scene-descs (mapv spine-scene-desc->resource-desc
                                (:spine-scenes scene))
        merged-resource-descs (into spine-scene-descs
                                    (:resources scene))]
    (-> scene
        (dissoc :background-color :spine-scenes)
        (protobuf/sanitize-repeated :nodes sanitize-scene-node)
        (protobuf/sanitize-repeated :layouts sanitize-layout)
        (protobuf/assign-repeated :resources merged-resource-descs)
        (update :material #(or % default-material-proj-path)))))

(defn- register [workspace def]
  (let [ext (:ext def)
        exts (if (vector? ext) ext [ext])]
    (for [ext exts]
      (resource-node/register-ddf-resource-type workspace
        :ext ext
        :label (:label def)
        :build-ext (:build-ext def)
        :node-type GuiSceneNode
        :ddf-type (:pb-class def)
        :load-fn load-gui-scene
        :sanitize-fn sanitize-scene
        :icon (:icon def)
        :icon-class (:icon-class def)
        :tags (:tags def)
        :tag-opts (:tag-opts def)
        :template (:template def)
        :view-types [:scene :text]
        :view-opts {:scene {:grid true}}))))

(defn register-resource-types [workspace]
  (register workspace pb-def))

(defn- move-child-node!
  [node-id offset]
  (let [parent (core/scope node-id)
        node-index (g/node-value node-id :child-index)
        child-indices (g/node-value parent :child-indices)
        before? (partial > node-index)
        after? (partial < node-index)
        neighbour (first (sort-by second
                                  (if (= offset -1) coll/descending-order coll/ascending-order)
                                  (filter (comp (if (= offset -1) before? after?) second)
                                          child-indices)))]
    (when neighbour
      (let [[neighbour-node-id neighbour-node-index] neighbour]
        (g/transact
          (concat
            (g/set-property node-id :child-index neighbour-node-index)
            (g/set-property neighbour-node-id :child-index node-index)))))))

(defn- selection->gui-node [selection]
  (g/override-root (handler/adapt-single selection GuiNode)))

(defn- selection->layer-node [selection]
  (g/override-root (handler/adapt-single selection LayerNode)))

(handler/defhandler :move-up :workbench
  (active? [selection] (or (selection->gui-node selection)
                           (selection->layer-node selection)))
  (enabled? [selection] (let [selected-node-id (g/override-root (handler/selection->node-id selection))
                              parent (core/scope selected-node-id)
                              node-child-index (g/node-value selected-node-id :child-index)
                              first-index (transduce (map second) min Long/MAX_VALUE (g/node-value parent :child-indices))]
                          (< first-index node-child-index)))
  (run [selection] (let [selected (g/override-root (handler/selection->node-id selection))]
                     (move-child-node! selected -1))))

(handler/defhandler :move-down :workbench
  (active? [selection] (or (selection->gui-node selection)
                           (selection->layer-node selection)))
  (enabled? [selection] (let [selected-node-id (g/override-root (handler/selection->node-id selection))
                              parent (core/scope selected-node-id)
                              node-child-index (g/node-value selected-node-id :child-index)
                              last-index (transduce (map second) max 0 (g/node-value parent :child-indices))]
                          (< node-child-index last-index)))
  (run [selection] (let [selected (g/override-root (handler/selection->node-id selection))]
                     (move-child-node! selected 1))))

(defn- resource->gui-scene
  ([project resource]
   (g/with-auto-evaluation-context evaluation-context
     (resource->gui-scene project resource evaluation-context)))
  ([project resource evaluation-context]
   (let [res-node (when resource
                    (project/get-resource-node project resource evaluation-context))]
     (when (and res-node (g/node-instance? GuiSceneNode res-node))
       res-node))))

(handler/defhandler :set-gui-layout :workbench
  (active? [project active-resource evaluation-context]
           (boolean (resource->gui-scene project active-resource evaluation-context)))
  (run [project active-resource user-data] (when user-data
                                             (when-let [scene (resource->gui-scene project active-resource)]
                                               (g/transact (g/set-property scene :visible-layout user-data)))))
  (state [project active-resource]
         (when-let [scene (resource->gui-scene project active-resource)]
           (let [visible (g/node-value scene :visible-layout)]
             {:label (if (empty? visible) "Default" visible)
              :command :set-gui-layout
              :user-data visible})))
  (options [project active-resource user-data]
           (when-not user-data
             (when-let [scene (resource->gui-scene project active-resource)]
               (let [layout-msgs (g/node-value scene :layout-msgs)
                     layouts (cons "" (map :name layout-msgs))]
                 (for [l layouts]
                   {:label (if (empty? l) "Default" l)
                    :command :set-gui-layout
                    :user-data l}))))))

(handler/register-menu! ::toolbar :visibility-settings
  [{:label :separator}
   {:icon layout-icon
    :command :set-gui-layout
    :label "Test"}])

;; SDK api
(def gui-base-node-defaults
  {:inherit-alpha true})

;; SDK api
(def visual-base-node-defaults gui-base-node-defaults)

;; SDK api
(def shape-base-node-defaults
  (assoc visual-base-node-defaults
    :manual-size default-manual-size
    :size-mode :size-mode-auto))

(def ^:private base-node-type-infos
  [{:node-type :type-box
    :node-cls BoxNode
    :display-name "Box"
    :custom-type 0
    :icon box-icon
    :defaults shape-base-node-defaults}
   {:node-type :type-pie
    :node-cls PieNode
    :display-name "Pie"
    :custom-type 0
    :icon pie-icon
    :defaults shape-base-node-defaults}
   {:node-type :type-text
    :node-cls TextNode
    :display-name "Text"
    :custom-type 0
    :icon text-icon
    :defaults (assoc visual-base-node-defaults
                :manual-size default-manual-size
                :text "<text>")}
   {:node-type :type-template
    :node-cls TemplateNode
    :display-name "Template"
    :custom-type 0
    :icon template-icon
    :defaults gui-base-node-defaults}
   {:node-type :type-particlefx
    :node-cls ParticleFXNode
    :display-name "ParticleFX"
    :custom-type 0
    :icon particlefx/particle-fx-icon
    :defaults visual-base-node-defaults}])

(defonce ^:private custom-node-type-infos (atom (sorted-map)))

(defn- get-registered-node-type-infos []
  (into base-node-type-infos
        (map val)
        @custom-node-type-infos))

(defn- get-registered-node-type-info [node-type custom-type]
  {:pre [(keyword? node-type)
         (integer? custom-type)]}
  (or (some (fn [info]
              (when (and (= node-type (:node-type info))
                         (= custom-type (:custom-type info)))
                info))
            (get-registered-node-type-infos))
      (throw (ex-info (format "Unable to locate GUI node type info. Extension not loaded? (node-type=%s, custom-type=%s)"
                              node-type
                              custom-type)
                      {:node-type node-type
                       :custom-type custom-type
                       :custom-node-type-infos (vals @custom-node-type-infos)}))))

(defn- get-registered-node-type-cls [node-type custom-type]
  (:node-cls (get-registered-node-type-info node-type custom-type)))

;; SDK api
(defn register-node-type-info! [{:keys [custom-type node-cls] :as type-info}]
  (when-not (integer? custom-type)
    (throw (ex-info (format "Plugin GUI node type %s does not specify a valid custom type."
                            (:name @node-cls))
                    {:node-cls node-cls})))
  (when-some [abstract-output-labels (not-empty (g/abstract-output-labels node-cls))]
    (throw (ex-info (format "Plugin GUI node type %s does not implement required outputs: %s"
                            (:name @node-cls)
                            (->> abstract-output-labels
                                 (sort)
                                 (map name)
                                 (str/join ", ")))
                    {:node-cls node-cls
                     :abstract-output-labels abstract-output-labels})))
  (when-not (map? (:defaults type-info))
    (throw (ex-info (format "Plugin GUI node type %s does not specify :defaults as a map of {:node-prop default-value}."
                            (:name @node-cls))
                    {:node-cls node-cls})))
  (swap! custom-node-type-infos update custom-type
         (fn [old-type-info]
           (if (nil? old-type-info)
             type-info
             (let [old-node-cls (:node-cls old-type-info)
                   old-node-type-key (:k old-node-cls)
                   new-node-type-key (:k node-cls)]
               (if (= old-node-type-key new-node-type-key)
                 type-info
                 (throw (ex-info (format "Plugin GUI node type %s custom-type conflicts with %s."
                                         (:name @node-cls)
                                         (:name @old-node-cls))
                                 {:custom-type custom-type
                                  :node-cls node-cls
                                  :conflicting-node-cls old-node-cls}))))))))

;; Used by tests
(defn clear-custom-gui-scene-loaders-and-node-types-for-tests! []
  ;; TODO(save-value-cleanup): These really should be registered with the workspace so they don't pollute integration tests across projects.
  (reset! custom-gui-scene-loaders (sorted-map))
  (reset! custom-node-type-infos (sorted-map)))
