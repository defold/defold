(ns editor.gui
  (:require [schema.core :as s]
            [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.graph-util :as gu]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.gui-clipping :as clipping]
            [editor.defold-project :as project]
            [editor.progress :as progress]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [editor.colors :as colors]
            [editor.gl.pass :as pass]
            [editor.types :as types]
            [editor.resource :as resource]
            [editor.properties :as properties]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.font :as font]
            [editor.dialogs :as dialogs]
            [editor.outline :as outline]
            [editor.material :as material]
            [editor.spine :as spine]
            [editor.validation :as validation])
  (:import [com.dynamo.gui.proto Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$NodeDesc Gui$NodeDesc$Type Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor
            Gui$NodeDesc$Pivot Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds Gui$NodeDesc$SizeMode]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.texture TextureLifecycle]
           [editor.types AABB]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]
           [java.awt.image BufferedImage]
           [com.defold.editor.pipeline TextureSetGenerator$UVTransform]
           [org.apache.commons.io FilenameUtils]))


(set! *warn-on-reflection* true)

(def ^:private texture-icon "icons/32/Icons_25-AT-Image.png")
(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private gui-icon "icons/32/Icons_38-GUI.png")
(def ^:private text-icon "icons/32/Icons_39-GUI-Text-node.png")
(def ^:private box-icon "icons/32/Icons_40-GUI-Box-node.png")
(def ^:private pie-icon "icons/32/Icons_41-GUI-Pie-node.png")
(def ^:private virtual-icon "icons/32/Icons_01-Folder-closed.png")
(def ^:private layer-icon "icons/32/Icons_42-Layers.png")
(def ^:private layout-icon "icons/32/Icons_50-Display-profiles.png")
(def ^:private template-icon gui-icon)

(def ^:private node-icons {:type-box box-icon
                           :type-pie pie-icon
                           :type-text text-icon
                           :type-template template-icon
                           :type-spine spine/spine-model-icon})

(def pb-def {:ext "gui"
             :label "Gui"
             :icon gui-icon
             :pb-class Gui$SceneDesc
             :resource-fields [:script :material [:fonts :font] [:textures :texture] [:spine-scenes :spine-scene]]
             :tags #{:component :non-embeddable}
             :tag-opts {:component {:transform-properties #{}}}})

; Fallback shader

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
    (persistent! (reduce conj! vb vs))))

(defn- ->uv-color-vtx-vb [vs uvs colors vcount]
  (let [vb (->uv-color-vtx vcount)
        vs (mapv (comp vec concat) vs uvs colors)]
    (persistent! (reduce conj! vb vs))))

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

(defn- premul [color]
  (let [[r g b a] color]
    [(* r a) (* g a) (* b a) a]))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])]
    (cond
      (contains? user-data :geom-data)
      (let [[vs uvs colors] (reduce (fn [[vs uvs colors :as vb] renderable]
                                      (let [user-data (:user-data renderable)
                                            world-transform (:world-transform renderable)
                                            vcount (count (:geom-data user-data))]
                                        (if (pos? vcount)
                                          [(into vs (geom/transf-p world-transform (:geom-data user-data)))
                                           (into uvs (:uv-data user-data))
                                           (into colors (repeat vcount (premul (:color user-data))))]
                                          vb)))
                                    [[] [] []] renderables)]
        (when (not-empty vs)
          (->uv-color-vtx-vb vs uvs colors (count vs))))

      (get-in user-data [:text-data :font-data])
      (font/gen-vertex-buffer gl (get-in user-data [:text-data :font-data])
                              (map (fn [r] (let [text-data (get-in r [:user-data :text-data])
                                                 alpha (get-in text-data [:color 3])]
                                             (-> text-data
                                                 (assoc :world-transform (:world-transform r))
                                                 (update-in [:outline 3] * alpha)
                                                 (update-in [:shadow 3] * alpha))))
                                   renderables))
      (contains? user-data :spine-scene-pb)
      (spine/gen-vb renderables))))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [user-data (get-in renderables [0 :user-data])
        clipping-state (:clipping-state user-data)
        gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
        material-shader (get user-data :material-shader)
        blend-mode (get user-data :blend-mode)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (let [shader (or material-shader shader)
            vertex-binding (vtx/use-with ::tris vb shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
          (clipping/setup-gl gl clipping-state)
          (case blend-mode
            :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
            :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
          (clipping/restore-gl gl clipping-state))))))

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

(defn render-nodes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (if (= pass pass/outline)
      (render-lines gl render-args renderables rcount)
      (render-tris gl render-args renderables rcount))))

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

(defn- pairs [v]
  (filter (fn [[v0 v1]] (> (Math/abs (double (- v1 v0))) 0)) (partition 2 1 v)))

(defn- proj-path [resource]
  (if resource
    (resource/proj-path resource)
    ""))

(defn- v3->v4 [v3 default]
  (conj (or v3 default) 1.0))

(def ^:private prop-index->prop-key
  (let [index->pb-field (protobuf/fields-by-indices Gui$NodeDesc)
        renames {:xanchor :x-anchor
                 :yanchor :y-anchor}]
    (into {} (map (fn [[k v]] [k (get renames v v)]) index->pb-field))))

(def ^:private prop-key->prop-index (clojure.set/map-invert prop-index->prop-key))

(g/defnk produce-node-msg [type parent _declared-properties _node-id basis]
  (let [pb-renames {:x-anchor :xanchor
                    :y-anchor :yanchor
                    :generated-id :id}
        v3-fields {:position [0.0 0.0 0.0] :scale [1.0 1.0 1.0] :size [200.0 100.0 0.0]}
        props (:properties _declared-properties)
        overridden-fields (->> props
                               (keep (fn [[prop val]] (when (contains? val :original-value) (prop-key->prop-index prop))))
                               (sort)
                               (vec))
        msg (-> {:type type
                 :template-node-child false
                 :overridden-fields overridden-fields}
              (into (map (fn [[k v]] [k (:value v)])
                         (filter (fn [[k v]] (and (get v :visible true)
                                                  (not (contains? (set (keys pb-renames)) k))))
                                 props)))
              (cond->
                (and parent (not-empty parent)) (assoc :parent parent)
                (= type :type-template) (->
                                          (update :template (fn [t] (resource/resource->proj-path (:resource t))))
                                          (assoc :color [1.0 1.0 1.0 1.0]))

                ;; To handle save "bugs" in the old editor; size and size-mode should not have been saved at all
                (= type :type-spine) (->
                                       (assoc :size [1.0 1.0 0.0 1.0])
                                       (assoc :size-mode :size-mode-auto)))
              (into (map (fn [[k v]] [v (get-in props [k :value])]) pb-renames)))
        msg (-> (reduce (fn [msg [k default]] (update msg k v3->v4 default)) msg v3-fields)
              (update :rotation (fn [r] (conj (math/quat->euler (doto (Quat4d.) (math/clj->vecmath (or r [0.0 0.0 0.0 1.0])))) 1))))]
    msg))

(def gui-node-parent-attachments
  [[:id :parent]
   [:material-shader :material-shader]
   [:texture-gpu-textures :texture-gpu-textures]
   [:texture-anim-datas :texture-anim-datas]
   [:texture-names :texture-names]
   [:font-shaders :font-shaders]
   [:font-datas :font-datas]
   [:font-names :font-names]
   [:layer-names :layer-names]
   [:layer->index :layer->index]
   [:spine-scene-element-ids :spine-scene-element-ids]
   [:spine-scene-infos :spine-scene-infos]
   [:spine-scene-names :spine-scene-names]
   [:id-prefix :id-prefix]
   [:current-layout :current-layout]])

(def gui-node-attachments
  [[:_node-id :nodes]
   [:node-ids :node-ids]
   [:node-outline :child-outlines]
   [:scene :child-scenes]
   [:node-msgs :node-msgs]
   [:node-rt-msgs :node-rt-msgs]
   [:node-overrides :node-overrides]
   [:build-errors :child-build-errors]
   [:template-build-targets :template-build-targets]])

(defn- attach-gui-node [parent gui-node type]
  (concat
    (for [[source target] gui-node-parent-attachments]
      (g/connect parent source gui-node target))
    (for [[source target] gui-node-attachments]
      (g/connect gui-node source parent target))))

(def GuiSceneNode nil)
(def GuiNode)
(def NodeTree)
(def LayoutsNode)
(def LayoutNode)
(def BoxNode)
(def PieNode)
(def TextNode)
(def TemplateNode)
(def SpineNode)

(defn- node->node-tree
  ([node]
    (node->node-tree (g/now) node))
  ([basis node]
    (if (g/node-instance? basis NodeTree node)
      node
      (recur basis (core/scope node)))))

(defn- node->gui-scene
  ([node]
    (node->gui-scene (g/now) node))
  ([basis node]
   (cond
     (g/node-instance? basis GuiSceneNode node)
     node

     (g/node-instance? basis GuiNode node)
     (node->gui-scene (node->node-tree node))

     :else
     (core/scope node))))

(defn- gen-gui-node-attach-fn [type]
  (fn [target source]
    (let [scene (node->gui-scene target)]
      (concat
        (g/update-property source :id outline/resolve-id (keys (g/node-value scene :node-ids)))
        (attach-gui-node target source type)))))

