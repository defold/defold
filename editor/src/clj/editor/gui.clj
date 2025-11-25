;; Copyright 2020-2025 The Defold Foundation
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
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.graph :as ext-graph]
            [editor.editor-extensions.node-types :as node-types]
            [editor.editor-extensions.runtime :as rt]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.gl.vertex2 :as vtx2]
            [editor.graph-util :as gu]
            [editor.gui-attachment :as gui-attachment]
            [editor.gui-clipping :as clipping]
            [editor.handler :as handler]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.menu-items :as menu-items]
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
            [editor.scene-tools :as scene-tools]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.util :as eutil]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [com.dynamo.gamesys.proto Gui$NodeDesc Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds Gui$NodeDesc$Pivot Gui$NodeDesc$SizeMode Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$SceneDesc$FontDesc Gui$SceneDesc$LayerDesc Gui$SceneDesc$LayoutDesc Gui$SceneDesc$MaterialDesc Gui$SceneDesc$ParticleFXDesc Gui$SceneDesc$TextureDesc]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.texture TextureLifecycle]
           [internal.graph.types Arc]
           [java.awt.image BufferedImage]
           [javax.vecmath Quat4d]
           [org.apache.commons.io FilenameUtils]))

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
             :label (localization/message "resource.type.gui")
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

(def ^:private node-property-to-pb-field-conversions
  (into {}
        (map (fn [{:keys [pb-field prop-key prop->pb]
                   :or {prop->pb identity}}]
               (pair prop-key (pair pb-field prop->pb))))
        property-conversions))

(def ^:private pb-field-to-node-property-conversions
  (into {}
        (map (fn [{:keys [pb-field prop-key pb->prop]
                   :or {pb->prop identity}}]
               (pair pb-field (pair prop-key pb->prop))))
        property-conversions))

(def pb-field-index->pb-field (protobuf/fields-by-indices Gui$NodeDesc))

(def pb-field-index->prop-key
  (persistent!
    (reduce-kv (fn [pb-field-index->prop-key pb-field-index pb-field]
                 (if-some [[prop-key] (pb-field-to-node-property-conversions pb-field)]
                   (assoc! pb-field-index->prop-key pb-field-index prop-key)
                   pb-field-index->prop-key))
               (transient pb-field-index->pb-field)
               pb-field-index->pb-field)))

(def prop-key->pb-field-index
  (persistent!
    (reduce-kv (fn [prop-key->pb-field-index pb-field-index prop-key]
                 (assoc! prop-key->pb-field-index prop-key pb-field-index))
               (transient {})
               pb-field-index->prop-key)))

(defn- prop-entry->pb-field-entry [[prop-key :as entry]]
  (if-some [[pb-field prop-value->pb-value] (node-property-to-pb-field-conversions prop-key)]
    (let [prop-value (val entry)
          pb-value (prop-value->pb-value prop-value)]
      (pair pb-field pb-value))
    entry))

(declare get-registered-node-type-info get-registered-node-type-infos)

;; /////////////////////////////////////////////////////////////////////////////////////////////////////////

(def gui-node-parent-attachments
  [[:id :parent]
   [:trivial-gui-scene-info :trivial-gui-scene-info]
   [:basic-gui-scene-info :basic-gui-scene-info]
   [:costly-gui-scene-info :costly-gui-scene-info]])

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