;; Schema validation is disabled because it severely affects project load times.
;; You might want to enable these before making drastic changes to Gui nodes.
(g/deftype ^:private GuiResourceNames s/Any #_(sorted-set s/Str))
(g/deftype ^:private GuiResourceTextures s/Any #_{s/Str TextureLifecycle})
(g/deftype ^:private GuiResourceShaders s/Any #_{s/Str ShaderLifecycle})
(g/deftype ^:private TextureAnimDatas s/Any #_{s/Str (s/maybe {s/Keyword s/Any})})
(g/deftype ^:private FontDatas s/Any #_{s/Str {s/Keyword s/Any}})
(g/deftype ^:private SpineSceneElementIds s/Any #_{s/Str {:spine-anim-ids (sorted-set s/Str)
                                                          :spine-skin-ids (sorted-set s/Str)}})
(g/deftype ^:private SpineSceneInfos s/Any #_{s/Str {:spine-scene-scene (s/maybe {s/Keyword s/Any})
                                                     :spine-scene-structure (s/maybe {s/Keyword s/Any})
                                                     :spine-scene-pb (s/maybe {s/Keyword s/Any})}})
(g/deftype ^:private IDMap {s/Str s/Int})
(g/deftype ^:private TemplateData {:resource  (s/maybe (s/protocol resource/Resource))
                                   :overrides {s/Str s/Any}})

(g/defnk override? [id-prefix] (some? id-prefix))
(g/defnk no-override? [id-prefix] (nil? id-prefix))

(defn- validate-contains [severity fmt prop-kw node-id coll key]
  (validation/prop-error severity node-id
                         prop-kw (fn [id ids]
                                   (when-not (contains? ids id)
                                     (format fmt id)))
                         key coll))

(defn- validate-gui-resource [severity fmt prop-kw node-id coll key]
  (validate-contains severity fmt prop-kw node-id coll key))

(defn- validate-required-gui-resource [fmt prop-kw node-id coll key]
  (or (validation/prop-error :fatal node-id prop-kw validation/prop-empty? key (properties/keyword->name prop-kw))
      (validate-gui-resource :fatal fmt prop-kw node-id coll key)))

(defn- validate-optional-gui-resource [fmt prop-kw node-id coll key]
  (when-not (empty? key)
    (validate-gui-resource :fatal fmt prop-kw node-id coll key)))

(defn- required-gui-resource-choicebox [coll]
  ;; The coll will contain a "" entry representing "No Selection". This is used
  ;; to lookup rendering resources in case the value has not been assigned.
  ;; We don't want any of these as an option in the dropdown.
  (properties/->choicebox (sort (remove empty? coll))))

(defn- optional-gui-resource-choicebox [coll]
  ;; The coll will contain a "" entry representing "No Selection". Remove this
  ;; before sorting the collection. We then provide the "" entry at the top.
  (properties/->choicebox (cons "" (sort (remove empty? coll)))))

(defn- prop-resource-error [node-id prop-kw prop-value prop-name]
  (or (validation/prop-error :fatal node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- references-gui-resource? [basis node-id prop-kw gui-resource-name]
  (and (g/property-value-origin? basis node-id prop-kw)
       (= gui-resource-name (g/node-value node-id prop-kw {:basis basis :no-cache true}))))

(defn- scene-node-ids [basis scene]
  ;; Unless we supply :no-cache true here, save-test/save-all fails. Reported as
  ;; DEFEDIT-1023: g/node-value writes stale results to cache when only basis is specified.
  (g/node-value scene :node-ids {:basis basis :no-cache true}))

(defmulti update-gui-resource-reference (fn [gui-resource-type basis node-id _old-name _new-name]
                                          [(:key @(g/node-type* basis node-id)) gui-resource-type]))
(defmethod update-gui-resource-reference :default [_ _basis _node-id _old-name _new-name] nil)

(defn- update-gui-resource-references [gui-resource-type basis gui-resource-node-id old-name new-name]
  (assert (keyword? gui-resource-type))
  (assert (or (nil? old-name) (string? old-name)))
  (assert (string? new-name))
  (when (and (not (empty? old-name))
             (not (empty? new-name)))
    (when-some [scene (core/scope-of-type basis gui-resource-node-id GuiSceneNode)]
      (into []
            (comp (map val)
                  (keep (fn [node-id]
                          (update-gui-resource-reference gui-resource-type basis node-id old-name new-name))))
            (scene-node-ids basis scene)))))

;; Base nodes

(def base-display-order [:id :generated-id scene/SceneNode :size])

(defn- validate-layer [emit-warnings? node-id layer-names layer]
  (when-not (empty? layer)
    ;; Layers are not brought in from template sources. The brought in nodes act
    ;; as if they belong to no layer if the layer does not exist in the scene,
    ;; but a warning is emitted.
    (if (g/property-value-origin? node-id :layer)
      (validate-contains :fatal "layer '%s' does not exist in the scene" :layer node-id layer-names layer)
      (when emit-warnings?
        (validate-contains :warning "layer '%s' from template scene does not exist in the scene - will use layer of parent" :layer node-id layer-names layer)))))

(g/defnode GuiNode
  (inherits core/Scope)
  (inherits scene/SceneNode)
  (inherits outline/OutlineNode)

  (property type g/Keyword (dynamic visible (g/constantly false)))

  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))

  (property id g/Str (default "")
            (dynamic visible no-override?))
  (property generated-id g/Str
            (dynamic label (g/constantly "Id"))
            (value (gu/passthrough id))
            (dynamic read-only? (g/constantly true))
            (dynamic visible override?))
  (property size types/Vec3 (default [0 0 0])
            (dynamic visible (g/fnk [type] (not= type :type-template))))
  (property color types/Color (default [1 1 1 1])
            (dynamic visible (g/fnk [type] (not= type :type-template)))
            (dynamic edit-type (g/constantly {:type types/Color
                                          :ignore-alpha? true})))
  (property alpha g/Num (default 1.0)
            (dynamic edit-type (g/constantly {:type :slider
                                          :min 0.0
                                          :max 1.0
                                          :precision 0.01})))
  (property inherit-alpha g/Bool (default true))

  (property layer g/Str
            (default "")
            (dynamic edit-type (g/fnk [layer-names] (optional-gui-resource-choicebox layer-names)))
            (dynamic error (g/fnk [_node-id layer layer-names] (validate-layer true _node-id layer-names layer))))
  (output layer-index g/Any :cached
          (g/fnk [layer layer->index] (layer->index layer)))

  (input parent g/Str)

  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (input texture-gpu-textures GuiResourceTextures)
  (output texture-gpu-textures GuiResourceTextures (gu/passthrough texture-gpu-textures))
  (input texture-anim-datas TextureAnimDatas)
  (output texture-anim-datas TextureAnimDatas (gu/passthrough texture-anim-datas))
  (input texture-names GuiResourceNames)
  (output texture-names GuiResourceNames (gu/passthrough texture-names))
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
  (input child-scenes g/Any :array)
  (output node-outline-children [outline/OutlineData] :cached (gu/passthrough child-outlines))
  (output node-outline-reqs g/Any :cached (g/fnk [] [{:node-type BoxNode
                                                      :tx-attach-fn (gen-gui-node-attach-fn :type-box)}
                                                     {:node-type PieNode
                                                      :tx-attach-fn (gen-gui-node-attach-fn :type-pie)}
                                                     {:node-type TextNode
                                                      :tx-attach-fn (gen-gui-node-attach-fn :type-text)}
                                                     {:node-type TemplateNode
                                                      :tx-attach-fn (gen-gui-node-attach-fn :type-template)}
                                                     {:node-type SpineNode
                                                      :tx-attach-fn (gen-gui-node-attach-fn :type-spine)}]))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id node-outline-children node-outline-reqs type outline-overridden?]
                 {:node-id _node-id
                  :label id
                  :icon (node-icons type)
                  :child-reqs node-outline-reqs
                  :copy-include-fn (fn [node]
                                     (let [node-id (g/node-id node)]
                                       (and (g/node-instance? GuiNode node-id)
                                            (not= node-id (g/node-value node-id :parent)))))
                  :children node-outline-children
                  :outline-overridden? outline-overridden?}))

  (output transform-properties g/Any scene/produce-scalable-transform-properties)
  (output node-msg g/Any :cached produce-node-msg)
  (input node-msgs g/Any :array)
  (output node-msgs g/Any :cached (g/fnk [node-msgs node-msg] (into [node-msg] node-msgs)))
  (input node-rt-msgs g/Any :array)
  (output node-rt-msgs g/Any :cached (g/fnk [node-rt-msgs node-msg] (into [node-msg] node-rt-msgs)))
  (output aabb g/Any :abstract)
  (output scene-children g/Any :cached (g/fnk [child-scenes] (vec child-scenes)))
  (output scene-renderable g/Any :abstract)
  (output color+alpha types/Color (g/fnk [color alpha] (assoc color 3 alpha)))
  (output scene g/Any :cached (g/fnk [_node-id aabb transform scene-children scene-renderable]
                                     {:node-id _node-id
                                      :aabb aabb
                                      :transform transform
                                      :children scene-children
                                      :renderable scene-renderable}))

  (input node-ids IDMap :array)
  (output id g/Str (g/fnk [id-prefix id] (str id-prefix id)))
  (output node-ids IDMap :cached (g/fnk [_node-id id node-ids] (reduce merge {id _node-id} node-ids)))

  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides id _properties]
                                         (into {id (into {} (->> (:properties _properties)
                                                                 (filter (fn [[_ v]] (contains? v :original-value)))
                                                                 (map (fn [[k v]] [k (:value v)]))))}
                                               node-overrides)))
  (input current-layout g/Str)
  (output current-layout g/Str (gu/passthrough current-layout))
  (input child-build-errors g/Any :array)
  (output build-errors-gui-node g/Any :cached (g/fnk [child-build-errors _node-id layer layer-names]
                                                (g/flatten-errors [child-build-errors (validate-layer false _node-id layer-names layer)])))
  (output build-errors g/Any (gu/passthrough build-errors-gui-node))
  (input template-build-targets g/Any :array)
  (output template-build-targets g/Any (gu/passthrough template-build-targets)))

(defmethod update-gui-resource-reference [::GuiNode :layer]
  [_ basis node-id old-name new-name]
  (when (references-gui-resource? basis node-id :layer old-name)
    (g/set-property node-id :layer new-name)))

(g/defnode VisualNode
  (inherits GuiNode)

  (property blend-mode g/Keyword (default :blend-mode-alpha)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$BlendMode))))
  (property adjust-mode g/Keyword (default :adjust-mode-fit)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$AdjustMode))))
  (property pivot g/Keyword (default :pivot-center)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$Pivot))))
  (property x-anchor g/Keyword (default :xanchor-none)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$XAnchor))))
  (property y-anchor g/Keyword (default :yanchor-none)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$YAnchor))))

  (output gpu-texture TextureLifecycle :abstract)
  (output aabb-size g/Any (gu/passthrough size))
  (output aabb g/Any :cached (g/fnk [pivot aabb-size transform scene-children]
                                    (let [offset-fn (partial mapv + (pivot-offset pivot aabb-size))
                                          [min-x min-y _] (offset-fn [0 0 0])
                                          [max-x max-y _] (offset-fn aabb-size)
                                          self-aabb (-> (geom/null-aabb)
                                                        (geom/aabb-incorporate min-x min-y 0)
                                                        (geom/aabb-incorporate max-x max-y 0)
                                                        (geom/aabb-transform transform))]
                                      (transduce (comp (keep :aabb)
                                                       (map #(geom/aabb-transform % transform)))
                                                 geom/aabb-union self-aabb scene-children))))
  (output scene-renderable-user-data g/Any :abstract)
  (output scene-renderable g/Any :cached
          (g/fnk [_node-id layer-index blend-mode inherit-alpha gpu-texture material-shader scene-renderable-user-data aabb]
            (let [gpu-texture (or gpu-texture (:gpu-texture scene-renderable-user-data))]
              {:render-fn render-nodes
               :aabb aabb
               :passes [pass/transparent pass/selection pass/outline]
               :user-data (assoc scene-renderable-user-data
                                 :gpu-texture gpu-texture
                                 :inherit-alpha inherit-alpha
                                 :material-shader material-shader
                                 :blend-mode blend-mode)
               :batch-key {:texture gpu-texture :blend-mode blend-mode}
               :select-batch-key _node-id
               :layer-index layer-index
               :topmost? true})))
  (output build-errors-visual-node g/Any (gu/passthrough build-errors-gui-node))
  (output build-errors g/Any (gu/passthrough build-errors-visual-node)))

(def ^:private validate-texture (partial validate-optional-gui-resource "texture '%s' does not exist in the scene" :texture))

(g/defnode ShapeNode
  (inherits VisualNode)

  (property size types/Vec3 (default [0 0 0])
            (value (g/fnk [size size-mode texture-size]
                          (if (= :size-mode-auto size-mode)
                            (or texture-size size)
                            size)))
            (dynamic read-only? (g/fnk [size-mode type] (and (or (= type :type-box) (= type :type-pie))
                                                             (= :size-mode-auto size-mode)))))
  (property size-mode g/Keyword (default :size-mode-auto)
            (dynamic visible (g/fnk [type] (or (= type :type-box) (= type :type-pie))))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$SizeMode))))
  (property texture g/Str
            (default "")
            (dynamic edit-type (g/fnk [texture-names] (optional-gui-resource-choicebox texture-names)))
            (dynamic error (g/fnk [_node-id texture-names texture] (validate-texture _node-id texture-names texture))))

  (property clipping-mode g/Keyword (default :clipping-mode-none)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$ClippingMode))))
  (property clipping-visible g/Bool (default true))
  (property clipping-inverted g/Bool (default false))


  (output anim-data g/Any (g/fnk [texture-anim-datas texture]
                            (or (texture-anim-datas texture)
                                (texture-anim-datas ""))))
  (output gpu-texture TextureLifecycle (g/fnk [texture-gpu-textures texture]
                                         (or (texture-gpu-textures texture)
                                             (texture-gpu-textures ""))))
  (output texture-size g/Any :cached (g/fnk [anim-data]
                                       (when (some? anim-data)
                                         [(double (:width anim-data)) (double (:height anim-data)) 0.0])))
  (output build-errors-shape-node g/Any :cached (g/fnk [build-errors-visual-node _node-id texture texture-names]
                                                  (g/flatten-errors [build-errors-visual-node (validate-texture _node-id texture-names texture)])))
  (output build-errors g/Any (gu/passthrough build-errors-shape-node)))

(defmethod update-gui-resource-reference [::ShapeNode :texture]
  [_ basis node-id old-name new-name]
  (when (g/property-value-origin? basis node-id :texture)
    (let [old-value (g/node-value node-id :texture {:basis basis :no-cache true})]
      (if (= old-name old-value)
        (g/set-property node-id :texture new-name)
        (let [[old-texture animation] (str/split old-value #"/")]
          (when (and (= old-name old-texture)
                     (not (empty? animation)))
            (g/set-property node-id :texture (str new-name "/" animation))))))))

;; Box nodes

(g/defnode BoxNode
  (inherits ShapeNode)

  (property slice9 types/Vec4 (default [0 0 0 0]))

  (display-order (into base-display-order
                       [:size-mode :texture :slice9 :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  ;; Overloaded outputs
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size color+alpha slice9 anim-data clipping-mode clipping-visible clipping-inverted]
                 (let [[w h _] size
                       offset (pivot-offset pivot size)
                       order [0 1 3 3 1 2]
                       x-vals (pairs [0 (get slice9 0) (- w (get slice9 2)) w])
                       y-vals (pairs [0 (get slice9 3) (- h (get slice9 1)) h])
                       corners (for [[x0 x1] x-vals
                                     [y0 y1] y-vals]
                                 (geom/transl offset [[x0 y0 0] [x0 y1 0] [x1 y1 0] [x1 y0 0]]))
                       vs (vec (mapcat #(map (partial nth %) order) corners))
                       tex anim-data
                       tex-w (:width tex 1)
                       tex-h (:height tex 1)
                       u-vals (pairs [0 (/ (get slice9 0) tex-w) (- 1 (/ (get slice9 2) tex-w)) 1])
                       v-vals (pairs [1 (- 1 (/ (get slice9 3) tex-h)) (/ (get slice9 1) tex-h) 0])
                       uv-trans (get-in tex [:uv-transforms 0])
                       uvs (for [[u0 u1] u-vals
                                 [v0 v1] v-vals]
                             (geom/uv-trans uv-trans [[u0 v0] [u0 v1] [u1 v1] [u1 v0]]))
                       uvs (vec (mapcat #(map (partial nth %) order) uvs))
                       lines (vec (mapcat #(interleave % (drop 1 (cycle %))) corners))
                       user-data {:geom-data vs
                                  :line-data lines
                                  :uv-data uvs
                                  :color color+alpha}]
                   (cond-> user-data
                     (not= :clipping-mode-none clipping-mode)
                     (assoc :clipping {:mode clipping-mode :inverted clipping-inverted :visible clipping-visible}))))))

;; Pie nodes

(def ^:private perimeter-vertices-min 4)
(def ^:private perimeter-vertices-max 1000)

(defn- validate-perimeter-vertices [node-id perimeter-vertices]
  (validation/prop-error :fatal node-id :perimeter-vertices validation/prop-outside-range? [perimeter-vertices-min perimeter-vertices-max] perimeter-vertices "Perimeter Vertices"))

(g/defnode PieNode
  (inherits ShapeNode)

  (property outer-bounds g/Keyword (default :piebounds-ellipse)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$PieBounds))))
  (property inner-radius g/Num (default 0.0))
  (property perimeter-vertices g/Int (default 10)
            (dynamic error (g/fnk [_node-id perimeter-vertices] (validate-perimeter-vertices _node-id perimeter-vertices))))

  (property pie-fill-angle g/Num (default 360.0))

  (display-order (into base-display-order
                       [:size-mode :texture :inner-radius :outer-bounds :perimeter-vertices :pie-fill-angle
                        :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output pie-data g/KeywordMap (g/fnk [_node-id outer-bounds inner-radius perimeter-vertices pie-fill-angle]
                                  (let [perimeter-vertices (max perimeter-vertices-min (min perimeter-vertices perimeter-vertices-max))]
                                    {:outer-bounds outer-bounds :inner-radius inner-radius
                                     :perimeter-vertices perimeter-vertices :pie-fill-angle pie-fill-angle})))

  ;; Overloaded outputs
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [pivot size color+alpha pie-data anim-data clipping-mode clipping-visible clipping-inverted]
                 (let [[w h _] size
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
                                  :color color+alpha}]
                   (cond-> user-data
                     (not= :clipping-mode-none clipping-mode)
                     (assoc :clipping {:mode clipping-mode :inverted clipping-inverted :visible clipping-visible})))))
  (output build-errors g/Any :cached (g/fnk [build-errors-shape-node _node-id perimeter-vertices]
                                       (g/flatten-errors [build-errors-shape-node (validate-perimeter-vertices _node-id perimeter-vertices)]))))

;; Text nodes

(def ^:private validate-font (partial validate-required-gui-resource "font '%s' does not exist in the scene" :font))

(g/defnode TextNode
  (inherits VisualNode)

  ; Text
  (property text g/Str
            (default "<text>")
            (dynamic edit-type (g/constantly {:type :multi-line-text})))
  (property line-break g/Bool (default false))
  (property font g/Str
    (default "")
    (dynamic edit-type (g/fnk [font-names] (required-gui-resource-choicebox font-names)))
    (dynamic error (g/fnk [_node-id font-names font]
                     (validate-font _node-id font-names font))))
  (property text-leading g/Num (default 1.0))
  (property text-tracking g/Num (default 0.0))
  (property outline types/Color (default [1 1 1 1])
            (dynamic edit-type (g/constantly {:type types/Color
                                          :ignore-alpha? true})))
  (property outline-alpha g/Num (default 1.0)
    (dynamic edit-type (g/constantly {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  (property shadow types/Color (default [1 1 1 1])
            (dynamic edit-type (g/constantly {:type types/Color
                                          :ignore-alpha? true})))
  (property shadow-alpha g/Num (default 1.0)
    (dynamic edit-type (g/constantly {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))

  (display-order (into base-display-order [:text :line-break :font :color :alpha :inherit-alpha :text-leading :text-tracking :outline :outline-alpha :shadow :shadow-alpha :layer]))

  (output font-data font/FontData (g/fnk [font-datas font]
                                    (or (font-datas font)
                                        (font-datas ""))))

  ;; Overloaded outputs
  (output material-shader ShaderLifecycle (g/fnk [font-shaders font]
                                            (or (font-shaders font)
                                                (font-shaders ""))))
  (output gpu-texture TextureLifecycle (g/fnk [font-data] (:texture font-data)))
  (output scene-renderable-user-data g/Any :cached
          (g/fnk [aabb pivot text-data]
                 (let [min (types/min-p aabb)
                       max (types/max-p aabb)
                       size [(- (.x max) (.x min)) (- (.y max) (.y min)) 0]
                       [w h _] size
                       offset (pivot-offset pivot size)
                       lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
                   {:line-data lines
                    :text-data text-data})))
  (output text-layout g/Any :cached (g/fnk [size font-data text line-break text-leading text-tracking]
                                           (font/layout-text (:font-map font-data) text line-break (first size) text-tracking text-leading)))
  (output aabb-size g/Any :cached (g/fnk [text-layout]
                                         [(:width text-layout) (:height text-layout) 0]))
  (output text-data g/KeywordMap (g/fnk [text-layout font-data line-break color alpha outline outline-alpha shadow shadow-alpha aabb-size pivot text-leading text-tracking]
                                        (cond-> {:text-layout text-layout
                                                 :font-data font-data
                                                 :color (assoc color 3 alpha)
                                                 :outline (assoc outline 3 outline-alpha)
                                                 :shadow (assoc shadow 3 shadow-alpha)
                                                 :align (pivot->h-align pivot)}
                                          font-data (assoc :offset (let [[x y] (pivot-offset pivot aabb-size)
                                                                         h (second aabb-size)]
                                                                     [x (+ y (- h (get-in font-data [:font-map :max-ascent])))])))))
  (output build-errors g/Any :cached (g/fnk [build-errors-visual-node _node-id font font-names]
                                       (g/flatten-errors [build-errors-visual-node (validate-font _node-id font-names font)]))))

(defmethod update-gui-resource-reference [::TextNode :font]
  [_ basis node-id old-name new-name]
  (when (references-gui-resource? basis node-id :font old-name)
    (g/set-property node-id :font new-name)))

;; Template nodes

(defn- trans-position [pos parent-p ^Quat4d parent-q parent-s]
  (let [[x y z] (mapv * pos parent-s)]
    (conj (->>
           (math/rotate parent-q (Vector3d. x y z))
           (math/vecmath->clj)
           (mapv + parent-p))
          1.0)))

(defn- trans-rotation [rot ^Quat4d parent-q]
  (let [q (math/euler->quat rot)]
    (-> q
      (doto (.mul parent-q q))
      (math/quat->euler)
      (conj 1.0))))

(defn- template-outline-subst [err]
  ;; TODO: embed error so can warn in outline
  ;; outline content not really used, only children if any.
  {:node-id 0
   :icon ""
   :label ""})

(g/defnode TemplateNode
  (inherits GuiNode)

  (property template TemplateData
            (dynamic read-only? override?)
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "gui"
                                              :to-type (fn [v] (:resource v))
                                              :from-type (fn [r] {:resource r :overrides {}})}))
            (dynamic error (g/fnk [_node-id template-resource]
                             (prop-resource-error _node-id :template template-resource "Template")))
            (value (g/fnk [_node-id id template-resource template-overrides]
                          (let [overrides (into {} (map (fn [[k v]] [(subs k (inc (count id))) v]) template-overrides))]
                            {:resource template-resource :overrides overrides})))
            (set (fn [basis self old-value new-value]
                   (let [project (project/get-project self)
                         current-scene (g/node-feeding-into basis self :template-resource)]
                     (concat
                       (if current-scene
                         (g/delete-node current-scene)
                         [])
                       (if (and new-value (:resource new-value))
                         (project/connect-resource-node project (:resource new-value) self []
                                                        (fn [scene-node]
                                                          (let [override (g/override basis scene-node {:traverse? (fn [basis [src src-label tgt tgt-label]]
                                                                                                                    (if (not= src current-scene)
                                                                                                                      (or (g/node-instance? basis GuiNode src)
                                                                                                                          (g/node-instance? basis NodeTree src)
                                                                                                                          (g/node-instance? basis GuiSceneNode src)
                                                                                                                          (g/node-instance? basis LayoutsNode src)
                                                                                                                          (g/node-instance? basis LayoutNode src))
                                                                                                                      false))})
                                                                id-mapping (:id-mapping override)
                                                                or-scene (get id-mapping scene-node)
                                                                node-mapping (comp id-mapping (or (g/node-value scene-node :node-ids {:basis basis}) (constantly nil)))]
                                                            (concat
                                                              (:tx-data override)
                                                              (for [[from to] [[:node-ids :node-ids]
                                                                               [:node-outline :template-outline]
                                                                               [:template-scene :template-scene]
                                                                               [:build-targets :template-build-targets]
                                                                               [:build-errors :child-build-errors]
                                                                               [:resource :template-resource]
                                                                               [:pb-msg :scene-pb-msg]
                                                                               [:rt-pb-msg :scene-rt-pb-msg]
                                                                               [:node-overrides :template-overrides]]]
                                                                (g/connect or-scene from self to))
                                                              (for [[from to] [[:layer-names :layer-names]
                                                                               [:texture-gpu-textures :aux-texture-gpu-textures]
                                                                               [:texture-anim-datas :aux-texture-anim-datas]
                                                                               [:texture-names :aux-texture-names]
                                                                               [:font-shaders :aux-font-shaders]
                                                                               [:font-datas :aux-font-datas]
                                                                               [:font-names :aux-font-names]
                                                                               [:spine-scene-element-ids :aux-spine-scene-element-ids]
                                                                               [:spine-scene-infos :aux-spine-scene-infos]
                                                                               [:spine-scene-names :aux-spine-scene-names]
                                                                               [:template-prefix :id-prefix]
                                                                               [:current-layout :current-layout]]]
                                                                (g/connect self from or-scene to))
                                                              (for [[id data] (:overrides new-value)
                                                                    :let [node-id (node-mapping id)]
                                                                    :when node-id
                                                                    [label value] data]
                                                                (g/set-property node-id label value))))))
                         []))))))

  (display-order (into base-display-order [:template]))

  (input scene-pb-msg g/Any :substitute (fn [_] {:nodes []}))
  (input scene-rt-pb-msg g/Any)
  (input scene-build-targets g/Any)
  (output scene-build-targets g/Any (gu/passthrough scene-build-targets))

  (input template-resource resource/Resource :cascade-delete)
  (input template-outline outline/OutlineData :substitute template-outline-subst)
  (input template-scene g/Any)
  (input template-overrides g/Any)
  (output template-prefix g/Str (g/fnk [id] (str id "/")))

  ; Overloaded outputs
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [template-outline current-layout]
                                                                     (get-in template-outline [:children 0 :children])))
  (output outline-overridden? g/Bool :cached (g/fnk [template-outline]
                                                    (let [children (get-in template-outline [:children 0 :children])]
                                                      (boolean (some :outline-overridden? children)))))
  (output node-outline-reqs g/Any :cached (g/constantly []))
  (output node-msgs g/Any :cached (g/fnk [id node-msg scene-pb-msg]
                                    (into [node-msg] (map #(cond-> (assoc % :template-node-child true)
                                                             (empty? (:parent %)) (assoc :parent id))
                                                          (:nodes scene-pb-msg)))))
  (output node-rt-msgs g/Any :cached (g/fnk [node-msg scene-rt-pb-msg]
                                       (let [parent-q (math/euler->quat (:rotation node-msg))]
                                         (into [] (map #(cond-> %
                                                          (empty? (:layer %)) (assoc :layer (:layer node-msg))
                                                          (:inherit-alpha %) (->
                                                                               (update :alpha * (:alpha node-msg))
                                                                               (assoc :inherit-alpha (:inherit-alpha node-msg)))
                                                          (empty? (:parent %)) (->
                                                                                 (assoc :parent (:parent node-msg))
                                                                                 ;; In fact incorrect, but only possibility to retain rotation/scale separation
                                                                                 (update :scale (partial mapv * (:scale node-msg)))
                                                                                 (update :position trans-position (:position node-msg) parent-q (:scale node-msg))
                                                                                 (update :rotation trans-rotation parent-q)))
                                                       (:nodes scene-rt-pb-msg))))))
  (output node-overrides g/Any :cached (g/fnk [id _properties template-overrides]
                                              (-> {id (into {} (map (fn [[k v]] [k (:value v)])
                                                                    (filter (fn [[_ v]] (contains? v :original-value))
                                                                            (:properties _properties))))}
                                                (merge template-overrides))))
  (output aabb g/Any (g/fnk [template-scene transform]
                       (geom/aabb-transform (:aabb template-scene (geom/null-aabb)) transform)))
  (output scene-children g/Any (g/fnk [_node-id template-scene]
                                 (if-let [child-scenes (:children template-scene)]
                                   (mapv #(scene/claim-child-scene % _node-id) child-scenes)
                                   [])))
  (output scene-renderable g/Any :cached (g/fnk [color+alpha inherit-alpha]
                                                {:passes [pass/selection]
                                                 :user-data {:color color+alpha :inherit-alpha inherit-alpha}}))
  (output build-errors g/Any :cached (g/fnk [build-errors-gui-node _node-id template-resource]
                                       (g/flatten-errors [build-errors-gui-node (prop-resource-error _node-id :template template-resource "Template")]))))

;; Spine nodes

(def ^:private validate-spine-scene (partial validate-required-gui-resource "spine scene '%s' does not exist in the scene" :spine-scene))

(defn- validate-spine-default-animation [node-id spine-scene-names spine-anim-ids spine-default-animation spine-scene]
  (when-not (g/error? (validate-spine-scene node-id spine-scene-names spine-scene))
    (validate-optional-gui-resource "animation '%s' could not be found in the specified spine scene" :spine-default-animation node-id spine-anim-ids spine-default-animation)))

(defn- validate-spine-skin [node-id spine-scene-names spine-skin-ids spine-skin spine-scene]
  (when-not (g/error? (validate-spine-scene node-id spine-scene-names spine-scene))
    (validate-optional-gui-resource "skin '%s' could not be found in the specified spine scene" :spine-skin node-id spine-skin-ids spine-skin)))

(g/defnode SpineNode
  (inherits VisualNode)

  (property spine-scene g/Str
    (default "")
    (dynamic edit-type (g/fnk [spine-scene-names] (required-gui-resource-choicebox spine-scene-names)))
    (dynamic error (g/fnk [_node-id spine-scene spine-scene-names]
                     (validate-spine-scene _node-id spine-scene-names spine-scene))))
  (property spine-default-animation g/Str
    (dynamic label (g/constantly "Default Animation"))
    (dynamic error (g/fnk [_node-id spine-anim-ids spine-default-animation spine-scene spine-scene-names]
                     (validate-spine-default-animation _node-id spine-scene-names spine-anim-ids spine-default-animation spine-scene)))
    (dynamic edit-type (g/fnk [spine-anim-ids] (optional-gui-resource-choicebox spine-anim-ids))))
  (property spine-skin g/Str
    (dynamic label (g/constantly "Skin"))
    (dynamic error (g/fnk [_node-id spine-scene spine-scene-names spine-skin spine-skin-ids]
                     (validate-spine-skin _node-id spine-scene-names spine-skin-ids spine-skin spine-scene)))
    (dynamic edit-type (g/fnk [spine-skin-ids] (optional-gui-resource-choicebox spine-skin-ids))))

  (property clipping-mode g/Keyword (default :clipping-mode-none)
    (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$NodeDesc$ClippingMode))))
  (property clipping-visible g/Bool (default true))
  (property clipping-inverted g/Bool (default false))

  (property size types/Vec3
    (dynamic visible (g/constantly false)))

  (display-order (into base-display-order
                       [:spine-scene :spine-default-animation :spine-skin :color :alpha :inherit-alpha :layer :blend-mode :pivot :x-anchor :y-anchor
                        :adjust-mode :clipping :visible-clipper :inverted-clipper]))

  (output spine-anim-ids GuiResourceNames (g/fnk [spine-scene-element-ids spine-scene]
                                            (:spine-anim-ids (or (spine-scene-element-ids spine-scene)
                                                                 (spine-scene-element-ids "")))))
  (output spine-skin-ids GuiResourceNames (g/fnk [spine-scene-element-ids spine-scene]
                                            (:spine-skin-ids (or (spine-scene-element-ids spine-scene)
                                                                 (spine-scene-element-ids "")))))
  (output spine-scene-scene g/Any (g/fnk [spine-scene-infos spine-scene]
                                    (:spine-scene-scene (or (spine-scene-infos spine-scene)
                                                            (spine-scene-infos "")))))
  (output spine-scene-structure g/Any (g/fnk [spine-scene-infos spine-scene]
                                        (:spine-scene-structure (or (spine-scene-infos spine-scene)
                                                                    (spine-scene-infos "")))))
  (output spine-scene-pb g/Any (g/fnk [spine-scene-infos spine-scene]
                                 (:spine-scene-pb (or (spine-scene-infos spine-scene)
                                                      (spine-scene-infos "")))))
  (output gpu-texture TextureLifecycle (g/constantly nil))
  (output scene-renderable-user-data g/Any :cached
    (g/fnk [spine-scene-scene color+alpha clipping-mode clipping-inverted clipping-visible]
      (let [user-data (-> spine-scene-scene
                        (get-in [:renderable :user-data])
                        (assoc :color (premul color+alpha)))]
        (cond-> user-data
          (not= :clipping-mode-none clipping-mode)
          (assoc :clipping {:mode clipping-mode :inverted clipping-inverted :visible clipping-visible})))))

  (output bone-node-msgs g/Any :cached (g/fnk [node-msgs spine-scene-structure spine-scene-pb adjust-mode]
                                         (let [pb-msg (first node-msgs)
                                               gui-node-id (:id pb-msg)
                                               id-fn (fn [b] (format "%s/%s" gui-node-id (:name b)))
                                               bones (tree-seq :children :children (:skeleton spine-scene-structure))
                                               bone-order (zipmap (map id-fn (-> spine-scene-pb :skeleton :bones)) (range))
                                               child-to-parent (reduce (fn [m b] (into m (map (fn [c] [(:name c) b]) (:children b)))) {} bones)
                                               bone-msg {:spine-node-child true
                                                         :size [0.0 0.0 0.0 0.0]
                                                         :position [0.0 0.0 0.0 0.0]
                                                         :scale [1.0 1.0 1.0 0.0]
                                                         :type :type-box
                                                         :adjust-mode adjust-mode}
                                               bone-msgs (mapv (fn [b] (assoc bone-msg :id (id-fn b) :parent (if (contains? child-to-parent (:name b))
                                                                                                               (id-fn (get child-to-parent (:name b)))
                                                                                                               gui-node-id))) bones)]
                                           ;; Bone nodes need to be sorted in same order as bones in rig scene
                                           (sort-by #(bone-order (:id %)) bone-msgs))))
  
  (output node-rt-msgs g/Any :cached
    (g/fnk [node-msgs node-rt-msgs bone-node-msgs spine-skin-ids]
      (let [pb-msg (first node-msgs)
            rt-pb-msgs (into node-rt-msgs [(update pb-msg :spine-skin (fn [skin] (if (str/blank? skin) (first spine-skin-ids) skin)))])]
        (into rt-pb-msgs bone-node-msgs))))
  (output build-errors g/Any :cached (g/fnk [build-errors-visual-node _node-id spine-anim-ids spine-default-animation spine-skin-ids spine-skin spine-scene spine-scene-names]
                                       (g/flatten-errors [build-errors-visual-node
                                                          (validate-spine-scene _node-id spine-scene-names spine-scene)
                                                          (validate-spine-default-animation _node-id spine-scene-names spine-anim-ids spine-default-animation spine-scene)
                                                          (validate-spine-skin _node-id spine-scene-names spine-skin-ids spine-skin spine-scene)]))))

(defmethod update-gui-resource-reference [::SpineNode :spine-scene]
  [_ basis node-id old-name new-name]
  (when (references-gui-resource? basis node-id :spine-scene old-name)
    (g/set-property node-id :spine-scene new-name)))

(g/defnode ImageTextureNode
  (input image BufferedImage)
  (output packed-image BufferedImage (gu/passthrough image))
  (output anim-data g/Any (g/fnk [^BufferedImage image]
                            {nil {:width (.getWidth image)
                                  :height (.getHeight image)
                                  :frames [{:tex-coords [[0 1] [0 0] [1 0] [1 1]]}]
                                  :uv-transforms [(TextureSetGenerator$UVTransform.)]}})))

;; NOTE: ImageTextureNode above is a source of image data.
;; This InternalTextureNode is a drop-in replacement for TextureNode below.
;; It can be used in place of TextureNode when you already have a gpu-texture.
(g/defnode InternalTextureNode
  (property name g/Str)
  (property gpu-texture TextureLifecycle)
  (output texture-anim-datas TextureAnimDatas (g/fnk [name] {name nil}))
  (output texture-gpu-textures GuiResourceTextures (g/fnk [name gpu-texture] {name gpu-texture})))

(g/defnk produce-texture-anim-datas [_node-id anim-data name]
  ;; If the referenced texture-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-texture entry for "".
  (when (some? anim-data)
    ;; Input anim-data is a map of anim-ids to anim-data.
    ;; The produced anim-data prefixes the anim-id with he texture name like so: "texture/anim".
    ;; If the texture does not contain animations, we emit an entry for the "texture" name only.
    (if (empty? anim-data)
      {name nil}
      (into {}
            (map (fn [[id data]] [(if id (format "%s/%s" name id) name) data]))
            anim-data))))

(g/defnk produce-texture-gpu-textures [_node-id anim-data name image samplers]
  ;; If the referenced texture-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-texture entry for "".
  (when (and (some? anim-data) (some? image))
    (let [gpu-texture (let [params (material/sampler->tex-params (first samplers))]
                        (texture/set-params (texture/image-texture _node-id image) params))]
      ;; If the texture does not contain animations, we emit an entry for the "texture" name only.
      (if (empty? anim-data)
        {name gpu-texture}
        (into {}
              (map (fn [id] [(if id (format "%s/%s" name id) name) gpu-texture]))
              (keys anim-data))))))

(g/defnk produce-texture-names [anim-data name]
  ;; If the texture does not contain animations, we emit an entry for the "texture" name only.
  (if (empty? anim-data)
    (sorted-set name)
    (into (sorted-set)
          (map (fn [id] (if id (format "%s/%s" name id) name)))
          (keys anim-data))))

(g/defnode TextureNode
  (inherits outline/OutlineNode)

  (property name g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? name))
            (set (partial update-gui-resource-references :texture)))
  (property texture resource/Resource
            (value (gu/passthrough texture-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :texture-resource]
                                                [:packed-image :image]
                                                [:anim-data :anim-data]
                                                [:anim-ids :anim-ids]
                                                [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id texture]
                                  (prop-resource-error _node-id :texture texture "Texture")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["atlas" "tilesource"]})))

  (input texture-resource resource/Resource)
  (input image BufferedImage :substitute (constantly nil))
  (input anim-data g/Any :substitute (constantly nil))
  (input anim-ids g/Any :substitute (constantly []))
  (input image-texture g/NodeID :cascade-delete)
  (input samplers [g/KeywordMap] :substitute (constantly []))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))

  (output node-outline outline/OutlineData (g/fnk [_node-id name]
                                             {:node-id _node-id
                                              :label name
                                              :icon texture-icon}))
  (output pb-msg g/Any (g/fnk [name texture-resource]
                         {:name name
                          :texture (proj-path texture-resource)}))
  (output texture-anim-datas TextureAnimDatas :cached produce-texture-anim-datas)
  (output texture-gpu-textures GuiResourceTextures :cached produce-texture-gpu-textures)
  (output texture-names GuiResourceNames :cached produce-texture-names)
  (output build-errors g/Any :cached (g/fnk [_node-id name texture]
                                       (g/flatten-errors [(validation/prop-error :fatal _node-id :name validation/prop-empty? name "Name")
                                                          (prop-resource-error _node-id :texture texture "Texture")]))))

(g/defnode FontNode
  (inherits outline/OutlineNode)
  (property name g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? name))
            (set (partial update-gui-resource-references :font)))
  (property font resource/Resource
            (value (gu/passthrough font-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter
                    basis self old-value new-value
                    [:resource :font-resource]
                    [:font-data :font-data]
                    [:material-shader :font-shader]
                    [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id font]
                                  (prop-resource-error _node-id :font font "Font")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["font"]})))

  (input font-resource resource/Resource)
  (input font-data font/FontData :substitute (constantly nil))
  (input font-shader ShaderLifecycle :substitute (constantly nil))

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any :cached (gu/passthrough dep-build-targets))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon font-icon}))
  (output pb-msg g/Any (g/fnk [name font-resource]
                              {:name name
                               :font (proj-path font-resource)}))
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
  (output build-errors g/Any :cached (g/fnk [_node-id name font]
                                       (g/flatten-errors [(validation/prop-error :fatal _node-id :name validation/prop-empty? name "Name")
                                                          (prop-resource-error _node-id :font font "Font")]))))