(defn- attach-gui-node [node-tree parent gui-node]
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

(def ^:private default-node-desc-pb-field-values (protobuf/default-value Gui$NodeDesc))

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

(defn- gui-node-attach-fn [target source]
  (g/with-auto-evaluation-context evaluation-context
    (let [node-tree (node->node-tree (:basis evaluation-context) target)
          taken-ids (g/node-value node-tree :id-counts evaluation-context)
          next-index (gui-attachment/next-child-index target evaluation-context)]
      (concat
        (g/update-property source :id id/resolve taken-ids)
        (g/set-property source :child-index next-index)
        (attach-gui-node node-tree target source)))))

;; SDK api
(defn gen-outline-node-tx-attach-fn
  ([attach-fn]
   (gen-outline-node-tx-attach-fn attach-fn :names))
  ([attach-fn target-name-key]
   (fn [target source]
     (let [node-tree (node->gui-scene target)
           taken-id-names (g/node-value target target-name-key)]
       ;; Note: We update the name before attaching the node to make sure the
       ;; update-gui-resource-references call in the property setter does not
       ;; treat this as a rename refactoring.
       (concat
         (g/update-property source :name id/resolve taken-id-names)
         (attach-fn node-tree target source))))))

;; Schema validation is disabled because it severely affects project load times.
;; You might want to enable these before making drastic changes to Gui nodes.

(s/def ^:private TNames [s/Str])
(s/def ^:private TNameSet #{s/Str})
(s/def ^:private TNameIntMap {s/Str s/Int})
(def ^:private TNameIndices TNameIntMap)

(s/def ^:private TMaterialInfo {(s/required-key :paged) s/Bool
                                (s/optional-key :max-page-count) s/Int})

(s/def ^:private TTextureInfo {(s/optional-key :anim-data) {s/Keyword s/Any}})

(s/def ^:private TGuiResourceType (s/enum :font
                                          :layer
                                          :material
                                          :particlefx
                                          :spine-scene
                                          :texture))

(s/def ^:private TGuiResourceNames s/Any #_(s/constrained #{s/Str} sorted?))

(s/def ^:private TGuiResourceTextures s/Any #_{s/Str TextureLifecycle})
(s/def ^:private TGuiResourceShaders s/Any #_{s/Str ShaderLifecycle})
(s/def ^:private TGuiResourceMaterialInfos s/Any #_{s/Str TMaterialInfo})
(s/def ^:private TGuiResourcePageCounts s/Any #_{s/Str (s/maybe s/Int)})
(s/def ^:private TGuiResourceTextureInfos s/Any #_{s/Str TTextureInfo})
(s/def ^:private TFontDatas s/Any #_{s/Str {s/Keyword s/Any}})
(s/def ^:private TSpineSceneElementIds s/Any #_{s/Str {:spine-anim-ids (s/constrained #{s/Str} sorted?)
                                                       :spine-skin-ids (s/constrained #{s/Str} sorted?)}})
(s/def ^:private TSpineSceneInfos s/Any #_{s/Str {:spine-data-handle (s/maybe {s/Int s/Any})
                                                  :spine-bones (s/maybe {s/Keyword s/Any})
                                                  :spine-scene-scene (s/maybe {s/Keyword s/Any})
                                                  :spine-scene-pb (s/maybe {s/Keyword s/Any})}})
(s/def ^:private TParticleFXInfos s/Any #_{s/Str {:particlefx-scene (s/maybe {s/Keyword s/Any})}})
(s/def ^:private TTrivialGuiSceneInfo {(s/optional-key :id-prefix) s/Str
                                       (s/optional-key :current-layout) s/Str
                                       (s/optional-key :layout-names) TGuiResourceNames})
(s/def ^:private TBasicGuiSceneInfo {(s/optional-key :font-names) TGuiResourceNames
                                     (s/required-key :layer->index) TNameIndices
                                     (s/optional-key :layer-names) TGuiResourceNames
                                     (s/optional-key :material-infos) TGuiResourceMaterialInfos
                                     (s/optional-key :particlefx-resource-names) TGuiResourceNames
                                     (s/optional-key :spine-scene-element-ids) TSpineSceneElementIds
                                     (s/optional-key :spine-scene-names) TGuiResourceNames
                                     (s/optional-key :texture-page-counts) TGuiResourcePageCounts
                                     (s/optional-key :texture-resource-names) TGuiResourceNames})
(s/def ^:private TCostlyGuiSceneInfo {(s/optional-key :font-datas) TFontDatas
                                      (s/optional-key :font-shaders) TGuiResourceShaders
                                      (s/optional-key :material-shaders) TGuiResourceShaders
                                      (s/optional-key :particlefx-infos) TParticleFXInfos
                                      (s/optional-key :spine-scene-infos) TSpineSceneInfos
                                      (s/optional-key :texture-gpu-textures) TGuiResourceTextures
                                      (s/optional-key :texture-infos) TGuiResourceTextureInfos})

;; SDK api
(g/deftype GuiResourceNames TGuiResourceNames)

(g/deftype ^:private Names TNames)
(g/deftype ^:private NameSet TNameSet)
(g/deftype ^:private GuiResourceTextures TGuiResourceTextures)
(g/deftype ^:private GuiResourceShaders TGuiResourceShaders)
(g/deftype ^:private GuiResourceMaterialInfos TGuiResourceMaterialInfos)
(g/deftype ^:private GuiResourcePageCounts TGuiResourcePageCounts)
(g/deftype ^:private GuiResourceTextureInfos TGuiResourceTextureInfos)
(g/deftype ^:private FontDatas TFontDatas)
(g/deftype ^:private SpineSceneElementIds TSpineSceneElementIds)
(g/deftype ^:private SpineSceneInfos TSpineSceneInfos)
(g/deftype ^:private ParticleFXInfos TParticleFXInfos)
(g/deftype ^:private GuiResourceTypeNames {TGuiResourceType TGuiResourceNames})

(g/deftype NameCounts TNameIntMap) ;; SDK api
(g/deftype ^:private NameIndices TNameIndices)
(g/deftype ^:private NameNodeIds TNameIntMap)
(g/deftype ^:private TemplateData {:resource  (s/maybe (s/protocol resource/Resource))
                                   :overrides {s/Str s/Any}})

(g/deftype ^:private NodeIndex [(s/one s/Int "node-id") (s/one s/Int "index")])
(g/deftype ^:private NameIndex [(s/one s/Str "name") (s/one s/Int "index")])

(g/deftype TrivialGuiSceneInfo TTrivialGuiSceneInfo)
(g/deftype BasicGuiSceneInfo TBasicGuiSceneInfo)
(g/deftype CostlyGuiSceneInfo TCostlyGuiSceneInfo)

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
   (optional-gui-resource-choicebox coll eutil/natural-order-sort))
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

(defmulti update-gui-resource-reference (fn [gui-resource-type evaluation-context node-id _old-name _new-name]
                                          [(g/node-type-kw (:basis evaluation-context) node-id) gui-resource-type]))
(defmethod update-gui-resource-reference :default [_ _evaluation-context _node-id _old-name _new-name] nil)

;; used by (property x (set (partial ...)), thus evaluation-context in signature
;; SDK api
(defn update-gui-resource-references [gui-resource-type evaluation-context gui-resource-node-id old-name new-name]
  (s/validate TGuiResourceType gui-resource-type)
  (assert (or (nil? old-name) (string? old-name)))
  (assert (string? new-name))
  (when (and (not (empty? old-name))
             (not (empty? new-name)))
    (let [basis (:basis evaluation-context)
          owner-gui-scene (core/scope-of-type basis gui-resource-node-id GuiSceneNode)]
      ;; Note: We can be without an owner-gui-scene if the gui resource isn't
      ;; attached yet. For example, if it is on the clipboard and about to be
      ;; pasted into a gui scene.
      (when owner-gui-scene
        (let [gui-scenes
              (tree-seq
                any?
                (fn [gui-scene]
                  (->> gui-scene
                       (g/overrides basis)
                       (e/remove
                         (fn [ov-gui-scene]
                           (-> ov-gui-scene
                               (g/valid-node-value :aux-gui-resource-type-names evaluation-context)
                               (get gui-resource-type)
                               (contains? old-name))))))
                owner-gui-scene)]
          (coll/transfer gui-scenes []
            (mapcat #(g/valid-node-value % :node-ids evaluation-context))
            (map val)
            (keep (fn [gui-node]
                    (update-gui-resource-reference gui-resource-type evaluation-context gui-node old-name new-name)))))))))

(defn- update-gui-resource-reference-impl [rename-fn evaluation-context node-id prop-kw old-name new-name]
  (let [basis (:basis evaluation-context)]
    (assert (g/property-value-origin? basis node-id :layout->prop->override))
    (concat
      (when (g/property-value-origin? basis node-id prop-kw)
        (let [old-value (g/node-value node-id prop-kw evaluation-context)
              new-value (rename-fn old-value old-name new-name)]
          (when (not= old-value new-value)
            (g/set-property node-id prop-kw new-value))))
      (let [old-layout->prop->override
            (g/node-value node-id :layout->prop->override evaluation-context)

            new-layout->prop->override
            (reduce-kv
              (fn [layout->prop->override layout-name prop->override]
                (let [old-value (prop->override prop-kw ::not-found)]
                  (case old-value
                    ::not-found layout->prop->override
                    (let [new-value (rename-fn old-value old-name new-name)]
                      (cond-> layout->prop->override
                              (not= old-value new-value)
                              (update layout-name assoc prop-kw new-value))))))
              old-layout->prop->override
              old-layout->prop->override)]
        (when-not (identical? old-layout->prop->override new-layout->prop->override)
          (g/set-property node-id :layout->prop->override new-layout->prop->override))))))

(defn- basic-gui-resource-rename-fn [prop-value old-name new-name]
  (if (= old-name prop-value)
    new-name
    prop-value))

(defn- namespaced-gui-resource-rename-fn [prop-value old-name new-name]
  (if (= old-name prop-value)
    new-name
    (let [[old-namespace element] (str/split prop-value #"/")]
      (if (and (= old-name old-namespace)
               (not (coll/empty? element)))
        (str new-name "/" element)
        prop-value))))

;; SDK api
(def update-basic-gui-resource-reference
  (partial update-gui-resource-reference-impl basic-gui-resource-rename-fn))

;; SDK api
(def update-namespaced-gui-resource-reference
  (partial update-gui-resource-reference-impl namespaced-gui-resource-rename-fn))

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

(g/defnk produce-gui-base-node-msg [_this type custom-type child-index ^:raw position ^:raw rotation ^:raw scale id generated-id ^:raw color ^:raw alpha ^:raw inherit-alpha ^:raw enabled ^:raw layer parent]
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

;; SDK api
(defmacro layout-property-getter [prop-sym]
  {:pre [(symbol? prop-sym)]}
  (let [prop-kw (keyword prop-sym)]
    `(g/fnk [~'prop->value ~prop-sym]
       (get ~'prop->value ~prop-kw ~prop-sym))))

(defn layout-property-set-fn [_evaluation-context node-id _old-value _new-value]
  (concat
    (g/invalidate-output node-id :prop->value)
    (g/invalidate-output node-id :layout->prop->value)))

;; SDK api
(defmacro layout-property-setter [prop-sym]
  {:pre [(symbol? prop-sym)]}
  layout-property-set-fn)

(defn- layout-property-prop-def? [prop-def]
  (let [value-dependencies (-> prop-def :value :dependencies)]
    (and (= 2 (count value-dependencies))
         (contains? value-dependencies :prop->value))))

(defn- node-type-deref->stripped-prop-kws-raw [node-type-deref]
  {:pre [(in/node-type-deref? node-type-deref)]}
  (->> (:property node-type-deref)
       (e/keep
         (fn [[prop-kw prop-def]]
           (when-not (layout-property-prop-def? prop-def)
             prop-kw)))
       (sort)
       (vec)))

(def ^:private node-type-deref->stripped-prop-kws (fn/memoize node-type-deref->stripped-prop-kws-raw))

(defn- make-prop->value-for-default-layout [node]
  (let [node-properties (g/own-property-values node)]
    (if (coll/empty? node-properties)
      node-properties
      (let [node-type (g/node-type node)
            stripped-prop-kws (node-type-deref->stripped-prop-kws @node-type)]
        (persistent!
          (reduce
            dissoc!
            (transient node-properties)
            stripped-prop-kws))))))

(defn- node-type-deref->layout-property-names-raw [node-type-deref]
  {:pre [(in/node-type-deref? node-type-deref)]}
  (->> node-type-deref
       :property
       (e/keep
         (fn [[prop-kw prop-def]]
           (when (layout-property-prop-def? prop-def)
             prop-kw)))
       (coll/pair-map-by #(str/replace (name %) \- \_))))

(def ^:private node-type-deref->layout-property-names (fn/memoize node-type-deref->layout-property-names-raw))

(defn- node-type->layout-property-names [node-type]
  {:pre [(g/node-type? node-type)]}
  (node-type-deref->layout-property-names @node-type))

(defn- make-recursive-prop->value-for-default-layout [basis gui-node-id]
  (let [[root-node-id & override-node-ids] (g/override-originals basis gui-node-id)
        root-node (g/node-by-id basis root-node-id)

        node-properties
        (reduce (fn [node-properties override-node-id]
                  (if-let [override-node (g/node-by-id basis override-node-id)]
                    (reduce conj! node-properties (gt/overridden-properties override-node))
                    node-properties))
                (transient (g/own-property-values root-node))
                override-node-ids)]

    (if (coll/empty? node-properties)
      {}
      (let [node-type (g/node-type root-node)
            stripped-prop-kws (node-type-deref->stripped-prop-kws @node-type)]
        (persistent!
          (reduce dissoc! node-properties stripped-prop-kws))))))

(defn- layout-property-changes-tx-data
  "Takes changes returned by a changes-fn associated with a layout property
  and returns the transaction steps that perform those changes to the specified
  layout in the GuiNode."
  [layout-name node-id changes]
  {:pre [(string? layout-name)]}
  (if (str/blank? layout-name)
    (coll/transfer changes []
      (mapcat (fn [[prop-kw value]]
                (if (nil? value)
                  (g/clear-property node-id prop-kw)
                  (g/set-property node-id prop-kw value)))))
    (g/update-property
      node-id :layout->prop->override
      update layout-name
      (fn [prop->override]
        (coll/not-empty
          (reduce-kv (fn [prop->override prop-kw new-value]
                       (if (nil? new-value)
                         (dissoc prop->override prop-kw)
                         (assoc prop->override prop-kw new-value)))
                     prop->override
                     changes))))))

(defn- layout-property-clears-tx-data
  "Takes a sequence of prop-kws, possibly based on the return value of a
  changes-fn, and returns the transaction steps that will clear overrides for
  those properties."
  [layout-name node-id cleared-prop-kws]
  (when (coll/not-empty cleared-prop-kws)
    (if (str/blank? layout-name)
      (coll/mapcat
        #(g/clear-property node-id %)
        cleared-prop-kws)
      (g/update-property
        node-id :layout->prop->override
        update layout-name
        (fn [prop->override]
          (coll/not-empty
            (apply dissoc prop->override cleared-prop-kws)))))))

(defn- prop->value-for-specific-layout
  [node-id layout-name evaluation-context]
  (let [layout->prop->value (g/node-value node-id :layout->prop->value evaluation-context)
        prop->value (get layout->prop->value layout-name ::not-found)]
    (when (= ::not-found prop->value)
      (throw (ex-info (format "Layout '%s' does not exist in the scene." layout-name)
                      {:node-id node-id
                       :layout-name layout-name
                       :layout-name-candidates (vec (sort (keys layout->prop->value)))})))
    prop->value))

(defn- layout-property-edit-type-set-in-specific-layout
  [layout-name changes-fn prop-kw evaluation-context node-id old-value new-value]
  (let [changes (if (nil? changes-fn)
                  {prop-kw new-value}
                  (changes-fn evaluation-context node-id prop-kw old-value new-value))]
    (layout-property-changes-tx-data layout-name node-id changes)))

(defn layout-property-edit-type-set-in-current-layout
  [changes-fn prop-kw evaluation-context node-id old-value new-value]
  (let [trivial-gui-scene-info (g/valid-node-value node-id :trivial-gui-scene-info evaluation-context)
        current-layout (:current-layout trivial-gui-scene-info)]
    (layout-property-edit-type-set-in-specific-layout current-layout changes-fn prop-kw evaluation-context node-id old-value new-value)))

(defn- layout-property-edit-type-clear-in-specific-layout
  [layout-name changes-fn node-id prop-kw]
  (let [cleared-prop-kws
        (if (nil? changes-fn)
          [prop-kw]
          (g/with-auto-evaluation-context evaluation-context
            (keys (changes-fn evaluation-context node-id prop-kw nil nil))))]
    (layout-property-clears-tx-data layout-name node-id cleared-prop-kws)))

(defn layout-property-edit-type-clear-in-current-layout
  [changes-fn node-id prop-kw]
  (g/with-auto-evaluation-context evaluation-context
    (let [trivial-gui-scene-info (g/valid-node-value node-id :trivial-gui-scene-info evaluation-context)
          current-layout (:current-layout trivial-gui-scene-info)

          cleared-prop-kws
          (if (nil? changes-fn)
            [prop-kw]
            (keys (changes-fn evaluation-context node-id prop-kw nil nil)))]

      (layout-property-clears-tx-data current-layout node-id cleared-prop-kws))))

(defn basic-layout-property-clear-in-current-layout
  [node-id prop-kw]
  (let [trivial-gui-scene-info (g/valid-node-value node-id :trivial-gui-scene-info)
        current-layout (:current-layout trivial-gui-scene-info)]
    (layout-property-clears-tx-data current-layout node-id [prop-kw])))

(defn- basic-layout-property-update-in-current-layout
  [evaluation-context node-id prop-kw update-fn & args]
  (let [old-value (g/node-value node-id prop-kw evaluation-context)
        new-value (apply update-fn old-value args)
        trivial-gui-scene-info (g/valid-node-value node-id :trivial-gui-scene-info evaluation-context)
        current-layout (:current-layout trivial-gui-scene-info)]
    (layout-property-changes-tx-data current-layout node-id {prop-kw new-value})))

(defn- layout-property-set-in-specific-layout
  "Uses the changes-fn associated with each property to create a series of
  transaction steps that will set or clear properties on a GuiNode in a
  specific layout. Nil is not a valid value for a layout property, but nil
  values can be used to clear overrides on specific properties."
  [evaluation-context layout-name node-id & prop-kws-and-values]
  {:pre [(string? layout-name)]}
  (let [kv-count (count prop-kws-and-values)]
    (when (pos? kv-count)
      (assert (even? kv-count))
      (let [basis (:basis evaluation-context)
            node (g/node-by-id basis node-id)
            prop->value-delay (delay (prop->value-for-specific-layout node-id layout-name evaluation-context))

            changes
            (coll/transfer prop-kws-and-values {}
              (partition-all 2)
              (mapcat (fn [[prop-kw new-value]]
                        (if-let [changes-fn (:changes-fn (g/node-property-dynamic node prop-kw :edit-type evaluation-context))]
                          (let [old-value (get @prop->value-delay prop-kw)]
                            (changes-fn evaluation-context node-id prop-kw old-value new-value))
                          {prop-kw new-value}))))]

        (coll/not-empty
          (layout-property-changes-tx-data layout-name node-id changes))))))

(defn- layout-property-clear-in-specific-layout
  [evaluation-context layout-name node-id prop-kw]
  (coll/not-empty
    (layout-property-clears-tx-data
      layout-name node-id
      (if-let [changes-fn (-> evaluation-context
                              :basis
                              (g/node-by-id node-id)
                              (g/node-property-dynamic prop-kw :edit-type evaluation-context)
                              :changes-fn)]
        (keys (changes-fn evaluation-context node-id prop-kw nil nil))
        [prop-kw]))))

;; SDK api
(defmacro wrap-layout-property-edit-type
  ([prop-sym edit-type-form]
   {:pre [(symbol? prop-sym)]}
   (let [edit-type-set-fn-form `(fn/partial layout-property-edit-type-set-in-current-layout nil ~(keyword prop-sym))]
     `(assoc ~edit-type-form
        :set-fn ~edit-type-set-fn-form
        :clear-fn basic-layout-property-clear-in-current-layout)))
  ([prop-sym edit-type-form changes-fn-form]
   {:pre [(symbol? prop-sym)
          (symbol? changes-fn-form)]}
   (let [edit-type-set-fn-form `(fn/partial layout-property-edit-type-set-in-current-layout ~changes-fn-form ~(keyword prop-sym))
         edit-type-clear-fn-form `(fn/partial layout-property-edit-type-clear-in-current-layout ~changes-fn-form)]
     `(assoc ~edit-type-form
        :set-fn ~edit-type-set-fn-form
        :clear-fn ~edit-type-clear-fn-form
        :changes-fn ~changes-fn-form))))

;; SDK api
(defmacro layout-property-edit-type
  ([prop-sym edit-type-form]
   `(g/constantly
      (wrap-layout-property-edit-type ~prop-sym ~edit-type-form)))
  ([prop-sym edit-type-form changes-fn-form]
   `(g/constantly
      (wrap-layout-property-edit-type ~prop-sym ~edit-type-form ~changes-fn-form))))

(defn- scale-property-changes-fn [_evaluation-context _self _prop-kw _old-value new-value]
  {:scale (some-> new-value scene/non-zeroify-scale)})

(g/defnode GuiNode
  (inherits core/Scope)
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  ;; Storage for layout overrides.
  (property layout->prop->override g/Any (default {})
            (dynamic visible (g/constantly false)))

  ;; SceneNode property overrides with layout support
  (property position types/Vec3 (default scene/default-position)
            (dynamic edit-type (layout-property-edit-type position {:type types/Vec3}))
            (value (layout-property-getter position))
            (set (layout-property-setter position)))
  (property rotation types/Vec4 (default scene/default-rotation)
            (dynamic edit-type (layout-property-edit-type rotation properties/quat-rotation-edit-type))
            (value (layout-property-getter rotation))
            (set (layout-property-setter rotation)))
  (property scale types/Vec3 (default scene/default-scale)
            (dynamic edit-type (layout-property-edit-type scale {:type types/Vec3 :precision 0.1} scale-property-changes-fn))
            (value (layout-property-getter scale))
            (set (layout-property-setter scale)))

  (property child-index g/Int (dynamic visible (g/constantly false)) (default 0)) ; No protobuf counterpart.
  (property type g/Keyword (dynamic visible (g/constantly false))) ; Always assigned in load-fn.
  (property custom-type g/Int (dynamic visible (g/constantly false)) (default (protobuf/default Gui$NodeDesc :custom-type)))

  (input trivial-gui-scene-info TrivialGuiSceneInfo)
  (output trivial-gui-scene-info TrivialGuiSceneInfo (gu/passthrough trivial-gui-scene-info))
  (input basic-gui-scene-info BasicGuiSceneInfo)
  (output basic-gui-scene-info BasicGuiSceneInfo (gu/passthrough basic-gui-scene-info))
  (input costly-gui-scene-info CostlyGuiSceneInfo)
  (output costly-gui-scene-info CostlyGuiSceneInfo (gu/passthrough costly-gui-scene-info))

  (input id-counts NameCounts)

  (output node-id+child-index NodeIndex (g/fnk [_node-id child-index] [_node-id child-index]))

  (property id g/Str (default (protobuf/default Gui$NodeDesc :id))
            (dynamic error (g/fnk [_node-id id id-counts] (prop-unique-id-error _node-id :id id id-counts "Id")))
            (dynamic visible not-override-node?))
  (property generated-id g/Str ; Just for presentation.
            (dynamic label (properties/label-dynamic :id))
            (dynamic tooltip (properties/tooltip-dynamic :id))
            (value (gu/passthrough id)) ; see (output id ...) below
            (dynamic read-only? (g/constantly true))
            (dynamic visible override-node?))
  (property color types/Color (default (protobuf/default Gui$NodeDesc :color))
            (dynamic visible (g/fnk [type] (not= type :type-template)))
            (dynamic edit-type (layout-property-edit-type color {:type types/Color
                                                                 :ignore-alpha? true}))
            (dynamic label (properties/label-dynamic :gui :color))
            (dynamic tooltip (properties/tooltip-dynamic :gui :color))
            (value (layout-property-getter color))
            (set (layout-property-setter color)))
  (property alpha g/Num (default (protobuf/default Gui$NodeDesc :alpha))
            (dynamic edit-type (layout-property-edit-type alpha {:type :slider
                                                                 :min 0.0
                                                                 :max 1.0
                                                                 :precision 0.01}))
            (dynamic label (properties/label-dynamic :gui :alpha))
            (dynamic tooltip (properties/tooltip-dynamic :gui :alpha))
            (value (layout-property-getter alpha))
            (set (layout-property-setter alpha)))
  (property inherit-alpha g/Bool (default (protobuf/default Gui$NodeDesc :inherit-alpha))
            (dynamic edit-type (layout-property-edit-type inherit-alpha {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :inherit-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :gui :inherit-alpha))
            (value (layout-property-getter inherit-alpha))
            (set (layout-property-setter inherit-alpha)))
  (property enabled g/Bool (default (protobuf/default Gui$NodeDesc :enabled))
            (dynamic edit-type (layout-property-edit-type enabled {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :enabled))
            (dynamic tooltip (properties/tooltip-dynamic :gui :enabled))
            (value (layout-property-getter enabled))
            (set (layout-property-setter enabled)))
  (property layer g/Str (default (protobuf/default Gui$NodeDesc :layer))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [layer->index (:layer->index basic-gui-scene-info)
                                       layer-names (:layer-names basic-gui-scene-info)]
                                   (wrap-layout-property-edit-type layer (optional-gui-resource-choicebox layer-names (partial sort-by layer->index))))))
            (dynamic error (g/fnk [_node-id layer basic-gui-scene-info]
                             (let [layer-names (:layer-names basic-gui-scene-info)]
                               (validate-layer true _node-id layer-names layer))))
            (dynamic label (properties/label-dynamic :gui :layer))
            (dynamic tooltip (properties/tooltip-dynamic :gui :layer))
            (value (layout-property-getter layer))
            (set (layout-property-setter layer)))
  (output layer-index g/Any
          (g/fnk [basic-gui-scene-info layer]
            (let [layer->index (:layer->index basic-gui-scene-info)]
              (layer->index layer))))

  (input parent g/Str)
  (input child-scenes g/Any :array)
  (input child-indices NodeIndex :array)
  (output node-outline-link resource/Resource (g/constantly nil))
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [child-outlines]
                                                                     (vec (sort-by :child-index child-outlines))))
  (output node-outline-reqs g/Any :cached (g/fnk []
                                            (mapv (fn [type-info]
                                                    {:node-type (:node-cls type-info)
                                                     :tx-attach-fn gui-node-attach-fn})
                                                  (get-registered-node-type-infos))))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id child-index node-outline-link node-outline-children node-outline-reqs type custom-type own-build-errors trivial-gui-scene-info layout->prop->override _overridden-properties]
            (let [current-layout (:current-layout trivial-gui-scene-info)]
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
                       :outline-overridden? (if (str/blank? current-layout)
                                              (< 1 (count _overridden-properties)) ; :layout->prop->override will always be present, and we shouldn't count it.
                                              (pos? (count (layout->prop->override current-layout))))}
                      (resource/resource? node-outline-link) (assoc :link node-outline-link :outline-reference? true)))))

  (output transform-properties g/Any scene/produce-scalable-transform-properties)
  (output gui-base-node-msg g/Any produce-gui-base-node-msg)
  (output node-msg g/Any :abstract)
  (input node-msgs g/Any :array)
  (output node-msgs g/Any (g/fnk [layout->prop->override layout->prop->value node-msg node-msgs]
                            (let [decorated-node-msg
                                  (assoc node-msg
                                    :layout->prop->override layout->prop->override
                                    :layout->prop->value layout->prop->value)]
                              (into [decorated-node-msg]
                                    (map #(dissoc % :child-index))
                                    (flatten
                                      (sort-by #(get-in % [0 :child-index])
                                               node-msgs))))))
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

  (input node-ids NameNodeIds :array)
  (output id g/Str (g/fnk [id trivial-gui-scene-info] (str (:id-prefix trivial-gui-scene-info) id)))
  (output node-ids NameNodeIds (g/fnk [_node-id id node-ids] (reduce coll/merge {id _node-id} node-ids)))

  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides id _overridden-properties]
                                         (into {id _overridden-properties}
                                               node-overrides)))
  (output layout->prop->value g/Any :cached
          (g/fnk [^:unsafe _evaluation-context _this layout->prop->override trivial-gui-scene-info]
            ;; All layout-property-setters explicitly invalidate this output, so
            ;; it is safe to extract properties from _this here.
            (let [original-node-id (gt/original _this)
                  layout->prop->value-for-original (some-> original-node-id (g/node-value :layout->prop->value _evaluation-context))]
              (if (g/error-value? layout->prop->value-for-original)
                layout->prop->value-for-original
                (let [original-meta (meta layout->prop->value-for-original)
                      original-layout-names (:layout-names original-meta)
                      layout-names (:layout-names trivial-gui-scene-info)

                      prop->value-for-default-layout-in-original
                      (coll/not-empty (get layout->prop->value-for-original ""))

                      prop->value-for-default-layout
                      (cond->> (make-prop->value-for-default-layout _this)
                               prop->value-for-default-layout-in-original
                               (coll/merge prop->value-for-default-layout-in-original))

                      introduced-layout-names
                      (if original-layout-names
                        (e/remove original-layout-names layout-names)
                        layout-names)]

                  (-> (reduce
                        (fn [layout->prop->value introduced-layout-name]
                          (update layout->prop->value introduced-layout-name coll/merge prop->value-for-default-layout))
                        layout->prop->value-for-original
                        introduced-layout-names)
                      (assoc "" prop->value-for-default-layout)
                      (coll/deep-merge layout->prop->override)
                      (vary-meta assoc :layout-names layout-names)))))))
  (output prop->value g/Any :cached
          (g/fnk [^:unsafe _evaluation-context _node-id layout->prop->override trivial-gui-scene-info]
            ;; This output is used in the getters for all layout-related
            ;; properties. Since it only needs to consider the current layout,
            ;; and will fall back on the raw property values from the default
            ;; layout if there isn't a layout-specific override, we can make it
            ;; faster than the full layout->prop->value output.
            ;;
            ;; All layout-property-setters explicitly invalidate this output, so
            ;; it is safe to evaluate properties on ourselves and our override
            ;; originals here.
            (let [current-layout (:current-layout trivial-gui-scene-info)]
              (when (coll/not-empty current-layout)
                (let [basis (:basis _evaluation-context)]
                  (loop [node-id _node-id
                         layout-names (:layout-names trivial-gui-scene-info)
                         prop->value (get layout->prop->override current-layout)]
                    (if-let [original-node-id (g/override-original basis node-id)]
                      (let [trivial-gui-scene-info-for-original (g/node-value original-node-id :trivial-gui-scene-info _evaluation-context)]
                        (if (g/error-value? trivial-gui-scene-info-for-original)
                          trivial-gui-scene-info-for-original

                          ;; If our scene introduces the current-layout, we
                          ;; don't need to consider any more layout overrides
                          ;; from our override originals. Instead, anything not
                          ;; overridden for the current-layout by this point
                          ;; will use values from our default layout, or values
                          ;; inherited from the default layout in our override
                          ;; originals.
                          (let [layout-names-for-original (:layout-names trivial-gui-scene-info-for-original)
                                introduces-current-layout (and (contains? layout-names current-layout)
                                                               (not (contains? layout-names-for-original current-layout)))]
                            (if introduces-current-layout
                              (coll/merge
                                (make-recursive-prop->value-for-default-layout basis node-id)
                                prop->value)
                              (let [layout->prop->override-for-original (g/node-value original-node-id :layout->prop->override _evaluation-context)]
                                (if (g/error-value? layout->prop->override-for-original)
                                  layout->prop->override-for-original
                                  (recur original-node-id
                                         layout-names-for-original
                                         (coll/merge
                                           (get layout->prop->override-for-original current-layout)
                                           prop->value))))))))

                      ;; We've reached the override root. Anything not
                      ;; overridden for the current-layout by this point will
                      ;; use values from our default layout.
                      (let [node (g/node-by-id basis node-id)]
                        (coll/merge
                          (make-prop->value-for-default-layout node)
                          prop->value)))))))))
  (output _properties g/Properties :cached
          (g/fnk [_declared-properties layout->prop->override trivial-gui-scene-info]
            ;; For layout properties, the :original-value of each property is
            ;; based on the value returned by the layout-property-getter.
            ;; However, the presence of the :original-value is based on whether
            ;; we have an override node. What we want is for the :original-value
            ;; to be present when the user should be able to clear an override
            ;; from the current layout.
            (let [current-layout (:current-layout trivial-gui-scene-info)]
              (if (str/blank? current-layout)
                ;; We're observing the Default layout. Since the :original-value
                ;; will reflect overrides to the Default layout in this case, we
                ;; don't have to do anything.
                _declared-properties

                ;; We're observing a non-Default layout. We need to manually
                ;; manage the :original-value to reflect the layout property
                ;; overrides present on this node.
                (let [prop-kw->layout-override-value (layout->prop->override current-layout)]
                  (update _declared-properties :properties
                          (fn [prop-kw->prop-info]
                            (into {}
                                  (map
                                    (fn [[prop-kw prop-info]]
                                      (let [layout-override-value (get prop-kw->layout-override-value prop-kw)]
                                        (pair prop-kw
                                              (cond-> (assoc prop-info :assoc-original-value? false) ; Disable automatic assoc in OverrideNode.produce-value. We want to manage it ourselves.

                                                      (some? layout-override-value)
                                                      (assoc :original-value layout-override-value) ; Any :original-value is fine. The key just needs to be present.

                                                      (nil? layout-override-value)
                                                      (dissoc :original-value))))))
                                  prop-kw->prop-info))))))))
  (input child-build-errors g/Any :array)
  (output build-errors-gui-node g/Any
          (g/fnk [_node-id basic-gui-scene-info id id-counts layer]
            (let [layer-names (:layer-names basic-gui-scene-info)]
              (g/package-errors
                _node-id
                (prop-unique-id-error _node-id :id id id-counts "Id")
                (validate-layer false _node-id layer-names layer)))))
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

(defmethod scene-tools/manip-move ::GuiNode [evaluation-context node-id delta]
  (basic-layout-property-update-in-current-layout evaluation-context node-id :position scene/apply-move-delta delta))

(defmethod scene-tools/manip-rotate ::GuiNode [evaluation-context node-id delta]
  (basic-layout-property-update-in-current-layout evaluation-context node-id :rotation scene/apply-rotate-delta delta))

(defmethod scene-tools/manip-scale ::GuiNode [evaluation-context node-id delta]
  (basic-layout-property-update-in-current-layout evaluation-context node-id :scale scene/apply-scale-delta delta))

(defn- transfer-overrides-target-properties
  [target-node-id target-layout-name evaluation-context]
  {:pre [(string? target-layout-name)]}
  (let [layout->prop->value (g/node-value target-node-id :layout->prop->value evaluation-context)
        prop-kw->prop-info (:properties (g/node-value target-node-id :_declared-properties evaluation-context))
        prop-kw->value (get layout->prop->value target-layout-name ::not-found)]
    ;; We don't return any target properties if the layout does not exist in the
    ;; target node. Otherwise, we would have to create the layout in the target
    ;; gui scene, which might be confusing?
    (when (not= ::not-found prop-kw->value)
      (coll/transfer prop-kw->prop-info {}
        (map (fn [[prop-kw prop-info]]
               (let [value (get prop-kw->value prop-kw)
                     edit-type (:edit-type prop-info)
                     changes-fn (:changes-fn edit-type)
                     set-fn (fn/partial layout-property-edit-type-set-in-specific-layout target-layout-name changes-fn prop-kw)
                     clear-fn (fn/partial layout-property-edit-type-clear-in-specific-layout target-layout-name changes-fn)]
                 (pair prop-kw
                       {:node-id (or (:node-id prop-info) target-node-id)
                        :prop-kw (or (:prop-kw prop-info) prop-kw)
                        :value value
                        :edit-type (assoc edit-type
                                     :set-fn set-fn
                                     :clear-fn clear-fn)}))))))))

(defn- transfer-overrides-plan
  [override-transfer-type source-layout-name source-prop-infos-by-prop-kw target-node-id+layout-names {:keys [basis] :as evaluation-context}]
  {:pre [(not (coll/empty? target-node-id+layout-names))]}
  (when-let [target-infos
             (coll/not-empty
               (coll/transfer target-node-id+layout-names []
                 (keep (fn [[target-node-id target-layout-name]]
                         (when-let [target-prop-infos-by-prop-kw (transfer-overrides-target-properties target-node-id target-layout-name evaluation-context)]
                           (cond-> {:target-node-id target-node-id
                                    :target-prop-infos-by-prop-kw target-prop-infos-by-prop-kw}
                                   (or (not= "" source-layout-name)
                                       (not= "" target-layout-name))
                                   (assoc :target-aspect
                                          [(localization/message "override.aspect.layout" {"layout" (if (= "" target-layout-name)
                                                                                                      (localization/message "gui.layout.default")
                                                                                                      target-layout-name)})
                                           (localization/message "override.aspect.layout.kind")])))))))]
    (properties/transfer-overrides-plan basis override-transfer-type source-prop-infos-by-prop-kw target-infos)))

(defmethod properties/pull-up-overrides-plan-alternatives ::GuiNode
  [source-node-id source-prop-infos-by-prop-kw {:keys [basis] :as evaluation-context}]
  (when-let [trivial-gui-scene-info (g/maybe-node-value source-node-id :trivial-gui-scene-info evaluation-context)]
    (let [source-layout-name (or (:current-layout trivial-gui-scene-info) "")

          original-node-ids
          (iterate #(g/override-original basis %)
                   (g/override-original basis source-node-id))

          pull-up-overrides-to-default-layout-plan
          (when (not= "" source-layout-name)
            (transfer-overrides-plan :pull-up-overrides source-layout-name source-prop-infos-by-prop-kw [[source-node-id ""]] evaluation-context))

          source-node-layout-transfer-overrides-plans
          (if pull-up-overrides-to-default-layout-plan
            [pull-up-overrides-to-default-layout-plan]
            [])]

      (coll/transfer
        original-node-ids source-node-layout-transfer-overrides-plans
        (take-while some?)
        (keep (fn [original-node-id]
                (transfer-overrides-plan :pull-up-overrides source-layout-name source-prop-infos-by-prop-kw [[original-node-id source-layout-name]] evaluation-context)))))))

(defmethod properties/push-down-overrides-plan-alternatives ::GuiNode
  [source-node-id source-prop-infos-by-prop-kw {:keys [basis] :as evaluation-context}]
  (when-let [override-node-ids (coll/not-empty (g/overrides basis source-node-id))]
    (when-let [trivial-gui-scene-info (g/maybe-node-value source-node-id :trivial-gui-scene-info evaluation-context)]
      (let [source-layout-name (or (:current-layout trivial-gui-scene-info) "")
            target-node-id+layout-names (e/map #(pair % source-layout-name) override-node-ids)
            transfer-overrides-plan (transfer-overrides-plan :push-down-overrides source-layout-name source-prop-infos-by-prop-kw target-node-id+layout-names evaluation-context)]
        [transfer-overrides-plan]))))

;; SDK api
(defmethod update-gui-resource-reference [::GuiNode :layer]
  [_ evaluation-context node-id old-name new-name]
  (update-basic-gui-resource-reference evaluation-context node-id :layer old-name new-name))

(defn- validate-particlefx-adjust-mode [node-id adjust-mode]
  (validation/prop-error :warning
                         node-id
                         :adjust-mode
                         (fn [adjust-mode]
                           (when (= adjust-mode :adjust-mode-stretch)
                             "Adjust mode \"Stretch\" not supported for particlefx nodes."))
                         adjust-mode))

(def ^:private validate-material-resource (partial validate-optional-gui-resource "Material '%s' does not exist in the scene" :material))

(defn- validate-material-capabilities [_node-id material-infos material texture-page-counts texture]
  (let [material-info (or (get material-infos material)
                          (get material-infos ""))
        is-paged-material (:paged material-info)
        material-max-page-count (:max-page-count material-info)
        texture-page-count (get texture-page-counts texture)]
    (validation/prop-error :fatal _node-id :material shader/page-count-mismatch-error-message is-paged-material texture-page-count material-max-page-count "Texture")))

(g/defnk produce-visual-base-node-msg [gui-base-node-msg ^:raw visible ^:raw blend-mode ^:raw adjust-mode ^:raw material ^:raw pivot ^:raw x-anchor ^:raw y-anchor]
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

  (property visible g/Bool (default (protobuf/default Gui$NodeDesc :visible))
            (dynamic edit-type (layout-property-edit-type visible {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :visible))
            (dynamic tooltip (properties/tooltip-dynamic :gui :visible))
            (value (layout-property-getter visible))
            (set (layout-property-setter visible)))
  (property blend-mode g/Keyword (default (protobuf/default Gui$NodeDesc :blend-mode))
            (dynamic edit-type (layout-property-edit-type blend-mode (properties/->pb-choicebox Gui$NodeDesc$BlendMode)))
            (value (layout-property-getter blend-mode))
            (set (layout-property-setter blend-mode)))
  (property adjust-mode g/Keyword (default (protobuf/default Gui$NodeDesc :adjust-mode))
            (dynamic error (g/fnk [_node-id adjust-mode type]
                                  (when (= type :type-particlefx)
                                    (validate-particlefx-adjust-mode _node-id adjust-mode))))
            (dynamic edit-type (layout-property-edit-type adjust-mode (properties/->pb-choicebox Gui$NodeDesc$AdjustMode)))
            (dynamic label (properties/label-dynamic :gui :adjust-mode))
            (dynamic tooltip (properties/tooltip-dynamic :gui :adjust-mode))
            (value (layout-property-getter adjust-mode))
            (set (layout-property-setter adjust-mode)))
  (property pivot g/Keyword (default (protobuf/default Gui$NodeDesc :pivot))
            (dynamic edit-type (layout-property-edit-type pivot (properties/->pb-choicebox Gui$NodeDesc$Pivot)))
            (dynamic label (properties/label-dynamic :gui :pivot))
            (dynamic tooltip (properties/tooltip-dynamic :gui :pivot))
            (value (layout-property-getter pivot))
            (set (layout-property-setter pivot)))
  (property x-anchor g/Keyword (default (protobuf/default Gui$NodeDesc :xanchor))
            (dynamic edit-type (layout-property-edit-type x-anchor (properties/->pb-choicebox Gui$NodeDesc$XAnchor)))
            (dynamic label (properties/label-dynamic :gui :x-anchor))
            (dynamic tooltip (properties/tooltip-dynamic :gui :x-anchor))
            (value (layout-property-getter x-anchor))
            (set (layout-property-setter x-anchor)))
  (property y-anchor g/Keyword (default (protobuf/default Gui$NodeDesc :yanchor))
            (dynamic edit-type (layout-property-edit-type y-anchor (properties/->pb-choicebox Gui$NodeDesc$YAnchor)))
            (dynamic label (properties/label-dynamic :gui :y-anchor))
            (dynamic tooltip (properties/tooltip-dynamic :gui :y-anchor))
            (value (layout-property-getter y-anchor))
            (set (layout-property-setter y-anchor)))
  (property material g/Str (default (protobuf/default Gui$NodeDesc :material))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [material-infos (:material-infos basic-gui-scene-info)
                                       material-names (some-> material-infos keys)]
                                   (wrap-layout-property-edit-type material (optional-gui-resource-choicebox material-names)))))
            (value (layout-property-getter material))
            (set (layout-property-setter material))
            (dynamic error (g/fnk [_node-id basic-gui-scene-info material]
                             (let [material-infos (:material-infos basic-gui-scene-info)]
                               (validate-material-resource _node-id material-infos material)))))

  (output visual-base-node-msg g/Any produce-visual-base-node-msg)
  (output material-shader ShaderLifecycle (g/fnk [costly-gui-scene-info material]
                                            (let [material-shaders (:material-shaders costly-gui-scene-info)]
                                              (or (get material-shaders material)
                                                  (get material-shaders "")))))
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
          (g/fnk [_node-id basic-gui-scene-info build-errors-gui-node material]
            (let [material-infos (:material-infos basic-gui-scene-info)]
              (g/package-errors
                _node-id
                build-errors-gui-node
                (validate-material-resource _node-id material-infos material)))))

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

(g/defnk produce-shape-base-node-msg [visual-base-node-msg ^:raw manual-size ^:raw size-mode ^:raw texture ^:raw clipping-mode ^:raw clipping-visible ^:raw clipping-inverted]
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

(defn- size-mode-property-changes-fn [{:keys [basis] :as evaluation-context} self _prop-kw old-value new-value]
  (coll/merge
    {:size-mode new-value}
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
        {:manual-size nil}

        ;; Use the size of the assigned texture as :manual-size
        ;; when the user switches from :size-mode-auto to
        ;; :size-mode-manual.
        (and (= :size-mode-auto old-value)
             (= :size-mode-manual new-value))
        (let [costly-gui-scene-info (g/node-value self :costly-gui-scene-info evaluation-context)
              texture-infos (:texture-infos costly-gui-scene-info)
              texture-info (get texture-infos texture)]
          (when-some [anim-data (:anim-data texture-info)]
            (let [texture-size [(float (:width anim-data)) (float (:height anim-data)) protobuf/float-zero]]
              {:manual-size texture-size})))))))

(defn- texture-property-changes-fn [{:keys [basis] :as evaluation-context} self _prop-kw old-value new-value]
  ;; Clear any existing :manual-size override when
  ;; assigning a texture will hide the :manual-size
  ;; property.
  (let [size-mode (g/node-value self :size-mode evaluation-context)]
    (cond-> {:texture new-value}
            (and (= :manual-size (visible-size-property-label size-mode old-value))
                 (g/property-overridden? basis self :manual-size)
                 (let [effective-new-value
                       (or new-value
                           (let [original-node-id (g/override-original basis self)]
                             (g/node-value original-node-id :texture evaluation-context)))]
                   (= :texture-size (visible-size-property-label size-mode effective-new-value))))
            (assoc :manual-size nil))))

(g/defnode ShapeNode
  (inherits VisualNode)

  (property manual-size types/Vec3 (default (protobuf/vector4->vector3 (protobuf/default Gui$NodeDesc :size)))
            (dynamic label (properties/label-dynamic :size))
            (dynamic tooltip (properties/tooltip-dynamic :size))
            (dynamic visible (g/fnk [size-mode texture]
                               (= :manual-size (visible-size-property-label size-mode texture))))
            (dynamic edit-type (layout-property-edit-type manual-size {:type types/Vec3}))
            (value (layout-property-getter manual-size))
            (set (layout-property-setter manual-size)))
  (property texture-size types/Vec3 ; Just for presentation.
            (value (g/fnk [anim-data]
                     (if (some? anim-data)
                       [(float (:width anim-data)) (float (:height anim-data)) protobuf/float-zero]
                       protobuf/vector3-zero)))
            (dynamic label (properties/label-dynamic :size))
            (dynamic tooltip (properties/tooltip-dynamic :size))
            (dynamic read-only? (g/constantly true))
            (dynamic visible (g/fnk [size-mode texture]
                               (= :texture-size (visible-size-property-label size-mode texture)))))
  (property size-mode g/Keyword (default (protobuf/default Gui$NodeDesc :size-mode))
            (dynamic edit-type (layout-property-edit-type size-mode (properties/->pb-choicebox Gui$NodeDesc$SizeMode) size-mode-property-changes-fn))
            (dynamic label (properties/label-dynamic :gui :size-mode))
            (dynamic tooltip (properties/tooltip-dynamic :gui :size-mode))
            (value (layout-property-getter size-mode))
            (set (layout-property-setter size-mode)))
  (property material g/Str (default (protobuf/default Gui$NodeDesc :material))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [material-infos (:material-infos basic-gui-scene-info)
                                       material-names (some-> material-infos keys)]
                                   (wrap-layout-property-edit-type material (optional-gui-resource-choicebox material-names)))))
            (dynamic error (g/fnk [_node-id basic-gui-scene-info material texture]
                             (let [material-infos (:material-infos basic-gui-scene-info)
                                   texture-page-counts (:texture-page-counts basic-gui-scene-info)]
                               (or (validate-material-resource _node-id material-infos material)
                                   (validate-material-capabilities _node-id material-infos material texture-page-counts texture)))))
            (value (layout-property-getter material))
            (set (layout-property-setter material)))
  (property texture g/Str (default (protobuf/default Gui$NodeDesc :texture))
            (value (layout-property-getter texture))
            (set (layout-property-setter texture))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [texture-page-counts (:texture-page-counts basic-gui-scene-info)
                                       texture-names (some-> texture-page-counts keys)]
                                   (wrap-layout-property-edit-type texture (optional-gui-resource-choicebox texture-names) texture-property-changes-fn))))
            (dynamic error (g/fnk [_node-id basic-gui-scene-info texture]
                             (let [texture-page-counts (:texture-page-counts basic-gui-scene-info)]
                               (validate-texture-resource _node-id texture-page-counts texture))))
            (dynamic label (properties/label-dynamic :gui :texture))
            (dynamic tooltip (properties/tooltip-dynamic :gui :texture)))

  (property clipping-mode g/Keyword (default (protobuf/default Gui$NodeDesc :clipping-mode))
            (dynamic edit-type (layout-property-edit-type clipping-mode (properties/->pb-choicebox Gui$NodeDesc$ClippingMode)))
            (dynamic label (properties/label-dynamic :gui :clipping-mode))
            (dynamic tooltip (properties/tooltip-dynamic :gui :clipping-mode))
            (value (layout-property-getter clipping-mode))
            (set (layout-property-setter clipping-mode)))
  (property clipping-visible g/Bool (default (protobuf/default Gui$NodeDesc :clipping-visible))
            (dynamic edit-type (layout-property-edit-type clipping-visible {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :clipping-visible))
            (dynamic tooltip (properties/tooltip-dynamic :gui :clipping-visible))
            (value (layout-property-getter clipping-visible))
            (set (layout-property-setter clipping-visible)))
  (property clipping-inverted g/Bool (default (protobuf/default Gui$NodeDesc :clipping-inverted))
            (dynamic edit-type (layout-property-edit-type clipping-inverted {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :clipping-inverted))
            (dynamic tooltip (properties/tooltip-dynamic :gui :clipping-inverted))
            (value (layout-property-getter clipping-inverted))
            (set (layout-property-setter clipping-inverted)))

  (output shape-base-node-msg g/Any produce-shape-base-node-msg)
  (output aabb g/Any :cached (g/fnk [pivot size] (calc-aabb pivot size)))
  (output anim-data g/Any
          (g/fnk [costly-gui-scene-info texture]
            (let [texture-infos (:texture-infos costly-gui-scene-info)
                  texture-info (get texture-infos texture)]
              (:anim-data texture-info))))
  (output gpu-texture TextureLifecycle (g/fnk [costly-gui-scene-info texture]
                                         (let [texture-gpu-textures (:texture-gpu-textures costly-gui-scene-info)]
                                           (or (get texture-gpu-textures texture)
                                               @texture/white-pixel))))
  (output size types/Vec3 (g/fnk [manual-size size-mode texture texture-size]
                            (if (or (= :size-mode-manual size-mode)
                                    (coll/empty? texture))
                              manual-size
                              texture-size)))
  (output build-errors-shape-node g/Any
          (g/fnk [_node-id basic-gui-scene-info build-errors-visual-node material texture]
            (let [material-infos (:material-infos basic-gui-scene-info)
                  texture-page-counts (:texture-page-counts basic-gui-scene-info)]
              (g/package-errors
                _node-id
                build-errors-visual-node ; Validates material name.
                (validate-texture-resource _node-id texture-page-counts texture)
                (validate-material-capabilities _node-id material-infos material texture-page-counts texture)))))
  (output own-build-errors g/Any (gu/passthrough build-errors-shape-node)))

(defmethod update-gui-resource-reference [::ShapeNode :texture]
  [_ evaluation-context node-id old-name new-name]
  (update-namespaced-gui-resource-reference evaluation-context node-id :texture old-name new-name))

(defmethod update-gui-resource-reference [::VisualNode :material]
  [_ evaluation-context node-id old-name new-name]
  (update-basic-gui-resource-reference evaluation-context node-id :material old-name new-name))

;; Box nodes

(g/defnk produce-box-node-msg [shape-base-node-msg ^:raw slice9]
  (merge shape-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :slice9 slice9)))