(g/defnode LayerNode
  (inherits outline/OutlineNode)
  (property name g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? name))
            (set (partial update-gui-resource-references :layer)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon layer-icon}))
  (output pb-msg g/Any (g/fnk [name]
                              {:name name}))
  (output build-errors g/Any :cached (validation/prop-error-fnk :fatal validation/prop-empty? name)))

(g/defnk produce-spine-scene-element-ids [name spine-anim-ids spine-scene spine-scene-structure]
  ;; If the referenced spine-scene-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-spine-scene entry for "".
  ;; NOTE: the no-spine-scene entry uses an instance of SpineSceneNode with an empty name.
  ;; It does not have any data, but it should still return an entry.
  (when (or (and (= "" name) (nil? spine-scene))
            (every? some? [spine-anim-ids spine-scene-structure]))
    {name {:spine-anim-ids (into (sorted-set) spine-anim-ids)
           :spine-skin-ids (into (sorted-set) (:skins spine-scene-structure))}}))

(g/defnk produce-spine-scene-infos [name spine-scene spine-scene-pb spine-scene-scene spine-scene-structure]
  ;; If the referenced spine-scene-resource is missing, we don't return an entry.
  ;; This will cause every usage to fall back on the no-spine-scene entry for "".
  ;; NOTE: the no-spine-scene entry uses an instance of SpineSceneNode with an empty name.
  ;; It does not have any data, but it should still return an entry.
  (when (or (and (= "" name) (nil? spine-scene))
            (every? some? [spine-scene-pb spine-scene-scene spine-scene-structure]))
    {name {:spine-scene-pb spine-scene-pb
           :spine-scene-scene spine-scene-scene
           :spine-scene-structure spine-scene-structure}}))