(g/defnode BoxNode
  (inherits ShapeNode)

  (property slice9 types/Vec4 (default (protobuf/default Gui$NodeDesc :slice9))
            (dynamic read-only? (g/fnk [size-mode] (not= :size-mode-manual size-mode)))
            (dynamic edit-type (layout-property-edit-type slice9 {:type types/Vec4 :labels ["L" "T" "R" "B"]}))
            (dynamic label (properties/label-dynamic :gui :slice9))
            (dynamic tooltip (properties/tooltip-dynamic :gui :slice9))
            (value (layout-property-getter slice9))
            (set (layout-property-setter slice9)))

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

(g/defnk produce-pie-node-msg [shape-base-node-msg ^:raw outer-bounds ^:raw inner-radius ^:raw perimeter-vertices ^:raw pie-fill-angle]
  (merge shape-base-node-msg
         (protobuf/make-map-without-defaults Gui$NodeDesc
           :outer-bounds outer-bounds
           :inner-radius inner-radius
           :perimeter-vertices perimeter-vertices
           :pie-fill-angle pie-fill-angle)))

(g/defnode PieNode
  (inherits ShapeNode)

  (property outer-bounds g/Keyword (default (protobuf/default Gui$NodeDesc :outer-bounds))
            (dynamic edit-type (layout-property-edit-type outer-bounds (properties/->pb-choicebox Gui$NodeDesc$PieBounds)))
            (dynamic label (properties/label-dynamic :gui :outer-bounds))
            (dynamic tooltip (properties/tooltip-dynamic :gui :outer-bounds))
            (value (layout-property-getter outer-bounds))
            (set (layout-property-setter outer-bounds)))
  (property inner-radius g/Num (default (protobuf/default Gui$NodeDesc :inner-radius))
            (dynamic edit-type (layout-property-edit-type inner-radius {:type g/Num}))
            (dynamic label (properties/label-dynamic :gui :inner-radius))
            (dynamic tooltip (properties/tooltip-dynamic :gui :inner-radius))
            (value (layout-property-getter inner-radius))
            (set (layout-property-setter inner-radius)))
  (property perimeter-vertices g/Int (default (protobuf/default Gui$NodeDesc :perimeter-vertices))
            (dynamic error (g/fnk [_node-id perimeter-vertices] (validate-perimeter-vertices _node-id perimeter-vertices)))
            (dynamic edit-type (layout-property-edit-type perimeter-vertices {:type g/Int
                                                                              :min perimeter-vertices-min
                                                                              :max perimeter-vertices-max}))
            (dynamic label (properties/label-dynamic :gui :perimeter-vertices))
            (dynamic tooltip (properties/tooltip-dynamic :gui :perimeter-vertices))
            (value (layout-property-getter perimeter-vertices))
            (set (layout-property-setter perimeter-vertices)))
  (property pie-fill-angle g/Num (default (protobuf/default Gui$NodeDesc :pie-fill-angle))
            (dynamic edit-type (layout-property-edit-type pie-fill-angle {:type g/Num}))
            (dynamic label (properties/label-dynamic :gui :pie-fill-angle))
            (dynamic tooltip (properties/tooltip-dynamic :gui :pie-fill-angle))
            (value (layout-property-getter pie-fill-angle))
            (set (layout-property-setter pie-fill-angle)))

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

(g/defnk produce-text-node-msg [visual-base-node-msg ^:raw manual-size ^:raw text ^:raw line-break ^:raw font ^:raw text-leading ^:raw text-tracking ^:raw outline ^:raw outline-alpha ^:raw shadow ^:raw shadow-alpha]
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
            (dynamic label (properties/label-dynamic :size))
            (dynamic tooltip (properties/tooltip-dynamic :size))
            (dynamic edit-type (layout-property-edit-type manual-size {:type types/Vec3}))
            (value (layout-property-getter manual-size))
            (set (layout-property-setter manual-size)))
  (property text g/Str (default (protobuf/default Gui$NodeDesc :text))
            (dynamic edit-type (layout-property-edit-type text {:type :multi-line-text}))
            (dynamic label (properties/label-dynamic :gui :text))
            (dynamic tooltip (properties/tooltip-dynamic :gui :text))
            (value (layout-property-getter text))
            (set (layout-property-setter text)))
  (property line-break g/Bool (default (protobuf/default Gui$NodeDesc :line-break))
            (dynamic edit-type (layout-property-edit-type line-break {:type g/Bool}))
            (dynamic label (properties/label-dynamic :gui :line-break))
            (dynamic tooltip (properties/tooltip-dynamic :gui :line-break))
            (value (layout-property-getter line-break))
            (set (layout-property-setter line-break)))
  (property font g/Str (default (protobuf/default Gui$NodeDesc :font))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [font-names (:font-names basic-gui-scene-info)]
                                   (wrap-layout-property-edit-type font (required-gui-resource-choicebox font-names)))))
            (dynamic error (g/fnk [_node-id basic-gui-scene-info font]
                             (let [font-names (:font-names basic-gui-scene-info)]
                               (validate-font _node-id font-names font))))
            (dynamic label (properties/label-dynamic :gui :font))
            (dynamic tooltip (properties/tooltip-dynamic :gui :font))
            (value (layout-property-getter font))
            (set (layout-property-setter font)))
  (property text-leading g/Num (default (protobuf/default Gui$NodeDesc :text-leading))
            (dynamic edit-type (layout-property-edit-type text-leading {:type g/Num}))
            (dynamic label (properties/label-dynamic :gui :text-leading))
            (dynamic tooltip (properties/tooltip-dynamic :gui :text-leading))
            (value (layout-property-getter text-leading))
            (set (layout-property-setter text-leading)))
  (property text-tracking g/Num (default (protobuf/default Gui$NodeDesc :text-tracking))
            (dynamic edit-type (layout-property-edit-type text-tracking {:type g/Num}))
            (dynamic label (properties/label-dynamic :gui :text-tracking))
            (dynamic tooltip (properties/tooltip-dynamic :gui :text-tracking))
            (value (layout-property-getter text-tracking))
            (set (layout-property-setter text-tracking)))
  (property outline types/Color (default (protobuf/default Gui$NodeDesc :outline))
            (dynamic edit-type (layout-property-edit-type outline {:type types/Color
                                                                   :ignore-alpha? true}))
            (dynamic label (properties/label-dynamic :gui :outline))
            (dynamic tooltip (properties/tooltip-dynamic :gui :outline))
            (value (layout-property-getter outline))
            (set (layout-property-setter outline)))
  (property outline-alpha g/Num (default (protobuf/default Gui$NodeDesc :outline-alpha))
            (dynamic edit-type (layout-property-edit-type outline-alpha {:type :slider
                                                                         :min 0.0
                                                                         :max 1.0
                                                                         :precision 0.01}))
            (dynamic label (properties/label-dynamic :gui :outline-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :gui :outline-alpha))
            (value (layout-property-getter outline-alpha))
            (set (layout-property-setter outline-alpha)))
  (property shadow types/Color (default (protobuf/default Gui$NodeDesc :shadow))
            (dynamic edit-type (layout-property-edit-type shadow {:type types/Color
                                                                  :ignore-alpha? true}))
            (dynamic label (properties/label-dynamic :gui :shadow))
            (dynamic tooltip (properties/tooltip-dynamic :gui :shadow))
            (value (layout-property-getter shadow))
            (set (layout-property-setter shadow)))
  (property shadow-alpha g/Num (default (protobuf/default Gui$NodeDesc :shadow-alpha))
            (dynamic edit-type (layout-property-edit-type shadow-alpha {:type :slider
                                                                        :min 0.0
                                                                        :max 1.0
                                                                        :precision 0.01}))
            (dynamic label (properties/label-dynamic :gui :shadow-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :gui :shadow-alpha))
            (value (layout-property-getter shadow-alpha))
            (set (layout-property-setter shadow-alpha)))

  (display-order (into base-display-order [:manual-size :enabled :visible :text :line-break :font :material :color :alpha :inherit-alpha :text-leading :text-tracking :outline :outline-alpha :shadow :shadow-alpha :layer]))

  (output font-data font/FontData (g/fnk [costly-gui-scene-info font]
                                    (let [font-datas (:font-datas costly-gui-scene-info)]
                                      (or (get font-datas font)
                                          (get font-datas "")))))

  ;; Overloaded outputs
  (output node-msg g/Any :cached produce-text-node-msg)
  (output gpu-texture TextureLifecycle (g/fnk [font-data] (:texture font-data)))
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [costly-gui-scene-info manual-size font material material-shader pivot text-data color+alpha]
                 (let [[w h] manual-size
                       offset (pivot-offset pivot manual-size)
                       lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))
                       font-map (get-in text-data [:font-data :font-map])
                       texture-recip-uniform (font/get-texture-recip-uniform font-map)
                       material-shader (when (not (empty? material)) material-shader)
                       font-shaders (:font-shaders costly-gui-scene-info)
                       font-shader (or material-shader (get font-shaders font) (get font-shaders ""))
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
  (output own-build-errors g/Any
          (g/fnk [_node-id basic-gui-scene-info build-errors-visual-node font]
            (let [font-names (:font-names basic-gui-scene-info)]
              (g/package-errors
                _node-id
                build-errors-visual-node
                (validate-font _node-id font-names font))))))

(defmethod update-gui-resource-reference [::TextNode :font]
  [_ evaluation-context node-id old-name new-name]
  (update-basic-gui-resource-reference evaluation-context node-id :font old-name new-name))

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

(def ^:private override-gui-node-init-props {:layout->prop->override {}})

(defn- contains-resource?
  [project gui-scene resource evaluation-context]
  (let [workspace (project/workspace project evaluation-context)
        acc-fn (fn [target-node]
                 (->> (g/node-value target-node :node-msgs evaluation-context)
                      (filter #(= :type-template (:type %)))
                      (keep #(workspace/resolve-workspace-resource workspace (:template %) evaluation-context))))]
    (project/node-refers-to-resource? project gui-scene resource acc-fn)))

(g/defnode TemplateNode
  (inherits GuiNode)

  (property template TemplateData
            (dynamic read-only? override-node?)
            (dynamic edit-type (g/fnk [_node-id] {:type resource/Resource
                                                  :ext "gui"
                                                  :dialog-accept-fn (fn [r]
                                                                      (g/with-auto-evaluation-context evaluation-context
                                                                        (let [basis (:basis evaluation-context)
                                                                              gui-scene (node->gui-scene basis _node-id)
                                                                              project (project/get-project basis gui-scene)]
                                                                          (not (contains-resource? project gui-scene r evaluation-context)))))
                                                  :to-type (fn [v] (:resource v))
                                                  :from-type (fn [r] {:resource r :overrides {}})}))
            (dynamic error (g/fnk [_node-id template-resource]
                             (prop-resource-error _node-id :template template-resource "Template")))
            (dynamic label (properties/label-dynamic :gui :template))
            (dynamic tooltip (properties/tooltip-dynamic :gui :template))
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
                                               (into {}
                                                     (keep (fn [[id node-id]]
                                                             (when-some [properties (coll/not-empty (properties-by-id id))]
                                                               (pair node-id properties))))
                                                     node-ids-by-id)))))
                                       {})]
                               ;; TODO: Check if we can filter based on connection label instead of source-node-id to be able to share :traverse-fn with other overrides.
                               (g/override scene-node {:traverse-fn (g/make-override-traverse-fn
                                                                      (fn [basis ^Arc arc]
                                                                        (let [source-node-id (.source-id arc)]
                                                                          (and (not= source-node-id current-scene)
                                                                               (g/node-instance-match basis source-node-id
                                                                                                      [GuiNode
                                                                                                       NodeTree
                                                                                                       GuiSceneNode])))))
                                                       :init-props-fn (fn init-props-fn [_basis _node-id node-type]
                                                                        (when (in/inherits? node-type GuiNode)
                                                                          override-gui-node-init-props))
                                                       :properties-by-node-id properties-by-node-id}
                                           (fn [_evaluation-context id-mapping]
                                             (let [or-scene (get id-mapping scene-node)]
                                               (concat
                                                 (for [[from to] [[:node-ids :node-ids]
                                                                  [:node-outline :template-outline]
                                                                  [:scene :template-scene]
                                                                  [:build-targets :template-build-targets]
                                                                  [:build-errors :child-build-errors]
                                                                  [:resource :template-resource]
                                                                  [:node-msgs :scene-node-msgs]
                                                                  [:node-overrides :template-overrides]]]
                                                   (g/connect or-scene from self to))
                                                 (for [[from to] [[:template-trivial-gui-scene-info :aux-trivial-gui-scene-info]
                                                                  [:basic-gui-scene-info :aux-basic-gui-scene-info]
                                                                  [:costly-gui-scene-info :aux-costly-gui-scene-info]]]
                                                   (g/connect self from or-scene to)))))))))))))))

  (display-order (into base-display-order [:enabled :template]))

  (input scene-node-msgs g/Any :substitute [])
  (input scene-build-targets g/Any)
  (output scene-build-targets g/Any (gu/passthrough scene-build-targets))

  (input template-resource resource/Resource :cascade-delete)
  (input template-outline outline/OutlineData :substitute template-outline-subst)
  (input template-scene g/Any)
  (input template-overrides g/Any)

  (output template-trivial-gui-scene-info TrivialGuiSceneInfo
          (g/fnk [id trivial-gui-scene-info]
            (assoc trivial-gui-scene-info
              :id-prefix (str id "/"))))

  ; Overloaded outputs
  (output node-outline-link resource/Resource (gu/passthrough template-resource))
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [template-outline]
                                                                (get-in template-outline [:children 0 :children])))
  (output node-outline-reqs g/Any :cached (g/constantly []))
  (output node-msg g/Any :cached produce-template-node-msg)
  (output node-msgs g/Any :cached (g/fnk [id layout->prop->override layout->prop->value node-msg scene-node-msgs]
                                    (let [decorated-node-msg
                                          (assoc node-msg
                                            :layout->prop->override layout->prop->override
                                            :layout->prop->value layout->prop->value)]
                                      (into [decorated-node-msg]
                                            (map #(-> %
                                                      (vary-meta update :templates (fnil conj []) decorated-node-msg)
                                                      (assoc :template-node-child true)
                                                      (cond-> (empty? (:parent %)) (assoc :parent id))))
                                            scene-node-msgs))))
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