(g/defnode SpineSceneNode
  (inherits outline/OutlineNode)
  (property name g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? name))
            (set (partial update-gui-resource-references :spine-scene)))
  (property spine-scene resource/Resource
            (value (gu/passthrough spine-scene-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter
                     basis self old-value new-value
                     [:resource :spine-scene-resource]
                     [:build-targets :dep-build-targets]
                     [:spine-anim-ids :spine-anim-ids]
                     [:scene :spine-scene-scene]
                     [:scene-structure :spine-scene-structure]
                     [:spine-scene-pb :spine-scene-pb])))
            (dynamic error (g/fnk [_node-id spine-scene]
                                  (prop-resource-error _node-id :spine-scene spine-scene "Spine Scene")))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["spinescene"]})))

  (input spine-scene-resource resource/Resource)
  (input spine-anim-ids g/Any :substitute (constantly nil))
  (input dep-build-targets g/Any)
  (input spine-scene-scene g/Any :substitute (constantly nil))
  (input spine-scene-structure g/Any :substitute (constantly nil))
  (input spine-scene-pb g/Any :substitute (constantly nil))

  (output dep-build-targets g/Any :cached (gu/passthrough dep-build-targets))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon spine/spine-scene-icon}))
  (output pb-msg g/Any (g/fnk [name spine-scene]
                              {:name name
                               :spine-scene (proj-path spine-scene)}))
  (output spine-scene-element-ids SpineSceneElementIds :cached produce-spine-scene-element-ids)
  (output spine-scene-infos SpineSceneInfos :cached produce-spine-scene-infos)
  (output spine-scene-names GuiResourceNames (g/fnk [name] (sorted-set name)))
  (output build-errors g/Any :cached (g/fnk [_node-id name spine-scene]
                                       (g/flatten-errors [(validation/prop-error :fatal _node-id :name validation/prop-empty? name "Name")
                                                          (prop-resource-error _node-id :spine-scene spine-scene "Spine Scene")]))))

(def ^:private non-overridable-fields #{:template :id :parent})

(defn- extract-overrides [node-desc]
  (select-keys node-desc (remove non-overridable-fields (map prop-index->prop-key (:overridden-fields node-desc)))))

(defn- layout-pb-msg [name node-msgs]
  (let [node-msgs (filter (comp not-empty :overridden-fields) node-msgs)]
    (cond-> {:name name}
      (not-empty node-msgs)
      (assoc :nodes node-msgs))))

(g/defnode LayoutNode
  (inherits outline/OutlineNode)
  (property name g/Str
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-empty? name)))
  (property nodes g/Any
            (dynamic visible (g/constantly false))
            (value (gu/passthrough layout-overrides))
            (set (fn [basis self _ new-value]
                   (let [scene (ffirst (g/targets-of basis self :_node-id))
                         node-tree (g/node-value scene :node-tree {:basis basis})
                         or-data new-value
                         override (g/override basis node-tree {:traverse? (fn [basis [src src-label tgt tgt-label]]
                                                                            (or (g/node-instance? basis GuiNode src)
                                                                                (g/node-instance? basis NodeTree src)
                                                                                (g/node-instance? basis GuiSceneNode src)))})
                         id-mapping (:id-mapping override)
                         or-node-tree (get id-mapping node-tree)
                         node-mapping (comp id-mapping (g/node-value node-tree :node-ids {:basis basis}))]
                     (concat
                       (:tx-data override)
                       (for [[from to] [[:node-overrides :layout-overrides]
                                        [:node-msgs :node-msgs]
                                        [:node-rt-msgs :node-rt-msgs]
                                        [:node-outline :node-tree-node-outline]
                                        [:scene :node-tree-scene]]]
                         (g/connect or-node-tree from self to))

                       (for [[from to] [[:id-prefix :id-prefix]]]
                         (g/connect self from or-node-tree to))
                       (for [[id data] or-data
                             :let [node-id (node-mapping id)]
                             :when node-id
                             [label value] data]
                         (g/set-property node-id label value)))))))

  (input layout-overrides g/Any :cascade-delete)
  (input node-msgs g/Any)
  (input node-rt-msgs g/Any)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon layout-icon}))
  (output pb-msg g/Any :cached (g/fnk [name node-msgs] (layout-pb-msg name node-msgs)))
  (output pb-rt-msg g/Any :cached (g/fnk [name node-rt-msgs] (layout-pb-msg name node-rt-msgs)))
  (input node-tree-node-outline g/Any)
  (output layout-node-outline g/Any (g/fnk [name node-tree-node-outline] [name node-tree-node-outline]))
  (input node-tree-scene g/Any)
  (output layout-scene g/Any (g/fnk [name node-tree-scene] [name node-tree-scene]))
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))
  (output build-errors g/Any :cached (validation/prop-error-fnk :fatal validation/prop-empty? name)))