(g/defnk produce-particlefx-node-msg [visual-base-node-msg ^:raw particlefx]
  (-> visual-base-node-msg
      (merge (protobuf/make-map-without-defaults Gui$NodeDesc
               :particlefx particlefx
               :size-mode :size-mode-auto))))


(g/defnode ParticleFXNode
  (inherits VisualNode)

  (property particlefx g/Str (default (protobuf/default Gui$NodeDesc :particlefx))
            (dynamic edit-type (g/fnk [basic-gui-scene-info]
                                 (let [particlefx-resource-names (:particlefx-resource-names basic-gui-scene-info)]
                                   (wrap-layout-property-edit-type particlefx (required-gui-resource-choicebox particlefx-resource-names)))))
            (dynamic error (g/fnk [_node-id particlefx basic-gui-scene-info]
                             (let [particlefx-resource-names (:particlefx-resource-names basic-gui-scene-info)]
                               (validate-particlefx-resource _node-id particlefx-resource-names particlefx))))
            (dynamic label (properties/label-dynamic :gui :particlefx))
            (dynamic tooltip (properties/tooltip-dynamic :gui :particlefx))
            (value (layout-property-getter particlefx))
            (set (layout-property-setter particlefx)))
  (property blend-mode g/Keyword (default (protobuf/default Gui$NodeDesc :blend-mode))
            (dynamic visible (g/constantly false))
            (dynamic edit-type (layout-property-edit-type blend-mode (properties/->pb-choicebox Gui$NodeDesc$BlendMode)))
            (value (layout-property-getter blend-mode))
            (set (layout-property-setter blend-mode)))
  (property pivot g/Keyword (default (protobuf/default Gui$NodeDesc :pivot))
            (dynamic visible (g/constantly false))
            (dynamic edit-type (layout-property-edit-type pivot (properties/->pb-choicebox Gui$NodeDesc$Pivot)))
            (dynamic label (properties/label-dynamic :gui :pivot))
            (dynamic tooltip (properties/tooltip-dynamic :gui :pivot))
            (value (layout-property-getter pivot))
            (set (layout-property-setter pivot)))

  (display-order (into base-display-order
                       [:enabled :visible :particlefx :material :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output node-msg g/Any :cached produce-particlefx-node-msg)
  (output source-scene g/Any :cached (g/fnk [_node-id costly-gui-scene-info id particlefx child-index layer-index material-shader color+alpha]
                                            (when-let [source-scene (get-in costly-gui-scene-info [:particlefx-infos particlefx :particlefx-scene])]
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
  (output own-build-errors g/Any
          (g/fnk [_node-id basic-gui-scene-info build-errors-visual-node particlefx]
            (let [particlefx-resource-names (:particlefx-resource-names basic-gui-scene-info)]
              (g/package-errors
                _node-id
                build-errors-visual-node
                (validate-particlefx-resource _node-id particlefx-resource-names particlefx))))))

(defmethod update-gui-resource-reference [::ParticleFXNode :particlefx]
  [_ evaluation-context node-id old-name new-name]
  (update-basic-gui-resource-reference evaluation-context node-id :particlefx old-name new-name))

(g/defnk produce-texture-gpu-textures [_node-id texture-names gpu-texture default-tex-params samplers]
  ;; If the referenced texture-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-texture entry for "".
  (when (some? gpu-texture)
    (let [gpu-texture (let [params (material/sampler->tex-params (first samplers) default-tex-params)]
                        (texture/set-params gpu-texture params))]
      (into {}
            (map (fn [texture-name]
                   (pair texture-name gpu-texture)))
            texture-names))))

(definline ^:private make-texture-name [texture-name anim-id]
  `(str ~texture-name \/ ~anim-id))

(g/defnk produce-texture-infos [anim-data texture-names name]
  ;; The anim-data may be nil if the referenced texture is missing or defective.
  (let [has-animations (not (coll/empty? anim-data))]
    ;; If the texture does not contain animations, we emit an entry for the
    ;; "texture" name only.
    (if-not has-animations
      (sorted-map name {})
      (into (sorted-map)
            (map (fn [[anim-id anim-data]]
                   ;; Use the existing texture-name string to save memory.
                   (let [texture-name (if-not anim-id
                                        name
                                        (let [texture-name (make-texture-name name anim-id)]
                                          (get texture-names texture-name texture-name)))
                         texture-info {:anim-data anim-data}]
                     (pair texture-name texture-info))))
            anim-data))))

(g/defnk produce-texture-names [anim-ids name]
  ;; If the texture does not contain animations, we emit an entry for the
  ;; "texture" name only.
  (if (coll/empty? anim-ids)
    (sorted-set name)
    (into (sorted-set)
          (map (fn [anim-id]
                 (make-texture-name name anim-id)))
          anim-ids)))

(g/defnk produce-texture-page-counts [texture-names texture-page-count]
  ;; The texture-page-count can be nil, which will disable validation against
  ;; the material page count. We still want an entry in the map since the keys
  ;; are used to validate the texture names.
  (into {}
        (map (fn [texture-name]
               (pair texture-name texture-page-count)))
        texture-names))

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
            (dynamic label (properties/label-dynamic :gui :texture))
            (dynamic tooltip (properties/tooltip-dynamic :gui :texture))
            (dynamic edit-type (g/fnk [_node-id]
                                 {:type resource/Resource
                                  :ext (workspace/resource-kind-extensions (project/workspace (project/get-project _node-id)) :atlas)})))

  (input name-counts NameCounts)
  (input default-tex-params g/Any)
  (input texture-resource resource/Resource)
  (input image BufferedImage :substitute nil)
  (input gpu-texture g/Any :substitute nil)
  (input texture-page-count g/Int :substitute nil)
  (input anim-data g/Any :substitute nil)
  (input anim-ids g/Any :substitute nil)
  (input samplers [g/KeywordMap] :substitute (constantly []))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name texture-resource build-errors]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key name
                                                              :label name
                                                              :icon texture-icon
                                                              :outline-error? (g/error-fatal? build-errors)}
                                                             (resource/resource? texture-resource) (assoc :link texture-resource :outline-show-link? true))))
  (output pb-msg g/Any (g/fnk [name texture-resource]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$TextureDesc
                           :name name
                           :texture (resource/resource->proj-path texture-resource))))
  (output texture-gpu-textures GuiResourceTextures :cached produce-texture-gpu-textures)
  (output texture-infos GuiResourceTextureInfos :cached produce-texture-infos)
  (output texture-names NameSet :cached produce-texture-names)
  (output texture-page-counts GuiResourcePageCounts :cached produce-texture-page-counts)
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
                                                             (resource/resource? font-resource) (assoc :link font-resource :outline-show-link? true))))
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
  (output material-infos GuiResourceMaterialInfos (g/fnk [name material-max-page-count material-shader]
                                                    ;; If the referenced material-resource is missing, material-max-page-count and material-shader will be nil.
                                                    ;; We still want an entry in the map, but we will not attempt to validate the page count against the texture.
                                                    ;; TODO: Depending on the material-shader here to figure out if the material is paged is a bit costly.
                                                    (let [is-paged-material (and (some? material-shader)
                                                                                 (shader/is-using-array-samplers? material-shader))
                                                          material-info (cond-> {:paged is-paged-material}
                                                                                material-max-page-count (assoc :max-page-count material-max-page-count))]
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
            (dynamic label (properties/label-dynamic :gui :particlefx))
            (dynamic tooltip (properties/tooltip-dynamic :gui :particlefx))
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
                                                             (resource/resource? particlefx-resource) (assoc :link particlefx-resource :outline-show-link? true))))
  (output pb-msg g/Any (g/fnk [name particlefx]
                         (protobuf/make-map-without-defaults Gui$SceneDesc$ParticleFXDesc
                           :name name
                           :particlefx (resource/resource->proj-path particlefx))))
  (output particlefx-infos ParticleFXInfos (g/fnk [name particlefx-scene]
                                             {name {:particlefx-scene particlefx-scene}}))
  (output build-errors g/Any (g/fnk [_node-id name name-counts particlefx]
                               (g/package-errors _node-id
                                                 (prop-unique-id-error _node-id :name name name-counts "Name")
                                                 (prop-resource-error _node-id :particlefx particlefx "Particle FX")))))

(g/defnode LayoutNode
  (inherits outline/OutlineNode)
  (property name g/Str ; Required protobuf field.
            (dynamic read-only? (g/constantly true))
            (dynamic error (g/fnk [_node-id name name-counts] (prop-unique-id-error _node-id :name name name-counts "Name"))))
  (input name-counts NameCounts)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name build-errors]
                                                          {:node-id _node-id
                                                           :node-outline-key name
                                                           :label name
                                                           :icon layout-icon
                                                           :outline-error? (g/error-fatal? build-errors)}))
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

  (input trivial-gui-scene-info TrivialGuiSceneInfo)
  (output trivial-gui-scene-info TrivialGuiSceneInfo (gu/passthrough trivial-gui-scene-info))
  (input basic-gui-scene-info BasicGuiSceneInfo)
  (output basic-gui-scene-info BasicGuiSceneInfo (gu/passthrough basic-gui-scene-info))
  (input costly-gui-scene-info CostlyGuiSceneInfo)
  (output costly-gui-scene-info CostlyGuiSceneInfo (gu/passthrough costly-gui-scene-info))

  (input child-scenes g/Any :array)
  (input child-indices NodeIndex :array)
  (output child-scenes g/Any (g/fnk [child-scenes] (vec (sort-by (comp :child-index :renderable) child-scenes))))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk (localization/message "outline.gui.nodes") nil 0 true
                           (mapv (fn [type-info]
                                   {:node-type (:node-cls type-info)
                                    :tx-attach-fn gui-node-attach-fn})
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
  (input node-ids NameNodeIds :array)
  (output node-ids NameNodeIds :cached (g/fnk [node-ids] (reduce coll/merge {} node-ids)))

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
  (when-let [resources (browse
                         (localization/message "dialog.select-gui-component.title" {"component" resources-type-label})
                         project
                         resource-exts)]
    (let [names (id/resolve-all (map resource->id resources) taken-ids)
          pairs (map vector resources names)
          op-seq (gensym)
          op-label (localization/message "operation.gui.add-resources" {"type" resources-type-label})
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

(defn- attach-texture [self textures-node texture]
  (concat
    (g/connect texture :_node-id textures-node :nodes)
    (g/connect texture :texture-gpu-textures self :texture-gpu-textures)
    (g/connect texture :texture-infos self :texture-infos)
    (g/connect texture :dep-build-targets self :dep-build-targets)
    (g/connect texture :pb-msg self :texture-msgs)
    (g/connect texture :build-errors textures-node :build-errors)
    (g/connect texture :node-outline textures-node :child-outlines)
    (g/connect texture :texture-page-counts textures-node :texture-page-counts)
    (g/connect texture :name textures-node :names)
    (g/connect textures-node :name-counts texture :name-counts)
    (g/connect self :samplers texture :samplers)
    (g/connect self :default-tex-params texture :default-tex-params)))

(defn add-texture [scene textures-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [TextureNode :name name :texture resource]]
                (attach-texture scene textures-node node)))

(defn- add-textures-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
   "Textures" (workspace/resource-kind-extensions (project/workspace project) :atlas) (g/node-value parent :name-counts) project select-fn
   (partial add-texture scene parent)))

(g/defnode TexturesNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (output texture-resource-names GuiResourceNames :cached (g/fnk [names] (into (sorted-set) names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (input texture-page-counts g/Any :array)
  (output texture-page-counts g/Any :cached (g/fnk [texture-page-counts]
                                              (into {} cat texture-page-counts)))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk (localization/message "outline.gui.textures") "Textures" 1 false
                           [{:node-type TextureNode
                             :tx-attach-fn (gen-outline-node-tx-attach-fn attach-texture)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Textures..." texture-icon add-textures-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-material [self materials-node material]
  (concat
    (g/connect material :_node-id materials-node :nodes)
    (g/connect material :material-shaders self :material-shaders)
    (g/connect material :material-infos self :material-infos)
    (g/connect material :dep-build-targets self :dep-build-targets)
    (g/connect material :pb-msg self :material-msgs)
    (g/connect material :build-errors materials-node :build-errors)
    (g/connect material :node-outline materials-node :child-outlines)
    (g/connect material :name materials-node :names)
    (g/connect materials-node :name-counts material :name-counts)))

(defn add-material [scene materials-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [MaterialNode :name name :material resource]]
    (attach-material scene materials-node node)))

(defn- add-materials-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
    "Materials" ["material"] (g/node-value parent :name-counts) project select-fn
    (partial add-material scene parent)))

(g/defnode MaterialsNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk (localization/message "outline.gui.materials") "Materials" 1 false
                           [{:node-type MaterialNode
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
       (g/connect font :name self :font-names)
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
          (gen-outline-fnk (localization/message "outline.gui.fonts") "Fonts" 2 false
                           [{:node-type FontNode
                             :tx-attach-fn (gen-outline-node-tx-attach-fn attach-font)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Fonts..." font-icon add-fonts-handler {}])))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-layer
  ;; Self is not used here but added to conform all attach-*** functions
  [_self layers-node layer]
  (concat
    (g/connect layer :_node-id layers-node :nodes)
    (g/connect layer :pb-msg layers-node :layer-msgs)
    (g/connect layer :build-errors layers-node :build-errors)
    (g/connect layer :node-outline layers-node :child-outlines)
    (g/connect layer :node-id+child-index layers-node :child-indices)
    (g/connect layer :name+child-index layers-node :indexed-layer-names)
    (g/connect layers-node :name-counts layer :name-counts)))

(defn add-layer [scene parent name child-index select-fn]
  (g/make-nodes (g/node-id->graph-id scene) [node [LayerNode :name name :child-index child-index]]
    (attach-layer scene parent node)
    (when select-fn
      (select-fn [node]))))

(defn- add-layer-handler [_project {:keys [scene parent]} select-fn]
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (let [name (id/gen "layer" (g/node-value parent :name-counts evaluation-context))
            next-index (gui-attachment/next-child-index parent evaluation-context)]
        (concat
          (g/operation-label (localization/message "operation.gui.add-layer"))
          (add-layer scene parent name next-index select-fn))))))

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

  (output layer->index NameIndices :cached
          (g/fnk [ordered-layer-names]
            (coll/transfer ordered-layer-names {}
              (map-indexed coll/flipped-pair))))
  (input child-indices NodeIndex :array)
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk (localization/message "outline.gui.layers") "Layers" 3 true
                           [{:node-type LayerNode
                             :tx-attach-fn (gen-outline-node-tx-attach-fn attach-layer :ordered-layer-names)}]))
  (output add-handler-info g/Any
          (g/fnk [_node-id]
                 [_node-id "Layer" layer-icon add-layer-handler {}])))


;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-layout [_self layouts-node layout]
  (concat
   (g/connect layout :build-errors layouts-node :build-errors)
   (g/connect layout :node-outline layouts-node :child-outlines)
   (g/connect layout :name layouts-node :names)
   (g/connect layouts-node :name-counts layout :name-counts)
   (g/connect layout :_node-id layouts-node :nodes)))

(defn add-layout-handler [project {:keys [scene parent display-profile]} select-fn]
  (g/transact
   (concat
    (g/operation-label (localization/message "operation.gui.add-layout"))
    (g/make-nodes (g/node-id->graph-id scene) [node [LayoutNode :name display-profile]]
                  (attach-layout scene parent node)
                  (when select-fn
                    (select-fn [node]))))))

(g/defnode LayoutsNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output names GuiResourceNames :cached (g/fnk [names] (into (sorted-set) names)))
  (input unused-display-profiles g/Any)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          ;; Layouts don't have any child-reqs for the outline copy/pasting,
          ;; since there is essentially only one node that _can_ be supported
          ;; per "layout type".
          (gen-outline-fnk (localization/message "outline.gui.layouts") "Layouts" 4 false []))
  (output add-handler-info g/Any
          (g/fnk [_node-id unused-display-profiles]
            (mapv #(vector _node-id % layout-icon add-layout-handler {:display-profile %})
                  unused-display-profiles))))

;; //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

(defn- attach-particlefx-resource [self particlefx-resources-node particlefx-resource]
  (concat
    (g/connect particlefx-resource :_node-id particlefx-resources-node :nodes)
    (g/connect particlefx-resource :particlefx-infos self :particlefx-infos)
    (g/connect particlefx-resource :name self :particlefx-resource-names)
    (g/connect particlefx-resource :dep-build-targets self :dep-build-targets)
    (g/connect particlefx-resource :pb-msg self :particlefx-resource-msgs)
    (g/connect particlefx-resource :build-errors particlefx-resources-node :build-errors)
    (g/connect particlefx-resource :node-outline particlefx-resources-node :child-outlines)
    (g/connect particlefx-resource :name particlefx-resources-node :names)
    (g/connect particlefx-resources-node :name-counts particlefx-resource :name-counts)))

(defn add-particlefx-resource [scene particlefx-resources-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [ParticleFXResource :name name :particlefx resource]]
                (attach-particlefx-resource scene particlefx-resources-node node)))

(defn- add-particlefx-resources-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
   "Particle FX" [particlefx/particlefx-ext] (g/node-value parent :name-counts) project select-fn
   (partial add-particlefx-resource scene parent)))

(g/defnode ParticleFXResources
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (input names g/Str :array)
  (output name-counts NameCounts :cached (g/fnk [names] (frequencies names)))
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk (localization/message "outline.particlefx") "Particle FX" 5 false
                           [{:node-type ParticleFXResource
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

(defn- make-layout-desc [layout-name decorated-node-msgs]
  (protobuf/make-map-without-defaults Gui$SceneDesc$LayoutDesc
    :name layout-name
    :nodes (coll/transfer decorated-node-msgs []
             (keep
               (fn [{:keys [layout->prop->override] :as decorated-node-msg}]
                 {:pre [(map? layout->prop->override)]}
                 (when-some [prop->override (coll/not-empty (layout->prop->override layout-name))]
                   (let [overridden-fields
                         (->> prop->override
                              (map (comp prop-key->pb-field-index key))
                              (sort))]
                     (->> prop->override
                          (e/map prop-entry->pb-field-entry)
                          (reduce
                            (fn [node-desc [pb-field pb-value]]
                              (let [default-pb-value (default-node-desc-pb-field-values pb-field)]
                                (if (= default-pb-value pb-value)
                                  (dissoc node-desc pb-field)
                                  (assoc node-desc pb-field pb-value))))
                            (-> decorated-node-msg
                                (dissoc :layout->prop->override :layout->prop->value)
                                (protobuf/assign-repeated :overridden-fields overridden-fields)))
                          (strip-unused-overridden-fields-from-node-desc)))))))))

(g/defnk produce-pb-msg [script-resource material-resource adjust-reference max-nodes max-dynamic-textures layer-msgs font-msgs texture-msgs material-msgs particlefx-resource-msgs resource-msgs]
  ;; Note: Both :layouts and :nodes are omitted here. They will be added to the
  ;; final Gui$SceneDesc when producing the save-value and build-targets,
  ;; because their contents differ between saving and building.
  (protobuf/make-map-without-defaults Gui$SceneDesc
    :script (resource/resource->proj-path script-resource)
    :material (resource/resource->proj-path material-resource)
    :adjust-reference adjust-reference
    :max-nodes max-nodes
    :max-dynamic-textures max-dynamic-textures
    :layers layer-msgs
    :fonts font-msgs
    :textures texture-msgs
    :materials material-msgs
    :particlefxs particlefx-resource-msgs
    :resources resource-msgs))

(g/defnk produce-save-value [layout-names node-msgs pb-msg]
  ;; Any Gui$NodeDescs in the resulting SceneDesc or its LayoutDescs should be
  ;; sparsely stored. Only field values that deviate from their originals should
  ;; be included, but ultimately it is the fields listed as :overridden-fields
  ;; that is the source of truth for whether a field shows up as overridden
  ;; inside the editor. Note that a field may be among the :overridden-fields
  ;; without us writing a corresponding value for it to the file in case its
  ;; overridden value matches the protobuf field default.
  (let [layout-descs
        (mapv #(make-layout-desc % node-msgs)
              layout-names)

        node-descs
        (coll/transfer node-msgs []
          (map #(dissoc % :layout->prop->override :layout->prop->value))
          (map (fn [node-desc]
                 (if (:template-node-child node-desc)
                   (strip-unused-overridden-fields-from-node-desc node-desc)
                   (strip-redundant-size-from-node-desc node-desc)))))]

    (protobuf/assign-repeated pb-msg
      :layouts layout-descs
      :nodes node-descs)))

(defn- build-pb [resource dep-resources user-data]
  (let [pb (transduce
             (map (fn [[label build-resource]]
                    {:pre [(or (nil? build-resource) (workspace/build-resource? build-resource))]}
                    (let [fused-build-resource (get dep-resources build-resource)
                          fused-build-resource-path (resource/resource->proj-path fused-build-resource)]
                      (pair label fused-build-resource-path))))
             (completing
               (fn [pb [label fused-build-resource-path]]
                 (if (vector? label)
                   (assoc-in pb label fused-build-resource-path)
                   (assoc pb label fused-build-resource-path))))
             (:pb user-data)
             (:dep-resources user-data))]
    {:resource resource :content (protobuf/map->bytes (:pb-class user-data) pb)}))

(defn- merge-rt-pb-msg [rt-pb-msg template-build-targets]
  (let [merge-fn! (fn [coll msg kw] (reduce conj! coll (e/map (coll/pair-fn :name) (get msg kw))))
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

(defn- node-desc->rt-node-desc [node-desc layout-name]
  {:pre [(map? node-desc) ; Gui$NodeDesc in map format.
         (string? layout-name)]}
  (cond
    (= :type-template (:type node-desc))
    nil ; Filtered out when building. We apply its properties to imported nodes.

    (:template-node-child node-desc)
    (reduce
      (fn [imported-node-desc decorated-template-node-msg]
        ;; The layout overrides have already been applied to the
        ;; imported-node-desc at this point, but we must also respect the
        ;; layout-specific values for any layout properties on the TemplateNode
        ;; that imported the node-desc. Non-layout properties like :id and
        ;; :parent are used directly from the decorated-template-node-msg.
        (let [layout->prop->value-for-template-node (:layout->prop->value decorated-template-node-msg)
              prop->value-for-template-node (layout->prop->value-for-template-node layout-name)]
          ;; Beware:
          ;; The prop->value map contains [prop-kw, prop-value] entries and
          ;; includes defaults. The node-desc is a map of [pb-field, pb-value]
          ;; with defaults stripped out. Refer to the property-conversions map
          ;; earlier in this file to determine if conversions must be performed
          ;; before combining values.
          (assert (map? layout->prop->value-for-template-node))
          (assert (map? prop->value-for-template-node))
          (cond-> imported-node-desc

                  (coll/empty? (:layer imported-node-desc))
                  (protobuf/assign :layer (coll/not-empty (prop->value-for-template-node :layer)))

                  (:inherit-alpha imported-node-desc)
                  (as-> imported-node-desc
                        (let [^float node-alpha (:alpha imported-node-desc protobuf/float-one)
                              ^float template-alpha (prop->value-for-template-node :alpha)
                              inherited-alpha (* node-alpha template-alpha)]
                          (protobuf/assign imported-node-desc
                            :inherit-alpha (when (prop->value-for-template-node :inherit-alpha)
                                             true) ; Protobuf default is false, and we want to exclude defaults.
                            :alpha (when (< inherited-alpha (float 1.0))
                                     inherited-alpha))))

                  (or (= (:id decorated-template-node-msg) (:parent imported-node-desc))
                      (coll/empty? (:parent imported-node-desc)))
                  (as-> imported-node-desc
                        ;; In fact incorrect, but only possibility to retain rotation/scale separation.
                        (let [template-node-pose
                              (pose/make
                                (pose/seq-translation (prop->value-for-template-node :position))
                                (pose/seq-rotation (prop->value-for-template-node :rotation)) ; Property value is a clj-quat.
                                (pose/seq-scale (prop->value-for-template-node :scale)))

                              imported-node-pose
                              (pose/make
                                (some-> (:position imported-node-desc) pose/seq-translation)
                                (some-> (:rotation imported-node-desc) pose/seq-euler-rotation) ; Protobuf value is a euler-v4.
                                (some-> (:scale imported-node-desc) pose/seq-scale))

                              baked-pose
                              (pose/pre-multiply imported-node-pose template-node-pose)]

                          (protobuf/assign imported-node-desc
                            :parent (:parent decorated-template-node-msg)
                            :enabled (when-not (and (:enabled imported-node-desc true)
                                                    (prop->value-for-template-node :enabled))
                                       false) ; Protobuf default is true, and we want to exclude defaults.
                            :position (when (pose/translated? baked-pose)
                                        (pose/translation-v4 baked-pose 1.0))
                            :rotation (when (pose/rotated? baked-pose)
                                        (pose/euler-rotation-v4 baked-pose))
                            :scale (when (pose/scaled? baked-pose)
                                     (pose/scale-v4 baked-pose))))))))
      (dissoc node-desc :overridden-fields :template-node-child)
      (:templates (meta node-desc)))

    :else
    (dissoc node-desc :overridden-fields)))

(defn- make-rt-layout-desc [layout-name decorated-node-msgs id->node-desc-for-default-layout]
  {:pre [(map? id->node-desc-for-default-layout)]}
  (protobuf/make-map-without-defaults Gui$SceneDesc$LayoutDesc
    :name layout-name
    :nodes (coll/transfer decorated-node-msgs []
             (keep
               (fn [{:keys [layout->prop->value] :as decorated-node-msg}]
                 {:pre [(map? layout->prop->value)]}
                 (let [prop->value (layout->prop->value layout-name)]
                   (assert (map? prop->value))
                   (when-some [node-desc-for-layout
                               (some->> (-> decorated-node-msg
                                            (dissoc :layout->prop->override :layout->prop->value)
                                            (into (map prop-entry->pb-field-entry)
                                                  prop->value)
                                            (node-desc->rt-node-desc layout-name))
                                        (protobuf/clear-defaults Gui$NodeDesc))]
                     (let [id (:id node-desc-for-layout)
                           node-desc-for-default-layout (id->node-desc-for-default-layout id)]
                       (assert (map? node-desc-for-default-layout))
                       (when (not= node-desc-for-default-layout node-desc-for-layout)
                         node-desc-for-layout)))))))))

(g/defnk produce-build-targets [_node-id build-errors resource pb-msg dep-build-targets template-build-targets layout-names node-msgs]
  ;; Built Gui$NodeDescs should be fully-formed, since no additional merging of
  ;; overridden properties will be done by the runtime. A corresponding NodeDesc
  ;; in a LayoutDesc will fully replace its original NodeDesc, if present in the
  ;; LayoutDesc. However, NodeDescs that are equivalent to their originals may
  ;; be omitted from the LayoutDesc to save space.
  (if-not (resource/loaded? resource)
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-pb
        :user-data {:pb {}
                    :pb-class (:pb-class pb-def)}})]
    (g/precluding-errors build-errors
      (let [template-build-targets (flatten template-build-targets)

            rt-node-descs
            (coll/transfer node-msgs []
              (keep
                (fn [decorated-node-msg]
                  (-> decorated-node-msg
                      (dissoc :layout->prop->override :layout->prop->value)
                      (node-desc->rt-node-desc "")))))

            id->node-desc-for-default-layout (coll/pair-map-by :id rt-node-descs)

            rt-layout-descs
            (mapv #(make-rt-layout-desc % node-msgs id->node-desc-for-default-layout)
                  layout-names)

            rt-pb-msg
            (protobuf/assign-repeated pb-msg
              :layouts rt-layout-descs
              :nodes rt-node-descs)

            rt-pb-msg (merge-rt-pb-msg rt-pb-msg template-build-targets)

            dep-build-targets
            (into (vec (flatten dep-build-targets))
                  (mapcat :deps)
                  template-build-targets)

            ;; TODO: Can we replace all this with pipeline/make-protobuf-build-target?
            deps-by-source
            (coll/transfer dep-build-targets {}
              (map (fn [dep-build-target]
                     (let [build-resource (:resource dep-build-target)
                           source-resource (:resource build-resource)
                           source-proj-path (resource/resource->proj-path source-resource)]
                       (pair source-proj-path build-resource)))))

            dep-resources
            (coll/transfer (:resource-fields pb-def) []
              (mapcat (fn [field]
                        (if (vector? field)
                          (e/map (fn [index]
                                   (into [(first field) index]
                                         (rest field)))
                                 (range (count (get rt-pb-msg (first field)))))
                          [field])))
              (map (fn [label]
                     (pair label
                           (get deps-by-source
                                (if (vector? label)
                                  (get-in rt-pb-msg label)
                                  (get rt-pb-msg label)))))))]

        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-pb
            :user-data {:pb rt-pb-msg
                        :pb-class (:pb-class pb-def)
                        :dep-resources dep-resources}
            :deps dep-build-targets})]))))

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
                                          :ext "gui_script"}))
            (dynamic label (properties/label-dynamic :gui :script))
            (dynamic tooltip (properties/tooltip-dynamic :gui :script)))

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
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$SceneDesc$AdjustReference)))
            (dynamic label (properties/label-dynamic :gui :adjust-reference))
            (dynamic tooltip (properties/tooltip-dynamic :gui :adjust-reference)))
  (property visible-layout g/Str (default "") ; No protobuf counterpart.
            (dynamic visible (g/constantly false)))
  (property current-nodes g/Int
            (value (g/fnk [node-ids] (count node-ids)))
            (dynamic read-only? (g/constantly true))
            (dynamic label (properties/label-dynamic :gui :current-nodes))
            (dynamic tooltip (properties/tooltip-dynamic :gui :current-nodes)))
  (property max-nodes g/Int (default (protobuf/default Gui$SceneDesc :max-nodes))
            (dynamic error (g/fnk [_node-id max-nodes ^:try node-ids]
                             (when-not (g/error-value? node-ids)
                               (validate-max-nodes _node-id max-nodes node-ids))))
            (dynamic label (properties/label-dynamic :gui :max-nodes))
            (dynamic tooltip (properties/tooltip-dynamic :gui :max-nodes)))
  (property max-dynamic-textures g/Int (default (protobuf/default Gui$SceneDesc :max-dynamic-textures))
            (dynamic error (g/fnk [_node-id max-dynamic-textures]
                              (validate-max-dynamic-textures _node-id max-dynamic-textures)))
            (dynamic label (properties/label-dynamic :gui :max-dynamic-textures))
            (dynamic tooltip (properties/tooltip-dynamic :gui :max-dynamic-textures)))

  (input script-resource resource/Resource)

  (input node-tree g/NodeID)
  (input layers-node g/NodeID)
  (input layouts-node g/NodeID)
  (input fonts-node g/NodeID)
  (input textures-node g/NodeID)
  (input particlefx-resources-node g/NodeID)
  (input materials-node g/NodeID)
  (input handler-infos g/Any :array)
  (output handler-infos g/Any (g/fnk [handler-infos]
                                (into [] (mapcat one-or-many-handler-infos-to-vec) handler-infos)))
  (input dep-build-targets g/Any :array)
  (input project-settings g/Any)
  (input default-tex-params g/Any)
  (output default-tex-params g/Any (gu/passthrough default-tex-params))
  (input display-profiles g/Any)
  (input node-msgs g/Any)
  (output node-msgs g/Any (gu/passthrough node-msgs))
  (input node-overrides g/Any)
  (output node-overrides g/Any :cached (gu/passthrough node-overrides))
  (input font-msgs g/Any :array)
  (input texture-msgs g/Any :array)
  (input material-msgs g/Any :array)
  (input layer-msgs g/Any)
  (output layer-msgs g/Any (g/fnk [layer-msgs] (mapv #(dissoc % :child-index) (sort-by :child-index layer-msgs))))
  (input particlefx-resource-msgs g/Any :array)
  (input resource-msgs g/Any :array)
  (input node-ids NameNodeIds)
  (output node-ids NameNodeIds (gu/passthrough node-ids))

  (input layout-names GuiResourceNames)
  (output layout-names GuiResourceNames
          (g/fnk [layout-names]
            (into (sorted-set)
                  layout-names)))

  (input texture-page-counts GuiResourcePageCounts)
  (output texture-page-counts GuiResourcePageCounts (gu/passthrough texture-page-counts))

  (input texture-resource-names GuiResourceNames)
  (input texture-gpu-textures GuiResourceTextures :array)
  (input texture-infos GuiResourceTextureInfos :array)
  (input material-shaders GuiResourceShaders :array)

  (input material-infos GuiResourceMaterialInfos :array)
  (output material-infos GuiResourceMaterialInfos
          (g/fnk [material-infos material-max-page-count]
            (into (sorted-map "" {:max-page-count material-max-page-count})
                  cat
                  material-infos)))

  (input font-shaders GuiResourceShaders :array)
  (input font-datas FontDatas :array)
  (input font-names g/Str :array)
  (output font-names GuiResourceNames :cached
          (g/fnk [font-names]
            (into (sorted-set) font-names)))

  (input layer-names GuiResourceNames)
  (input layer->index NameIndices)

  (input spine-scene-element-ids SpineSceneElementIds :array)
  (input spine-scene-infos SpineSceneInfos :array)
  (input spine-scene-names g/Str :array)
  (output spine-scene-names GuiResourceNames :cached
          (g/fnk [spine-scene-names]
            (into (sorted-set) spine-scene-names)))
  (input particlefx-infos ParticleFXInfos :array)

  (input particlefx-resource-names g/Str :array)
  (output particlefx-resource-names GuiResourceNames :cached
          (g/fnk [particlefx-resource-names]
            (into (sorted-set) particlefx-resource-names)))

  (input material-max-page-count g/Int :substitute nil)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (input samplers [g/KeywordMap])
  (output samplers [g/KeywordMap] (gu/passthrough samplers))

  (output aux-gui-resource-type-names GuiResourceTypeNames :cached
          (g/fnk [aux-basic-gui-scene-info]
            (let [material-names
                  (coll/transfer (:material-infos aux-basic-gui-scene-info) (sorted-set)
                    (map key)
                    (remove coll/empty?))]
              {:font (:font-names aux-basic-gui-scene-info)
               :layer (:layer-names aux-basic-gui-scene-info)
               :material material-names
               :particlefx (:particlefx-resource-names aux-basic-gui-scene-info)
               :spine-scene (:spine-scene-names aux-basic-gui-scene-info)
               :texture (:texture-resource-names aux-basic-gui-scene-info)})))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any :cached produce-save-value)

  (input template-build-targets g/Any :array)
  (output own-build-errors g/Any produce-own-build-errors)
  (input build-errors g/Any :array)
  (output build-errors g/Any produce-build-errors)
  (output build-targets g/Any :cached produce-build-targets)
  (input default-node-outline g/Any)
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id default-node-outline child-outlines own-build-errors]
                 (let [node-outline default-node-outline
                       icon (:icon pb-def)]
                   {:node-id _node-id
                    :node-outline-key (:ext pb-def)
                    :label (:label pb-def)
                    :icon icon
                    :children (vec (sort-by :order (conj child-outlines node-outline)))
                    :outline-error? (g/error-fatal? own-build-errors)})))
  (input default-scene g/Any)
  (output child-scenes g/Any (g/fnk [default-scene]
                               (let [node-tree-scene default-scene]
                                 (:children node-tree-scene))))
  (output scene g/Any :cached produce-scene)
  (output scene-dims g/Any :cached
          (g/fnk [display-profiles project-settings trivial-gui-scene-info]
            (let [current-layout (:current-layout trivial-gui-scene-info)]
              (or (some #(and (= current-layout (:name %))
                              (first (:qualifiers %)))
                        display-profiles)

                  ;; If the layout doesn't override our dimensions, use project
                  ;; settings. Note that project-settings might not have been
                  ;; connected in case our .gui scene hasn't been loaded, so we
                  ;; ultimately fall back on zero width and height.
                  (let [w (get project-settings ["display" "width"] 0)
                        h (get project-settings ["display" "height"] 0)]
                    {:width w :height h})))))
  (output unused-display-profiles g/Any (g/fnk [layout-names display-profiles]
                                          (coll/transfer display-profiles []
                                            (map :name)
                                            (remove layout-names))))

  (input aux-trivial-gui-scene-info TrivialGuiSceneInfo)
  (output own-trivial-gui-scene-info TrivialGuiSceneInfo
          (g/fnk [layout-names visible-layout]
            {:current-layout visible-layout
             :layout-names layout-names}))
  (output trivial-gui-scene-info TrivialGuiSceneInfo :cached
          (g/fnk [aux-trivial-gui-scene-info own-trivial-gui-scene-info]
            ;; Note: When our scene is imported as a template, the referencing
            ;; scene dictates the current-layout, and imported node ids are
            ;; prefixed with the id of the referencing template node.
            (coll/merge-with-kv
              (fn [key aux-value own-value]
                (case key
                  (:current-layout :id-prefix) aux-value ; Replaced, not merged.
                  (coll/merge aux-value own-value)))
              aux-trivial-gui-scene-info
              own-trivial-gui-scene-info)))

  (input aux-basic-gui-scene-info BasicGuiSceneInfo)
  (output own-basic-gui-scene-info BasicGuiSceneInfo :cached
          (g/fnk [font-names layer->index layer-names material-infos particlefx-resource-names spine-scene-names texture-page-counts texture-resource-names spine-scene-element-ids]
            {:font-names font-names
             :layer->index layer->index
             :layer-names layer-names
             :material-infos material-infos
             :particlefx-resource-names particlefx-resource-names
             :spine-scene-element-ids (reduce coll/merge spine-scene-element-ids)
             :spine-scene-names spine-scene-names
             :texture-page-counts texture-page-counts
             :texture-resource-names texture-resource-names}))
  (output basic-gui-scene-info BasicGuiSceneInfo :cached
          (g/fnk [aux-basic-gui-scene-info own-basic-gui-scene-info]
            ;; Note: When our scene is imported as a template, the layer
            ;; configuration of the referencing scene replaces our own.
            (coll/merge-with-kv
              (fn [key aux-value own-value]
                (case key
                  (:layer->index :layer-names) aux-value ; Replaced, not merged.
                  (coll/merge aux-value own-value)))
              aux-basic-gui-scene-info
              own-basic-gui-scene-info)))

  (input aux-costly-gui-scene-info CostlyGuiSceneInfo)
  (output own-costly-gui-scene-info CostlyGuiSceneInfo :cached
          (g/fnk [font-datas font-shaders material-shader material-shaders particlefx-infos spine-scene-infos texture-gpu-textures texture-infos]
            {:font-datas (reduce coll/merge font-datas)
             :font-shaders (reduce coll/merge font-shaders)
             :material-shaders (reduce coll/merge {"" material-shader} material-shaders)
             :particlefx-infos (reduce coll/merge particlefx-infos)
             :spine-scene-infos (reduce coll/merge spine-scene-infos)
             :texture-gpu-textures (reduce coll/merge texture-gpu-textures)
             :texture-infos (reduce coll/merge texture-infos)}))
  (output costly-gui-scene-info CostlyGuiSceneInfo :cached
          (g/fnk [aux-costly-gui-scene-info own-costly-gui-scene-info]
            (coll/merge-with coll/merge aux-costly-gui-scene-info own-costly-gui-scene-info))))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

(defn add-gui-node-with-props! [scene parent node-type custom-type props select-fn]
  (-> (g/with-auto-evaluation-context evaluation-context
        (let [node-tree (g/node-value scene :node-tree evaluation-context)
              id (or (:id props)
                     (id/resolve (subs (name node-type) 5)
                                 (g/node-value node-tree :id-counts evaluation-context)))
              def-node-type (get-registered-node-type-cls node-type custom-type)
              next-index (gui-attachment/next-child-index parent evaluation-context)
              node-properties (assoc props
                                :id id
                                :child-index next-index
                                :custom-type custom-type
                                :type node-type)]
          (concat
            (g/operation-label (localization/message "operation.gui.add-gui-node"))
            (g/make-nodes (g/node-id->graph-id scene) [gui-node [def-node-type node-properties]]
              (attach-gui-node node-tree parent gui-node)
              (when select-fn
                (select-fn [gui-node]))))))
      g/transact
      g/tx-nodes-added
      first))

(defn add-gui-node! [project scene parent node-type custom-type select-fn]
  ;; TODO: The project argument is unused. Remove.
  (let [node-type-info (get-registered-node-type-info node-type custom-type)
        props (:defaults node-type-info)]
    (add-gui-node-with-props! scene parent node-type custom-type props select-fn)))

(defn add-gui-node-handler [project {:keys [scene parent node-type custom-type]} select-fn]
  (add-gui-node! project scene parent node-type custom-type select-fn))

(defn add-template-gui-node-handler [project {:keys [scene parent node-type custom-type]} select-fn]
  (when-let [template-resources (g/with-auto-evaluation-context evaluation-context
                                  (resource-dialog/make
                                    (project/workspace project evaluation-context)
                                    project
                                    {:ext "gui"
                                     :accept-fn (fn [r] (not (contains-resource? project scene r evaluation-context)))}))]
    (let [template-resource (first template-resources)
          template-id (resource->id template-resource)
          node-type-info (get-registered-node-type-info node-type custom-type)
          default-props (:defaults node-type-info)
          props (assoc default-props :template {:resource template-resource :overrides {}}
                       :id template-id)]
      (add-gui-node-with-props! scene parent node-type custom-type props select-fn))))

(defn- make-add-handler [scene parent label icon handler-fn user-data]
  {:label label :icon icon :command :edit.add-embedded-component
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
                                   (let [add-handler (if (= (:node-type info) :type-template)
                                                       add-template-gui-node-handler
                                                       add-gui-node-handler)]
                                     (make-add-handler scene parent (:display-name info) (:icon info)
                                                       add-handler (into {} info)))))
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

(handler/defhandler :edit.add-embedded-component :workbench
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

(def ^:private node-property-defaults (node-desc->node-properties default-node-desc-pb-field-values))

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

        node->layout->prop->override
        (reduce (fn [node->layout->prop->override layout-desc]
                  (let [layout (:name layout-desc)]
                    (reduce (fn [node->layout->prop->override node-desc]
                              (let [node-properties (node-desc->node-properties node-desc)
                                    prop->override (extract-overrides node-properties)]
                                (cond-> node->layout->prop->override
                                        (coll/not-empty prop->override)
                                        (update (:id node-desc)
                                                assoc layout prop->override))))
                            node->layout->prop->override
                            (:nodes layout-desc))))
                {}
                (:layouts scene))

        node-descs         (map node-desc->node-properties (:nodes scene)) ; TODO: These are really the properties of the GuiNode subtype. Rename to node-properties.
        tmpl-node-descs    (into {}
                                 (comp (filter :template-node-child)
                                       (map (fn [{:keys [id parent] :as node-desc}]
                                              (pair id
                                                    {:template parent
                                                     :data (assoc (extract-overrides node-desc)
                                                             :layout->prop->override (get node->layout->prop->override id {}))}))))
                                 node-descs)
        tmpl-node-descs    (into {}
                                 (map (fn [[id data]]
                                        (pair id
                                              (update data :template
                                                      (fn [parent]
                                                        (if-let [parent-node-desc (tmpl-node-descs parent)]
                                                          (recur (:template parent-node-desc))
                                                          parent))))))
                                 tmpl-node-descs)
        node-descs         (eduction
                             (remove :template-node-child)
                             (map (fn [{:keys [id] :as node-desc}]
                                    (assoc node-desc :layout->prop->override (get node->layout->prop->override id {}))))
                             node-descs)
        tmpl-children      (group-by (comp :template second) tmpl-node-descs)
        tmpl-roots         (filter (complement tmpl-node-descs) (map first tmpl-children))
        template-data      (into {}
                                 (map (fn [r]
                                        (pair r
                                              (into {}
                                                    (map (fn [[id tmpl]]
                                                           (pair (subs id (inc (count r)))
                                                                 (:data tmpl))))
                                                    (rest (tree-seq fn/constantly-true
                                                                    (comp tmpl-children first)
                                                                    (pair r nil)))))))
                                 tmpl-roots)

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
      (g/make-nodes graph-id [textures-node TexturesNode]
                    (g/connect textures-node :texture-page-counts self :texture-page-counts)
                    (g/connect textures-node :_node-id self :textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :build-errors self :build-errors)
                    (g/connect textures-node :node-outline self :child-outlines)
                    (g/connect textures-node :add-handler-info self :handler-infos)
                    (g/connect textures-node :texture-resource-names self :texture-resource-names)
                    (for [texture-desc (:textures scene)
                          :let [resource (resolve-resource (:texture texture-desc))]]
                      (g/make-nodes graph-id [texture [TextureNode :name (:name texture-desc) :texture resource]]
                                    (attach-texture self textures-node texture))))

      ;; Materials list
      (g/make-nodes graph-id [materials-node MaterialsNode]
        (g/connect materials-node :_node-id self :materials-node)
        (g/connect materials-node :_node-id self :nodes)
        (g/connect materials-node :build-errors self :build-errors)
        (g/connect materials-node :node-outline self :child-outlines)
        (g/connect materials-node :add-handler-info self :handler-infos)
        (for [materials-desc (:materials scene)
              :let [resource (workspace/resolve-resource resource (:material materials-desc))]]
          (g/make-nodes graph-id [material [MaterialNode :name (:name materials-desc) :material resource]]
            (attach-material self materials-node material))))

      (g/make-nodes graph-id [particlefx-resources-node ParticleFXResources]
                    (g/connect particlefx-resources-node :_node-id self :particlefx-resources-node)
                    (g/connect particlefx-resources-node :_node-id self :nodes)
                    (g/connect particlefx-resources-node :build-errors self :build-errors)
                    (g/connect particlefx-resources-node :node-outline self :child-outlines)
                    (g/connect particlefx-resources-node :add-handler-info self :handler-infos)
                    (let [prop-keys (g/declared-property-labels ParticleFXResource)]
                      (for [particlefx-desc (:particlefxs scene)
                            :let [particlefx-desc (select-keys particlefx-desc prop-keys)]]
                        (g/make-nodes graph-id [particlefx-resource [ParticleFXResource
                                                                     :name (:name particlefx-desc)
                                                                     :particlefx (resolve-resource (:particlefx particlefx-desc))]]
                                      (attach-particlefx-resource self particlefx-resources-node particlefx-resource)))))

      (g/make-nodes graph-id [layers-node LayersNode]
                    (g/connect layers-node :_node-id self :layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :layer-msgs self :layer-msgs)
                    (g/connect layers-node :layer-names self :layer-names)
                    (g/connect layers-node :layer->index self :layer->index)
                    (g/connect layers-node :build-errors self :build-errors)
                    (g/connect layers-node :node-outline self :child-outlines)
                    (g/connect layers-node :add-handler-info self :handler-infos)
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
                    (for [[from to] [[:trivial-gui-scene-info :trivial-gui-scene-info]
                                     [:basic-gui-scene-info :basic-gui-scene-info]
                                     [:costly-gui-scene-info :costly-gui-scene-info]]]
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
                                        (let [parent (:parent node-desc)
                                              parent-node (if (str/blank? parent)
                                                            node-tree
                                                            (id->node parent))]
                                          (attach-gui-node node-tree parent-node gui-node)))
                              node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
                          (recur more
                                 (assoc id->node (:id node-desc) node-id)
                                 (into all-tx-data tx-data)
                                 (inc child-index)))
                        all-tx-data)))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                    (g/connect layouts-node :_node-id self :layouts-node)
                    (g/connect layouts-node :_node-id self :nodes)
                    (g/connect self :unused-display-profiles layouts-node :unused-display-profiles)
                    (g/connect layouts-node :names self :layout-names)
                    (g/connect layouts-node :build-errors self :build-errors)
                    (g/connect layouts-node :node-outline self :child-outlines)
                    (g/connect layouts-node :add-handler-info self :handler-infos)
                    (for [layout-desc (:layouts scene)]
                      (g/make-nodes graph-id [layout [LayoutNode (dissoc layout-desc :nodes)]]
                        (attach-layout self layouts-node layout))))
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
        (update :material fn/or default-material-proj-path))))

(defn- add-dropped-resource
  [scene workspace resource]
  (let [ext (resource/type-ext resource)
        base-name (resource/base-name resource)
        gen-name #(id/gen base-name (g/node-value (g/node-value scene %) :name-counts))]
    (cond
      (= ext "particlefx")
      (add-particlefx-resource scene (g/node-value scene :particlefx-resources-node) resource (gen-name :particlefx-resources-node))

      (= ext "font")
      (add-font scene (g/node-value scene :fonts-node) resource (gen-name :fonts-node))

      (some #{ext} (workspace/resource-kind-extensions workspace :atlas))
      (add-texture scene (g/node-value scene :textures-node) resource (gen-name :textures-node))

      (= ext "material")
      (add-material scene (g/node-value scene :materials-node) resource (gen-name :materials-node))

      :else
      nil)))

(defn- handle-drop
  [root-id _selection workspace _world-pos resources]
  (mapv (partial add-dropped-resource root-id workspace) resources))

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
        :allow-unloaded-use false ; Sort of works, but disabled until we can fix the file formats to not include all nodes imported from templates.
        :sanitize-fn sanitize-scene
        :icon (:icon def)
        :icon-class (:icon-class def)
        :tags (:tags def)
        :tag-opts (:tag-opts def)
        :template (:template def)
        :view-types [:scene :text]
        :view-opts {:scene {:grid true
                            :drop-fn handle-drop}}))))


(defn- attach-to-gui-scene-txs [{:keys [basis]} attach-fn scene-container-node-fn scene-node item-node]
  (attach-fn scene-node (scene-container-node-fn basis scene-node) item-node))

(defn- attach-to-gui-scene-fn [scene-container-node-fn attach-fn]
  (partial g/expand-ec attach-to-gui-scene-txs attach-fn scene-container-node-fn))

(defn- gui-scene-layers-getter [scene-node {:keys [basis] :as evaluation-context}]
  (let [layer-nodes (attachment/nodes-getter (gui-attachment/scene-node->layers-node basis scene-node) evaluation-context)]
    (vec (sort-by #(g/raw-property-value basis % :child-index) layer-nodes))))

(defn- reorder-gui-scene-layers [reordered-layer-node-ids]
  (coll/mapcat-indexed #(g/set-property %2 :child-index %1) reordered-layer-node-ids))

(defn- gui-scene-materials-getter [scene-node {:keys [basis] :as evaluation-context}]
  (attachment/nodes-getter (gui-attachment/scene-node->materials-node basis scene-node) evaluation-context))

(defn- gui-scene-particlefxs-getter [scene-node {:keys [basis] :as evaluation-context}]
  (attachment/nodes-getter (gui-attachment/scene-node->particlefx-resources-node basis scene-node) evaluation-context))

(defn- gui-scene-texture-nodes-getter [scene-node {:keys [basis] :as evaluation-context}]
  (attachment/nodes-getter (gui-attachment/scene-node->textures-node basis scene-node) evaluation-context))

(defn- gui-scene-layouts-getter [scene-node {:keys [basis] :as evaluation-context}]
  (attachment/nodes-getter (gui-attachment/scene-node->layouts-node basis scene-node) evaluation-context))

(defn- gui-scene-fonts-getter [scene-node {:keys [basis]}]
  (let [fonts-node (gui-attachment/scene-node->fonts-node basis scene-node)]
    ;; NOTE: we use :names instead of :nodes to get a list of fonts because it
    ;; excludes the internal fallback font
    (mapv gt/source-id (g/explicit-arcs-by-target basis fonts-node :names))))

(defn- gui-scene-nodes-getter [scene-node evaluation-context]
  ;; We need to use g/node-value instead of raw props and explicit arcs because
  ;; scene and gui nodes might be override nodes from templates: those don't
  ;; have explicit arcs.
  (let [node-tree (g/node-value scene-node :node-tree evaluation-context)
        nodes (g/node-value node-tree :nodes evaluation-context)]
    (vec (sort-by #(g/node-value % :child-index evaluation-context) nodes))))

(defn- gui-nodes-getter [parent-node evaluation-context]
  ;; We need to use g/node-value instead of raw props and explicit arcs because
  ;; scene and gui nodes might be override nodes from templates: those don't
  ;; have explicit arcs.
  (let [nodes (g/node-value parent-node :nodes evaluation-context)]
    (vec (sort-by #(g/node-value % :child-index evaluation-context) nodes))))

(defn- template-nodes-getter [template-node {:keys [basis] :as evaluation-context}]
  ;; We need to use g/node-value instead of raw props and explicit arcs because
  ;; scene and gui nodes might be override nodes from templates: those don't
  ;; have explicit arcs.
  (if-let [scene-node (g/node-feeding-into basis template-node :template-resource)]
    (gui-scene-nodes-getter scene-node evaluation-context)
    []))

(defn- attach-gui-node-to-gui-scene [{:keys [basis]} scene-node gui-node]
  (let [node-tree (gui-attachment/scene-node->node-tree basis scene-node)]
    (attach-gui-node node-tree node-tree gui-node)))

(def ^:private add-attachment-to-gui-scene-node (partial g/expand-ec attach-gui-node-to-gui-scene))

(defn- attach-gui-node-to-gui-node [{:keys [basis]} parent-gui-node gui-node]
  (let [node-tree (node->node-tree basis parent-gui-node)]
    (attach-gui-node node-tree parent-gui-node gui-node)))

(def ^:private add-attachment-to-gui-node (partial g/expand-ec attach-gui-node-to-gui-node))

(defn- reorder-gui-nodes [reordered-gui-node-ids]
  (coll/mapcat-indexed #(g/set-property %2 :child-index %1) reordered-gui-node-ids))

;; SDK api
(defn register-node-tree-attachment-node-type [workspace node-type]
  (concat
    ;; add the node type to gui scene's :nodes list (node tree root)
    (attachment/register workspace GuiSceneNode :nodes :add {node-type add-attachment-to-gui-scene-node})
    ;; add the node type to gui node's :nodes list (branches)
    (attachment/register workspace GuiNode :nodes :add {node-type add-attachment-to-gui-node})
    ;; make the node type a branch
    (attachment/alias workspace node-type :nodes GuiNode)))

(node-types/register-node-type-name! BoxNode "gui-node-type-box")
(node-types/register-node-type-name! PieNode "gui-node-type-pie")
(node-types/register-node-type-name! TextNode "gui-node-type-text")
(node-types/register-node-type-name! TemplateNode "gui-node-type-template")
(node-types/register-node-type-name! ParticleFXNode "gui-node-type-particlefx")

(defn- gui-node-nodes-read-only? [gui-node-id evaluation-context]
  (g/override? (:basis evaluation-context) gui-node-id))

(defn register-resource-types [workspace]
  (concat
    (attachment/register
      workspace GuiSceneNode :fonts
      :add {FontNode (attach-to-gui-scene-fn gui-attachment/scene-node->fonts-node attach-font)}
      :get gui-scene-fonts-getter)
    (attachment/register
      workspace GuiSceneNode :layers
      :add {LayerNode (attach-to-gui-scene-fn gui-attachment/scene-node->layers-node attach-layer)}
      :get gui-scene-layers-getter
      :reorder reorder-gui-scene-layers)
    (attachment/register
      workspace GuiSceneNode :layouts
      :add {LayoutNode (attach-to-gui-scene-fn gui-attachment/scene-node->layouts-node attach-layout)}
      :get gui-scene-layouts-getter)
    (attachment/register
      workspace GuiSceneNode :materials
      :add {MaterialNode (attach-to-gui-scene-fn gui-attachment/scene-node->materials-node attach-material)}
      :get gui-scene-materials-getter)
    (attachment/register
      workspace GuiSceneNode :nodes
      :get gui-scene-nodes-getter
      :reorder reorder-gui-nodes)
    (attachment/register
      workspace GuiNode :nodes
      :get gui-nodes-getter
      :reorder reorder-gui-nodes
      :read-only? gui-node-nodes-read-only?)
    (register-node-tree-attachment-node-type workspace BoxNode)
    (register-node-tree-attachment-node-type workspace PieNode)
    (register-node-tree-attachment-node-type workspace TextNode)

    ;; We don't use register-node-tree-attachment-node-type for registering the
    ;; TemplateNode since it does not use :nodes for children. Instead, it uses
    ;; the referenced scene node as override
    (attachment/register workspace GuiSceneNode :nodes :add {TemplateNode add-attachment-to-gui-scene-node})
    (attachment/register workspace GuiNode :nodes :add {TemplateNode add-attachment-to-gui-node})
    (attachment/register workspace TemplateNode :nodes :get template-nodes-getter)

    (register-node-tree-attachment-node-type workspace ParticleFXNode)
    (attachment/register
      workspace GuiSceneNode :particlefxs
      :add {ParticleFXResource (attach-to-gui-scene-fn gui-attachment/scene-node->particlefx-resources-node attach-particlefx-resource)}
      :get gui-scene-particlefxs-getter)
    (attachment/register
      workspace GuiSceneNode :textures
      :add {TextureNode (attach-to-gui-scene-fn gui-attachment/scene-node->textures-node attach-texture)}
      :get gui-scene-texture-nodes-getter)
    (register workspace pb-def)))

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

(handler/defhandler :edit.reorder-up :workbench
  (active? [selection] (or (selection->gui-node selection)
                           (selection->layer-node selection)))
  (enabled? [selection] (let [selected-node-id (g/override-root (handler/selection->node-id selection))
                              parent (core/scope selected-node-id)
                              node-child-index (g/node-value selected-node-id :child-index)
                              first-index (transduce (map second) min Long/MAX_VALUE (g/node-value parent :child-indices))]
                          (< first-index node-child-index)))
  (run [selection] (let [selected (g/override-root (handler/selection->node-id selection))]
                     (move-child-node! selected -1))))

(handler/defhandler :edit.reorder-down :workbench
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

(handler/defhandler :scene.set-gui-layout :workbench
  :label (localization/message "command.scene.set-gui-layout")
  (active? [project active-resource evaluation-context]
           (boolean (resource->gui-scene project active-resource evaluation-context)))
  (run [project active-resource user-data] (when user-data
                                             (when-let [scene (resource->gui-scene project active-resource)]
                                               (g/transact (g/set-property scene :visible-layout user-data)))))
  (state [project active-resource]
         (when-let [scene (resource->gui-scene project active-resource)]
           (let [visible (g/node-value scene :visible-layout)]
             {:label (if (empty? visible) (localization/message "gui.layout.default") visible)
              :command :scene.set-gui-layout
              :user-data visible})))
  (options [project active-resource user-data]
           (when-not user-data
             (when-let [scene (resource->gui-scene project active-resource)]
               (let [layout-names (g/node-value scene :layout-names)
                     layouts (cons "" layout-names)]
                 (for [l layouts]
                   {:label (if (empty? l) (localization/message "gui.layout.default") l)
                    :command :scene.set-gui-layout
                    :user-data l}))))))

(handler/register-menu! ::toolbar :visibility-settings
  [menu-items/separator
   {:icon layout-icon
    :command :scene.set-gui-layout}])

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

(def ^:private empty-node-type-registry
  {;; graph-node-type -> non-deprecated info
   :node-cls->type-info {}
   ;; node-desc-type keyword -> custom-type -> info
   :node-type->custom-type->type-info {}
   ;; flat list
   :type-infos []})

(defn- register-node-type-info [state {:keys [node-cls node-type custom-type] :as type-info}]
  (when-let [old-node-cls (-> state :node-type->custom-type->type-info (get node-type) (get custom-type) :node-cls)]
    (when-not (= old-node-cls node-cls)
      (throw (ex-info (format "Plugin GUI node type %s custom-type conflicts with %s." (:name @node-cls) (:name @old-node-cls))
                      {:custom-type custom-type
                       :node-cls node-cls
                       :conflicting-node-cls old-node-cls}))))
  (-> state
      (assoc-in [:node-type->custom-type->type-info node-type custom-type] type-info)
      (update :type-infos conj type-info)
      (cond-> (not (:deprecated type-info)) (update :node-cls->type-info assoc node-cls type-info))))

(defonce ^:private node-type-info-registry
  (atom (reduce register-node-type-info empty-node-type-registry base-node-type-infos)))

(defn- get-registered-node-type-infos []
  (:type-infos @node-type-info-registry))

(defn- get-registered-node-type-info
  ([node-cls]
   {:pre [(g/node-type? node-cls)]}
   (or (-> @node-type-info-registry
           :node-cls->type-info
           (get node-cls))
       (throw (ex-info (format "Unable to locate GUI node type info. Extension not loaded? (node-cls=%s)" (:k node-cls))
                       {:node-cls node-cls
                        :node-type-infos (keys (:node-cls->type-info @node-type-info-registry))}))))
  ([node-type custom-type]
   {:pre [(keyword? node-type)
          (integer? custom-type)]}
   (or (-> @node-type-info-registry
           :node-type->custom-type->type-info
           (get node-type)
           (get custom-type))
       (throw (ex-info (format "Unable to locate GUI node type info. Extension not loaded? (node-type=%s, custom-type=%s)"
                               node-type
                               custom-type)
                       {:node-type node-type
                        :custom-type custom-type
                        :node-type-infos (:node-type->custom-type->type-info @node-type-info-registry)})))))

(defn- extension-type-name->id [s]
  (if-let [i (str/last-index-of s \-)]
    (subs s (inc i))
    s))

(ext-graph/register-property-getter!
  ::GuiNode
  (fn GuiNode-getter [node-id property evaluation-context]
    (let [{:keys [basis]} evaluation-context
          node (g/node-by-id basis node-id)
          node-type (g/node-type node)
          last-colon-index (str/last-index-of property \:)
          property-name (if last-colon-index (subs property (inc last-colon-index)) property)]
      (when-let [prop-kw ((node-type->layout-property-names node-type) property-name)]
        (let [layout-name (if last-colon-index (subs property 0 last-colon-index) "")
              layout->prop->value (g/node-value node-id :layout->prop->value evaluation-context)]
          (when-let [prop->value (get layout->prop->value layout-name)]
            (let [value (prop->value prop-kw)
                  edit-type-id (properties/edit-type-id (g/node-property-dynamic node prop-kw :edit-type evaluation-context))]
              (when-let [converter (-> edit-type-id ext-graph/edit-type-id->value-converter :to)]
                #(converter value)))))))))

(ext-graph/register-property-setter!
  ::GuiNode
  (fn GuiNode-setter [node-id property rt project evaluation-context]
    (let [{:keys [basis]} evaluation-context
          node (g/node-by-id basis node-id)
          node-type (g/node-type node)
          last-colon-index (str/last-index-of property \:)
          property-name (if last-colon-index (subs property (inc last-colon-index)) property)]
      (when-let [prop-kw ((node-type->layout-property-names node-type) property-name)]
        (when-not (g/node-property-dynamic node prop-kw :read-only? false evaluation-context)
          (let [layout-name (if last-colon-index (subs property 0 last-colon-index) "")]
            (when (or (= "" layout-name)
                      (-> node-id
                          (g/node-value :trivial-gui-scene-info evaluation-context)
                          :layout-names
                          (contains? layout-name)))
              (let [edit-type (g/node-property-dynamic node prop-kw :edit-type evaluation-context)]
                (when-let [converter (-> edit-type properties/edit-type-id ext-graph/edit-type-id->value-converter :from)]
                  #(layout-property-set-in-specific-layout evaluation-context layout-name node-id prop-kw (converter % rt edit-type project evaluation-context)))))))))))

(ext-graph/register-property-resetter!
  ::GuiNode
  (fn GuiNode-resetter [node-id property evaluation-context]
    (let [{:keys [basis]} evaluation-context
          node (g/node-by-id basis node-id)
          node-type (g/node-type node)]
      (when-let [last-colon-index (str/last-index-of property \:)]
        (let [property-name (subs property (inc last-colon-index))]
          (when-let [prop-kw ((node-type->layout-property-names node-type) property-name)]
            (let [layout-name (subs property 0 last-colon-index)
                  layout->prop->override (g/node-value node-id :layout->prop->override evaluation-context)]
              (when-let [prop-kw->override (layout->prop->override layout-name)]
                (when (contains? prop-kw->override prop-kw)
                  #(layout-property-clear-in-specific-layout evaluation-context layout-name node-id prop-kw))))))))))

(defn- init-gui-node-attachment [{:keys [basis] :as evaluation-context} rt project parent-node-id child-node-type child-node-id attachment node-id-base-name-fn]
  (let [{:keys [node-type custom-type defaults]} (get-registered-node-type-info child-node-type)
        node-tree-or-gui-node (if (g/node-instance? basis GuiSceneNode parent-node-id)
                                (gui-attachment/scene-node->node-tree basis parent-node-id)
                                parent-node-id)
        props (assoc defaults
                :child-index (gui-attachment/next-child-index node-tree-or-gui-node evaluation-context)
                :custom-type custom-type
                :type node-type)]
    (concat
      (apply g/set-property child-node-id (coll/mapcat identity props))
      (-> attachment
          (eutil/provide-defaults
            "id" (rt/->lua
                   (id/gen (node-id-base-name-fn rt child-node-type attachment)
                           (g/node-value node-tree-or-gui-node :id-counts evaluation-context))))
          (ext-graph/attachment->set-tx-steps child-node-id rt project evaluation-context)))))

(defn- gui-node-id-base-name [_rt child-node-type _attachment]
  (or (some-> (node-types/->name child-node-type) extension-type-name->id) "node"))

(defmethod ext-graph/init-attachment ::GuiNode [evaluation-context rt project parent-node-id child-node-type child-node-id attachment]
  (init-gui-node-attachment evaluation-context rt project parent-node-id child-node-type child-node-id attachment gui-node-id-base-name))

(defn- template-node-id-base-name [rt _child-node-type attachment]
  (if-let [lua-template-str (attachment "template")]
    (FilenameUtils/getBaseName (rt/->clj rt coerce/string lua-template-str))
    "template"))

(defmethod ext-graph/init-attachment ::TemplateNode [evaluation-context rt project parent-node-id child-node-type child-node-id attachment]
  (init-gui-node-attachment evaluation-context rt project parent-node-id child-node-type child-node-id attachment template-node-id-base-name))

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
  (swap! node-type-info-registry register-node-type-info type-info))

;; Used by tests
(defn clear-custom-gui-scene-loaders-and-node-types-for-tests! []
  ;; TODO(save-value-cleanup): These really should be registered with the workspace so they don't pollute integration tests across projects.
  (reset! custom-gui-scene-loaders (sorted-map))
  (reset! node-type-info-registry (reduce register-node-type-info empty-node-type-registry base-node-type-infos)))