(defmacro gen-outline-fnk [label order sort-children? child-reqs]
  `(g/fnk [~'_node-id ~'child-outlines]
          {:node-id ~'_node-id
           :label ~label
           :icon ~virtual-icon
           :order ~order
           :read-only true
           :child-reqs ~child-reqs
           :children ~(if sort-children?
                       `(vec (sort-by :index ~'child-outlines))
                       'child-outlines)}))

(g/defnode NodeTree
  (inherits core/Scope)
  (inherits outline/OutlineNode)

  (property id g/Str (default (g/constantly ""))
            (dynamic visible (g/constantly false)))

  (input child-scenes g/Any :array)
  (output child-scenes g/Any :cached (g/fnk [child-scenes] (vec (sort-by (comp :index :renderable) child-scenes))))
  (output node-outline outline/OutlineData :cached
          (gen-outline-fnk "Nodes" 0 false [{:node-type BoxNode
                                             :tx-attach-fn (gen-gui-node-attach-fn :type-box)}
                                            {:node-type PieNode
                                             :tx-attach-fn (gen-gui-node-attach-fn :type-pie)}
                                            {:node-type TextNode
                                             :tx-attach-fn (gen-gui-node-attach-fn :type-text)}
                                            {:node-type TemplateNode
                                             :tx-attach-fn (gen-gui-node-attach-fn :type-template)}
                                            {:node-type SpineNode
                                             :tx-attach-fn (gen-gui-node-attach-fn :type-spine)}]))
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (map :aabb child-scenes))
                                      :children child-scenes}))
  (input node-msgs g/Any :array)
  (output node-msgs g/Any :cached (g/fnk [node-msgs] (flatten node-msgs)))
  (input node-rt-msgs g/Any :array)
  (output node-rt-msgs g/Any :cached (g/fnk [node-rt-msgs] (flatten node-rt-msgs)))
  (input node-overrides g/Any :array)
  (output node-overrides g/Any :cached (g/fnk [node-overrides] (into {} node-overrides)))
  (input node-ids IDMap :array)
  (output node-ids IDMap :cached (g/fnk [node-ids] (reduce merge node-ids)))
  (input texture-gpu-textures GuiResourceTextures)
  (output texture-gpu-textures GuiResourceTextures (gu/passthrough texture-gpu-textures))
  (input texture-anim-datas TextureAnimDatas)
  (output texture-anim-datas TextureAnimDatas (gu/passthrough texture-anim-datas))
  (input texture-names GuiResourceNames)
  (output texture-names GuiResourceNames (gu/passthrough texture-names))
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
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
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix))
  (input current-layout g/Str)
  (output current-layout g/Str (gu/passthrough current-layout))
  (input child-build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough child-build-errors))
  (input template-build-targets g/Any :array)
  (output template-build-targets g/Any (gu/passthrough template-build-targets)))


(g/defnode TexturesNode
  (inherits outline/OutlineNode)
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached (gen-outline-fnk "Textures" 1 false [])))

(g/defnode FontsNode
  (inherits outline/OutlineNode)
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached (gen-outline-fnk "Fonts" 2 false [])))

(g/defnode LayersNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))

  (input ordered-layer-names g/Str :array)
  (output layer-names GuiResourceNames :cached (g/fnk [ordered-layer-names] (into (sorted-set) ordered-layer-names)))

  (input layer-msgs g/Any :array)
  (output layer-msgs g/Any :cached (g/fnk [layer-msgs] (flatten layer-msgs)))

  (output layer->index g/Any :cached (g/fnk [ordered-layer-names]
                                       (zipmap ordered-layer-names (range))))

  (output node-outline outline/OutlineData :cached (gen-outline-fnk "Layers" 3 false [])))

(g/defnode LayoutsNode
  (inherits outline/OutlineNode)
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached (gen-outline-fnk "Layouts" 4 false [])))

(g/defnode SpineScenesNode
  (inherits outline/OutlineNode)
  (input build-errors g/Any :array)
  (output build-errors g/Any (gu/passthrough build-errors))
  (output node-outline outline/OutlineData :cached (gen-outline-fnk "Spine Scenes" 5 false [])))

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
    (-> scene
      clipping/setup-states
      sort-scene)))

(defn- ->scene-pb-msg [script-resource material-resource adjust-reference background-color max-nodes node-msgs layer-msgs font-msgs texture-msgs layout-msgs spine-scene-msgs]
  {:script (proj-path script-resource)
   :material (proj-path material-resource)
   :adjust-reference adjust-reference
   :background-color background-color
   :max-nodes max-nodes
   :nodes node-msgs
   :layers layer-msgs
   :fonts font-msgs
   :textures texture-msgs
   :layouts layout-msgs
   :spine-scenes spine-scene-msgs})

(g/defnk produce-pb-msg [script-resource material-resource adjust-reference background-color max-nodes node-msgs layer-msgs font-msgs texture-msgs layout-msgs spine-scene-msgs]
  (->scene-pb-msg script-resource material-resource adjust-reference background-color max-nodes node-msgs layer-msgs font-msgs texture-msgs layout-msgs spine-scene-msgs))

(g/defnk produce-rt-pb-msg [script-resource material-resource adjust-reference background-color max-nodes node-rt-msgs layer-msgs font-msgs texture-msgs layout-rt-msgs spine-scene-msgs]
  (->scene-pb-msg script-resource material-resource adjust-reference background-color max-nodes node-rt-msgs layer-msgs font-msgs texture-msgs layout-rt-msgs spine-scene-msgs))

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

(defn- merge-rt-pb-msg [rt-pb-msg template-build-targets]
  (let [merge-fn! (fn [coll msg kw] (reduce conj! coll (map #(do [(:name %) %]) (get msg kw))))
        [textures fonts spine-scenes] (loop [textures (transient {})
                                             fonts (transient {})
                                             spine-scenes (transient {})
                                             msgs (conj (mapv #(get-in % [:user-data :pb]) template-build-targets) rt-pb-msg)]
                                        (if-let [msg (first msgs)]
                                          (recur
                                            (merge-fn! textures msg :textures)
                                            (merge-fn! fonts msg :fonts)
                                            (merge-fn! spine-scenes msg :spine-scenes)
                                            (next msgs))
                                          [(persistent! textures) (persistent! fonts) (persistent! spine-scenes)]))]
    (assoc rt-pb-msg :textures (mapv second textures) :fonts (mapv second fonts) :spine-scenes (mapv second spine-scenes))))

(g/defnk produce-build-targets [_node-id build-errors resource rt-pb-msg dep-build-targets template-build-targets]
  (let [def pb-def
        template-build-targets (flatten template-build-targets)
        rt-pb-msg (merge-rt-pb-msg rt-pb-msg template-build-targets)
        dep-build-targets (concat (flatten dep-build-targets) (mapcat :deps (flatten template-build-targets)))
        deps-by-source (into {} (map #(let [res (:resource %)] [(proj-path (:resource res)) res]) dep-build-targets))
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

(defn- validate-max-nodes [_node-id max-nodes node-ids]
    (or (validation/prop-error :fatal _node-id :max-nodes (partial validation/prop-outside-range? [1 1024]) max-nodes "Max Nodes")
        (validation/prop-error :fatal _node-id :max-nodes (fn [v] (let [c (count node-ids)]
                                                                    (when (> c max-nodes)
                                                                      (format "the actual number of nodes (%d) exceeds 'Max Nodes' (%d)" c max-nodes)))) max-nodes)))

(g/defnk produce-build-errors [build-errors _node-id material max-nodes node-ids script]
  (g/flatten-errors [build-errors
                     (when script (prop-resource-error _node-id :script script "Script"))
                     (prop-resource-error _node-id :material material "Material")
                     (validate-max-nodes _node-id max-nodes node-ids)]))

(defn- get-ids [outline]
  (map :label (tree-seq (constantly true) :children outline)))

(g/defnode GuiSceneNode
  (inherits project/ResourceNode)

  (property script resource/Resource
            (value (gu/passthrough script-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter
                    basis self old-value new-value
                    [:resource :script-resource]
                    [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id script]
                             (when script
                               (prop-resource-error _node-id :script script "Script"))))
            (dynamic edit-type (g/fnk [] {:type resource/Resource
                                          :ext "gui_script"})))


  (property material resource/Resource
    (value (gu/passthrough material-resource))
    (set (fn [basis self old-value new-value]
           (project/resource-setter
            basis self old-value new-value
            [:resource :material-resource]
            [:shader :material-shader]
            [:samplers :samplers]
            [:build-targets :dep-build-targets])))
    (dynamic error (g/fnk [_node-id material]
                          (prop-resource-error _node-id :material material "Material")))
    (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]})))

  (property adjust-reference g/Keyword (dynamic edit-type (g/constantly (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property pb g/Any (dynamic visible (g/constantly false)))
  (property def g/Any (dynamic visible (g/constantly false)))
  (property background-color types/Color (dynamic visible (g/constantly false)) (default [1 1 1 1]))
  (property visible-layout g/Str (default (g/constantly ""))
            (dynamic visible (g/constantly false))
            (dynamic edit-type (g/fnk [layout-msgs] {:type :choicebox
                                                     :options (into {"" "Default"} (map (fn [l] [(:name l) (:name l)]) layout-msgs))})))
  (property max-nodes g/Int
            (dynamic error (g/fnk [_node-id max-nodes node-ids]
                             (validate-max-nodes _node-id max-nodes node-ids))))

  (input script-resource resource/Resource)

  (input node-tree g/NodeID)
  (input fonts-node g/NodeID)
  (input textures-node g/NodeID)
  (input layers-node g/NodeID)
  (input layouts-node g/NodeID)
  (input spine-scenes-node g/NodeID)
  (input dep-build-targets g/Any :array)
  (input project-settings g/Any)
  (input display-profiles g/Any)
  (input current-layout g/Str)
  (output current-layout g/Str (g/fnk [current-layout visible-layout] (or current-layout visible-layout)))
  (input node-msgs g/Any)
  (output node-msgs g/Any (gu/passthrough node-msgs))
  (input node-rt-msgs g/Any)
  (output node-rt-msgs g/Any (gu/passthrough node-rt-msgs))
  (input node-overrides g/Any)
  (output node-overrides g/Any :cached (gu/passthrough node-overrides))
  (input font-msgs g/Any :array)
  (input texture-msgs g/Any :array)
  (input layer-msgs g/Any)
  (output layer-msgs g/Any (gu/passthrough layer-msgs))
  (input layout-msgs g/Any :array)
  (input layout-rt-msgs g/Any :array)
  (input spine-scene-msgs g/Any :array)
  (input node-ids IDMap)
  (output node-ids IDMap (gu/passthrough node-ids))
  (input layout-names g/Str :array)

  (input aux-texture-gpu-textures GuiResourceTextures :array)
  (input texture-gpu-textures GuiResourceTextures :array)
  (output texture-gpu-textures GuiResourceTextures :cached (g/fnk [aux-texture-gpu-textures texture-gpu-textures] (into {} (concat aux-texture-gpu-textures texture-gpu-textures))))
  (input aux-texture-anim-datas TextureAnimDatas :array)
  (input texture-anim-datas TextureAnimDatas :array)
  (output texture-anim-datas TextureAnimDatas :cached (g/fnk [aux-texture-anim-datas texture-anim-datas] (into {} (concat aux-texture-anim-datas texture-anim-datas))))
  (input aux-texture-names GuiResourceNames :array)
  (input texture-names GuiResourceNames :array)
  (output texture-names GuiResourceNames :cached (g/fnk [aux-texture-names texture-names] (into (sorted-set) cat (concat aux-texture-names texture-names))))

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

  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (input samplers [g/KeywordMap])
  (output samplers [g/KeywordMap] (gu/passthrough samplers))
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
  (input build-errors g/Any :array)
  (output build-errors g/Any :cached produce-build-errors)
  (output build-targets g/Any :cached produce-build-targets)
  (input layout-node-outlines g/Any :array)
  (output layout-node-outlines g/Any :cached (g/fnk [layout-node-outlines] (into {} layout-node-outlines)))
  (input default-node-outline g/Any)
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id default-node-outline layout-node-outlines current-layout child-outlines]
                 (let [node-outline (get layout-node-outlines current-layout default-node-outline)]
                   {:node-id _node-id
                    :label (:label pb-def)
                    :icon (:icon pb-def)
                    :children (vec (sort-by :order (conj child-outlines node-outline)))})))
  (input default-scene g/Any)
  (input layout-scenes g/Any :array)
  (output layout-scenes g/Any :cached (g/fnk [layout-scenes] (into {} layout-scenes)))
  (output child-scenes g/Any :cached (g/fnk [default-scene layout-scenes current-layout]
                                       (let [node-tree-scene (get layout-scenes current-layout default-scene)]
                                         (:children node-tree-scene))))
  (output scene g/Any :cached produce-scene)
  (output template-scene g/Any :cached (g/fnk [scene child-scenes]
                                         (assoc scene :aabb (reduce geom/aabb-union (geom/null-aabb) (keep :aabb child-scenes)))))
  (output scene-dims g/Any :cached (g/fnk [project-settings current-layout display-profiles]
                                          (or (some #(and (= current-layout (:name %)) (first (:qualifiers %))) display-profiles)
                                              (let [w (get project-settings ["display" "width"])
                                                    h (get project-settings ["display" "height"])]
                                                {:width w :height h}))))
  (input id-prefix g/Str)
  (output id-prefix g/Str (gu/passthrough id-prefix)))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

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
         (g/connect font :node-outline fonts-node :child-outlines))))))

(defn- attach-texture
  ([self textures-node texture]
   (attach-texture self textures-node texture false))
  ([self textures-node texture internal?]
   (concat
     (g/connect texture :_node-id self :nodes)
     (g/connect texture :texture-gpu-textures self :texture-gpu-textures)
     (g/connect texture :texture-anim-datas self :texture-anim-datas)
     (when (not internal?)
       (concat
         (g/connect texture :texture-names self :texture-names)
         (g/connect texture :dep-build-targets self :dep-build-targets)
         (g/connect texture :pb-msg self :texture-msgs)
         (g/connect texture :build-errors textures-node :build-errors)
         (g/connect texture :node-outline textures-node :child-outlines)
         (g/connect self :samplers texture :samplers))))))

(defn- attach-layer
  ([layers-node layer]
   (attach-layer layers-node layer false))
  ([layers-node layer internal?]
   (concat
     (g/connect layer :_node-id layers-node :nodes)
     (when (not internal?)
       (concat
         (g/connect layer :name layers-node :ordered-layer-names)
         (g/connect layer :pb-msg layers-node :layer-msgs)
         (g/connect layer :build-errors layers-node :build-errors)
         (g/connect layer :node-outline layers-node :child-outlines))))))

(defn- attach-layout [self layouts-node layout]
  (concat
    (g/connect layout :build-errors layouts-node :build-errors)
    (g/connect layout :node-outline layouts-node :child-outlines)
    (for [[from to] [[:_node-id :nodes]
                     [:name :layout-names]
                     [:pb-msg :layout-msgs]
                     [:pb-rt-msg :layout-rt-msgs]
                     [:layout-node-outline :layout-node-outlines]
                     [:layout-scene :layout-scenes]]]
      (g/connect layout from self to))
    (for [[from to] [[:id-prefix :id-prefix]]]
      (g/connect self from layout to))))

(defn- attach-spine-scene
  ([self spine-scenes-node spine-scene]
   (attach-spine-scene self spine-scenes-node spine-scene false))
  ([self spine-scenes-node spine-scene internal?]
   (concat
     (g/connect spine-scene :_node-id self :nodes)
     (g/connect spine-scene :spine-scene-element-ids self :spine-scene-element-ids)
     (g/connect spine-scene :spine-scene-infos self :spine-scene-infos)
     (when (not internal?)
       (concat
         (g/connect spine-scene :spine-scene-names self :spine-scene-names)
         (g/connect spine-scene :dep-build-targets self :dep-build-targets)
         (g/connect spine-scene :pb-msg self :spine-scene-msgs)
         (g/connect spine-scene :build-errors spine-scenes-node :build-errors)
         (g/connect spine-scene :node-outline spine-scenes-node :child-outlines))))))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

(defn make-texture-node [self parent name resource]
  (g/make-nodes (g/node-id->graph-id self) [texture [TextureNode
                                                     :name name
                                                     :texture resource]]
    (attach-texture self parent texture)))

(defn- browse [title project exts]
  (seq (dialogs/make-resource-dialog (project/workspace project) project {:ext exts :title title :selection :multiple})))

(defn- resource->id [resource]
  (FilenameUtils/getBaseName ^String (resource/resource-name resource)))

(defn add-gui-node! [project scene parent node-type select-fn]
  (let [id (outline/resolve-id (subs (name node-type) 5) (keys (g/node-value scene :node-ids)))
        def-node-type (case node-type
                        :type-box BoxNode
                        :type-pie PieNode
                        :type-text TextNode
                        :type-template TemplateNode
                        :type-spine SpineNode
                        GuiNode)]
    (-> (concat
          (g/operation-label "Add Gui Node")
          (g/make-nodes (g/node-id->graph-id scene) [gui-node [def-node-type :id id :type node-type :size [200.0 100.0 0.0]]]
                        (attach-gui-node parent gui-node node-type)
                        (when select-fn
                          (select-fn [gui-node]))))
      g/transact
      g/tx-nodes-added
      first)))

(defn add-gui-node-handler [project {:keys [scene parent node-type]} select-fn]
  (add-gui-node! project scene parent node-type select-fn))

(defn- query-and-add-resources! [resources-type-label resource-exts taken-ids project select-fn make-node-fn]
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

(defn- outline-node-taken-ids [outline-node-id]
  (assert (g/node-instance? outline/OutlineNode outline-node-id))
  (map :label (:children (g/node-value outline-node-id :node-outline))))

(defn add-texture [scene textures-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [TextureNode :name name :texture resource]]
                (attach-texture scene textures-node node)))

(defn- add-textures-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
    "Textures" ["atlas" "tilesource"] (outline-node-taken-ids parent) project select-fn
    (partial add-texture scene parent)))

(defn add-font [scene fonts-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [FontNode :name name :font resource]]
                (attach-font scene fonts-node node)))

(defn- add-fonts-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
    "Fonts" ["font"] (outline-node-taken-ids parent) project select-fn
    (partial add-font scene parent)))

(defn add-layer! [project scene parent name select-fn]
  (g/transact
    (concat
      (g/operation-label "Add Layer")
      (g/make-nodes (g/node-id->graph-id scene) [node [LayerNode :name name]]
                    (attach-layer parent node)
                    (when select-fn
                      (select-fn [node]))))))

(defn- add-layer-handler [project {:keys [scene parent]} select-fn]
  (let [name (outline/resolve-id "layer" (outline-node-taken-ids parent))]
    (add-layer! project scene parent name select-fn)))

(defn add-layout-handler [project {:keys [scene parent display-profile]} select-fn]
  (g/transact
    (concat
      (g/operation-label "Add Layout")
      (g/make-nodes (g/node-id->graph-id scene) [node [LayoutNode :name display-profile]]
                    (attach-layout scene parent node)
                    (g/set-property node :nodes {})
                    (when select-fn
                      (select-fn [node]))))))

(defn add-spine-scene [scene spine-scenes-node resource name]
  (g/make-nodes (g/node-id->graph-id scene) [node [SpineSceneNode :name name :spine-scene resource]]
                (attach-spine-scene scene spine-scenes-node node)))

(defn- add-spine-scenes-handler [project {:keys [scene parent]} select-fn]
  (query-and-add-resources!
    "Spine Scenes" [spine/spine-scene-ext] (outline-node-taken-ids parent) project select-fn
    (partial add-spine-scene scene parent)))

(defn- make-add-handler [scene parent label icon handler-fn user-data]
  {:label label :icon icon :command :add
   :user-data (merge {:handler-fn handler-fn :scene scene :parent parent} user-data)})

(defn- add-handler-options [node]
  (let [types (protobuf/enum-values Gui$NodeDesc$Type)
        node (if (g/override? node) (g/override-original node) node)
        scene (node->gui-scene node)
        node-options (if (some #(g/node-instance? % node) [GuiSceneNode GuiNode NodeTree])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :node-tree)
                                      node)]
                         (mapv (fn [[type info]]
                                 (make-add-handler scene parent (:display-name info) (get node-icons type)
                                                   add-gui-node-handler {:node-type type}))
                           types))
                       [])
        texture-option (if (some #(g/node-instance? % node) [GuiSceneNode TexturesNode])
                         (let [parent (if (= node scene)
                                        (g/node-value scene :textures-node)
                                        node)]
                           (make-add-handler scene parent "Textures..." texture-icon add-textures-handler {})))
        font-option (if (some #(g/node-instance? % node) [GuiSceneNode FontsNode])
                      (let [parent (if (= node scene)
                                     (g/node-value scene :fonts-node)
                                     node)]
                        (make-add-handler scene parent "Fonts..." font-icon add-fonts-handler {})))
        layer-option (if (some #(g/node-instance? % node) [GuiSceneNode LayersNode])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :layers-node)
                                      node)]
                         (make-add-handler scene parent "Layer" layer-icon add-layer-handler {})))
        layout-option (if (some #(g/node-instance? % node) [GuiSceneNode LayoutsNode])
                        (let [parent (if (= node scene)
                                       (g/node-value scene :layouts-node)
                                       node)]
                          (make-add-handler scene parent "Layout" layout-icon add-layout-handler {:layout true})))
        spine-scene-option (if (some #(g/node-instance? % node) [GuiSceneNode SpineScenesNode])
                             (let [parent (if (= node scene)
                                            (g/node-value scene :spine-scenes-node)
                                            node)]
                               (make-add-handler scene parent "Spine Scenes..." spine/spine-scene-icon add-spine-scenes-handler {})))]
    (filter some? (conj node-options texture-option font-option layer-option layout-option spine-scene-option))))

(defn- unused-display-profiles [scene]
  (let [layouts (set (map :name (g/node-value scene :layout-msgs)))]
    (filter (complement layouts) (map :name (g/node-value scene :display-profiles)))))

(defn- add-layout-options [node user-data]
  (let [scene (node->gui-scene node)
        parent (if (= node scene)
                 (g/node-value scene :layouts-node)
                 node)]
    (mapv #(make-add-handler scene parent % layout-icon add-layout-handler {:display-profile %}) (unused-display-profiles scene))))

(handler/defhandler :add :workbench
  (active? [selection] (not-empty (some->> (handler/selection->node-id selection) add-handler-options)))
  (run [project user-data app-view] (when user-data ((:handler-fn user-data) project user-data (fn [node-ids] (app-view/select app-view node-ids)))))
  (options [selection user-data]
    (let [node-id (handler/selection->node-id selection)]
      (if (not user-data)
        (add-handler-options node-id)
        (when (:layout user-data)
          (add-layout-options node-id user-data))))))

(defn- color-alpha [node-desc color-field alpha-field]
  (let [color (get node-desc color-field)]
    (if (protobuf/field-set? node-desc alpha-field) (get node-desc alpha-field) (get color 3))))

(def node-property-fns (-> {}
                         (into (map (fn [label] [label [label (comp v4->v3 label)]]) [:position :rotation :scale :size]))
                         (conj [:rotation [:rotation (comp math/vecmath->clj math/euler->quat :rotation)]])
                         (into (map (fn [[label alpha]] [alpha [alpha (fn [n] (color-alpha n label alpha))]])
                                    [[:color :alpha]
                                     [:shadow :shadow-alpha]
                                     [:outline :outline-alpha]]))
                         (into (map (fn [[ddf-label label]] [ddf-label [label ddf-label]]) [[:xanchor :x-anchor]
                                                                                            [:yanchor :y-anchor]]))))

(defn- convert-node-desc [node-desc]
  (into {} (map (fn [[key val]] (let [[new-key f] (get node-property-fns key [key key])]
                                  [new-key (f node-desc)])) node-desc)))
(defn- sort-node-descs
  [node-descs]
  (let [parent-id->children (group-by :parent (remove #(str/blank? (:id %)) node-descs))
        parent->children #(parent-id->children (:id %))
        root {:id ""}]
    (rest (tree-seq parent->children parent->children root))))

(defn load-gui-scene [project self input]
  (let [def                pb-def
        scene              (protobuf/read-text (:pb-class def) input)
        resource           (g/node-value self :resource)
        resolve-fn         (partial workspace/resolve-resource resource)
        graph-id           (g/node-id->graph-id self)
        node-descs         (map convert-node-desc (:nodes scene))
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
                                                                  (rest (tree-seq (constantly true)
                                                                                  (comp tmpl-children first)
                                                                                  [r nil]))))])
                                         tmpl-roots))
        template-resources (keep (comp resolve-fn :template) (filter #(= :type-template (:type %)) node-descs))
        texture-resources  (keep (comp resolve-fn :texture) (:textures scene))
        scene-load-data  (project/load-resource-nodes (g/now) project
                                                      (->> (concat template-resources texture-resources)
                                                           (keep #(project/get-resource-node project %)))
                                                      progress/null-render-progress!)]
    (concat
      scene-load-data
      (g/set-property self :script (workspace/resolve-resource resource (:script scene)))
      (g/set-property self :material (workspace/resolve-resource resource (:material scene)))
      (g/set-property self :adjust-reference (:adjust-reference scene))
      (g/set-property self :pb scene)
      (g/set-property self :def def)
      (g/set-property self :background-color (:background-color scene))
      (g/set-property self :max-nodes (:max-nodes scene))
      (g/connect project :settings self :project-settings)
      (g/connect project :display-profiles self :display-profiles)
      (g/make-nodes graph-id [fonts-node FontsNode
                              no-font [FontNode
                                       :name ""
                                       :font (workspace/resolve-resource resource "/builtins/fonts/system_font.font")]]
                    (g/connect fonts-node :_node-id self :fonts-node)
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :build-errors self :build-errors)
                    (g/connect fonts-node :node-outline self :child-outlines)
                    (attach-font self fonts-node no-font true)
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [FontNode
                                                    :name (:name font-desc)
                                                    :font (workspace/resolve-resource resource (:font font-desc))]]
                                    (attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node TexturesNode
                              no-texture [InternalTextureNode
                                          :name ""
                                          :gpu-texture texture/white-pixel]]
                    (g/connect textures-node :_node-id self :textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :build-errors self :build-errors)
                    (g/connect textures-node :node-outline self :child-outlines)
                    (attach-texture self textures-node no-texture true)
                    (for [texture-desc (:textures scene)]
                      (let [resource (workspace/resolve-resource resource (:texture texture-desc))
                            outputs (some-> resource
                                            resource/resource-type
                                            :node-type
                                            g/output-labels)]
                        ;; Messy because we need to deal with legacy standalone image files
                        (if (or (nil? resource) ;; i.e. :texture field not set
                                (outputs :anim-data))
                          ;; TODO: have no tests for this
                          (g/make-nodes graph-id [texture [TextureNode :name (:name texture-desc) :texture resource]]
                                        (attach-texture self textures-node texture))
                          (g/make-nodes graph-id [img-texture [ImageTextureNode]
                                                  texture [TextureNode :name (:name texture-desc)]]
                                        (g/connect img-texture :_node-id texture :image-texture)
                                        (g/connect img-texture :packed-image texture :image)
                                        (g/connect img-texture :anim-data texture :anim-data)
                                        (project/connect-resource-node project resource img-texture [[:content :image]])
                                        (project/connect-resource-node project resource texture [[:resource :texture-resource]
                                                                                                 [:build-targets :dep-build-targets]])
                                        (attach-texture self textures-node texture))))))
      (g/make-nodes graph-id [spine-scenes-node SpineScenesNode
                              no-spine-scene [SpineSceneNode
                                              :name ""]]
                    (g/connect spine-scenes-node :_node-id self :spine-scenes-node)
                    (g/connect spine-scenes-node :_node-id self :nodes)
                    (g/connect spine-scenes-node :build-errors self :build-errors)
                    (g/connect spine-scenes-node :node-outline self :child-outlines)
                    (attach-spine-scene self spine-scenes-node no-spine-scene true)
                    (let [prop-keys (keys (g/public-properties SpineSceneNode))]
                      (for [spine-scene-desc (:spine-scenes scene)
                            :let [spine-scene-desc (select-keys spine-scene-desc prop-keys)]]
                        (g/make-nodes graph-id [spine-scene [SpineSceneNode
                                                             :name (:name spine-scene-desc)
                                                             :spine-scene (workspace/resolve-resource resource (:spine-scene spine-scene-desc))]]
                                      (attach-spine-scene self spine-scenes-node spine-scene)))))
      (g/make-nodes graph-id [layers-node LayersNode
                              no-layer [LayerNode
                                        :name ""]]
                    (g/connect layers-node :_node-id self :layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :layer-msgs self :layer-msgs)
                    (g/connect layers-node :layer-names self :layer-names)
                    (g/connect layers-node :layer->index self :layer->index)
                    (g/connect layers-node :build-errors self :build-errors)
                    (g/connect layers-node :node-outline self :child-outlines)
                    (attach-layer layers-node no-layer true)
                    (loop [[layer-desc & more] (:layers scene)
                           tx-data []]
                      (if layer-desc
                        (let [layer-tx-data (g/make-nodes graph-id
                                                          [layer [LayerNode :name (:name layer-desc)]]
                                                          (attach-layer layers-node layer))]
                          (recur more (conj tx-data layer-tx-data)))
                        tx-data)))
      (g/make-nodes graph-id [node-tree NodeTree]
                    (for [[from to] [[:_node-id :node-tree]
                                     [:_node-id :nodes]
                                     [:node-outline :default-node-outline]
                                     [:node-msgs :node-msgs]
                                     [:node-rt-msgs :node-rt-msgs]
                                     [:scene :default-scene]
                                     [:node-ids :node-ids]
                                     [:node-overrides :node-overrides]
                                     [:build-errors :build-errors]
                                     [:template-build-targets :template-build-targets]]]
                      (g/connect node-tree from self to))
                    (for [[from to] [[:material-shader :material-shader]
                                     [:texture-gpu-textures :texture-gpu-textures]
                                     [:texture-anim-datas :texture-anim-datas]
                                     [:texture-names :texture-names]
                                     [:font-shaders :font-shaders]
                                     [:font-datas :font-datas]
                                     [:font-names :font-names]
                                     [:layer-names :layer-names]
                                     [:layer->index :layer->index]
                                     [:spine-scene-element-ids :spine-scene-element-ids]
                                     [:spine-scene-infos :spine-scene-infos]
                                     [:spine-scene-names :spine-scene-names]
                                     [:id-prefix :id-prefix]
                                     [:current-layout :current-layout]]]
                      (g/connect self from node-tree to))
                    (loop [[node-desc & more] (sort-node-descs node-descs)
                           id->node {}
                           all-tx-data []]
                      (if node-desc
                        (let [node-type (case (:type node-desc)
                                          :type-box BoxNode
                                          :type-pie PieNode
                                          :type-text TextNode
                                          :type-template TemplateNode
                                          :type-spine SpineNode
                                          GuiNode)
                              props (-> node-desc
                                        (select-keys (keys (g/public-properties node-type)))
                                        (cond->
                                            (= :type-template (:type node-desc))
                                          (assoc :template {:resource (workspace/resolve-resource resource (:template node-desc))
                                                            :overrides (get template-data (:id node-desc) {})})))
                              tx-data (g/make-nodes graph-id [gui-node [node-type props]]
                                                    (let [parent (if (empty? (:parent node-desc)) node-tree (id->node (:parent node-desc)))]
                                                      (attach-gui-node parent gui-node (:type node-desc))))
                              node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
                          (recur more (assoc id->node (:id node-desc) node-id) (into all-tx-data tx-data)))
                        all-tx-data)))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                   (g/connect layouts-node :_node-id self :layouts-node)
                   (g/connect layouts-node :_node-id self :nodes)
                   (g/connect layouts-node :build-errors self :build-errors)
                   (g/connect layouts-node :node-outline self :child-outlines)
                   (let [prop-keys (keys (g/public-properties LayoutNode))]
                     (for [layout-desc (:layouts scene)
                           :let [layout-desc (-> layout-desc
                                               (select-keys prop-keys)
                                               (update :nodes (fn [nodes] (->> nodes
                                                                            (map (fn [v] [(:id v) (-> v extract-overrides convert-node-desc)]))
                                                                            (into {})))))]]
                       (g/make-nodes graph-id [layout [LayoutNode layout-desc]]
                                     (attach-layout self layouts-node layout))))))))

(defn- register [workspace def]
  (let [ext (:ext def)
        exts (if (vector? ext) ext [ext])]
    (for [ext exts]
      (workspace/register-resource-type workspace
                                        :textual? true
                                        :ext ext
                                        :label (:label def)
                                        :build-ext (:build-ext def)
                                        :node-type GuiSceneNode
                                        :load-fn load-gui-scene
                                        :icon (:icon def)
                                        :tags (:tags def)
                                        :tag-opts (:tag-opts def)
                                        :template (:template def)
                                        :view-types [:scene :text]
                                        :view-opts {:scene {:grid true}}))))

(defn register-resource-types [workspace]
  (register workspace pb-def))

(defn- vec-move
  [v x offset]
  (let [current-index (.indexOf ^java.util.List v x)
        new-index (max 0 (+ current-index offset))
        [before after] (split-at new-index (remove #(= x %) v))]
    (vec (concat before [x] after))))

(defn- move-node!
  [node-id offset]
  (let [parent (core/scope node-id)
        children (vec (g/node-value parent :nodes))
        new-children (vec-move children node-id offset)
        connections (keep (fn [[source source-label target target-label]]
                            (when (and (= source node-id)
                                       (= target parent))
                              [source-label target-label]))
                          (g/outputs node-id))]
    (g/transact
      (concat
        (for [child children
              [source target] connections]
          (g/disconnect child source parent target))
        (for [child new-children
              [source target] connections]
          (g/connect child source parent target))))))

(defn- selection->gui-node [selection]
  (handler/adapt-single selection GuiNode))

(defn- selection->layer-node [selection]
  (handler/adapt-single selection LayerNode))

(handler/defhandler :move-up :workbench
  (active? [selection] (or (selection->gui-node selection) (selection->layer-node selection)))
  (run [selection] (let [selected (handler/selection->node-id selection)]
                     (move-node! selected -1))))

(handler/defhandler :move-down :workbench
  (active? [selection] (or (selection->gui-node selection) (selection->layer-node selection)))
  (run [selection] (let [selected (handler/selection->node-id selection)]
                     (move-node! selected 1))))

(defn- resource->gui-scene [project resource]
  (let [res-node (some->> resource
                   (project/get-resource-node project))]
    (when (and res-node (g/node-instance? GuiSceneNode res-node))
      res-node)))

(handler/defhandler :set-gui-layout :workbench
  (active? [project active-resource] (boolean (resource->gui-scene project active-resource)))
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

(ui/extend-menu :toolbar :scale
                [{:label :separator}
                 {:icon layout-icon
                  :command :set-gui-layout
                  :label "Test"}])
