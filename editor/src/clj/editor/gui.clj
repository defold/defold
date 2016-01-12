(ns editor.gui
  (:require [clojure.string :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.core :as core]
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
            [editor.ui :as ui]
            [editor.font :as font]
            [editor.dialogs :as dialogs]
            [editor.outline :as outline]
            [editor.material :as material]
            [editor.validation :as validation])
  (:import [com.dynamo.gui.proto Gui$SceneDesc Gui$SceneDesc$AdjustReference Gui$NodeDesc$Type Gui$NodeDesc$XAnchor Gui$NodeDesc$YAnchor
            Gui$NodeDesc$Pivot Gui$NodeDesc$AdjustMode Gui$NodeDesc$BlendMode Gui$NodeDesc$ClippingMode Gui$NodeDesc$PieBounds]
           [editor.types AABB]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.vecmath Matrix4d Point3d Quat4d]
           [java.awt.image BufferedImage]
           [com.defold.editor.pipeline TextureSetGenerator$UVTransform]
           [org.apache.commons.io FilenameUtils]
           [editor.gl.shader ShaderLifecycle]))

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

(def ^:private node-icons {:type-box box-icon
                           :type-pie pie-icon
                           :type-text text-icon
                           :type-template template-icon})

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
        (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))))

(defn- premul [color]
  (let [[r g b a] color]
    [(* r a) (* g a) (* b a) a]))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])]
    (if (contains? user-data :geom-data)
      (let [[vs uvs colors] (reduce (fn [[vs uvs colors] renderable]
                                      (let [user-data (:user-data renderable)
                                            world-transform (:world-transform renderable)
                                            vcount (count (:geom-data user-data))]
                                        [(into vs (geom/transf-p world-transform (:geom-data user-data)))
                                         (into uvs (:uv-data user-data))
                                         (into colors (repeat vcount (premul (:color user-data))))]))
                                    [[] [] []] renderables)]
        (->uv-color-vtx-vb vs uvs colors (count vs)))
      (if (contains? user-data :text-data)
        (font/gen-vertex-buffer gl (get-in user-data [:text-data :font-data]) (map (fn [r] (let [alpha (get-in r [:user-data :color 3])
                                                                                                 text-data (get-in r [:user-data :text-data])]
                                                                                             (-> text-data
                                                                                               (assoc :world-transform (:world-transform r)
                                                                                                      :color (get-in r [:user-data :color]))
                                                                                               (update-in [:outline 3] * alpha)
                                                                                               (update-in [:shadow 3] * alpha))))
                                                                                   renderables))
        nil))))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [user-data (get-in renderables [0 :user-data])
        gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
        material-shader (get user-data :material-shader)
        blend-mode (get user-data :blend-mode)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (let [shader (if (types/selection? (:pass render-args))
                     shader ;; TODO - Always use the hard-coded shader for selection, DEFEDIT-231 describes a fix for this
                     (or material-shader shader))
            vertex-binding (vtx/use-with ::tris vb shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
          (case blend-mode
            :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
            :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))))

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

(defn- sort-by-angle [ps]
  (-> (sort-by (fn [[x y _]] (let [a (Math/atan2 y x)]
                              (cond-> a
                                (< a 0) (+ (* 2.0 Math/PI))))) ps)
    (vec)))

(defn- cornify [ps max-angle]
  (let [corner-count (int (/ (+ max-angle 45) 90))]
    (if (> corner-count 0)
      (-> ps
        (into (geom/chain (dec corner-count) (partial geom/rotate [0 0 90]) (geom/rotate [0 0 45] [[1 0 0]])))
        (sort-by-angle))
      ps)))

(defn- pie-circling [segments max-angle corners? ps]
  (let [cut-off? (< max-angle 360)
        angle (/ 360 segments)
        segments (if cut-off?
                   (int (/ max-angle angle))
                   segments)]
    (cond-> (geom/chain segments (partial geom/rotate [0 0 angle]) ps)
      corners? (cornify max-angle)
      cut-off? (into (geom/rotate [0 0 max-angle] ps)))))

(defn- pairs [v]
  (filter (fn [[v0 v1]] (> (Math/abs (double (- v1 v0))) 0)) (partition 2 1 v)))

(g/defnk produce-node-scene [_node-id type index layer-index aabb transform pivot size color blend-mode slice9 pie-data text-data inherit-alpha texture gpu-texture anim-data material-shader child-scenes]
  (let [min (types/min-p aabb)
        max (types/max-p aabb)
        size (if (= type :type-text) [(- (.x max) (.x min)) (- (.y max) (.y min))] size)
        [w h _] size
        offset (pivot-offset pivot size)
        userdata (if (= type :type-text)
                   (let [lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
                     (when-let [font-data (get text-data :font-data)]
                       {:text-data (assoc text-data :offset (let [[x y] offset]
                                                              [x (+ y (- h (get-in font-data [:font-map :max-ascent])))]))
                        :line-data lines
                        :color color}))
                   (let [[geom-data uv-data line-data] (case type
                                                         :type-box (let [order [0 1 3 3 1 2]
                                                                         x-vals (pairs [0 (get slice9 0) (- w (get slice9 2)) w])
                                                                         y-vals (pairs [0 (get slice9 3) (- h (get slice9 1)) h])
                                                                         corners (for [[x0 x1] x-vals
                                                                                       [y0 y1] y-vals]
                                                                                   (geom/transl offset [[x0 y0 0] [x0 y1 0] [x1 y1 0] [x1 y0 0]]))
                                                                         vs (vec (mapcat #(map (partial nth %) order) corners))
                                                                         tex (get anim-data texture)
                                                                         tex-w (:width tex 1)
                                                                         tex-h (:height tex 1)
                                                                         u-vals (pairs [0 (/ (get slice9 0) tex-w) (- 1 (/ (get slice9 2) tex-w)) 1])
                                                                         v-vals (pairs [1 (- 1 (/ (get slice9 3) tex-h)) (/ (get slice9 1) tex-h) 0])
                                                                         uv-trans (get-in tex [:uv-transforms 0])
                                                                         uvs (for [[u0 u1] u-vals
                                                                                   [v0 v1] v-vals]
                                                                               (geom/uv-trans uv-trans [[u0 v0] [u0 v1] [u1 v1] [u1 v0]]))
                                                                         uvs (vec (mapcat #(map (partial nth %) order) uvs))
                                                                         lines (vec (mapcat #(interleave % (drop 1 (cycle %))) corners))]
                                                                     [vs uvs lines])
                                                         :type-pie (let [offset (mapv + offset [(* 0.5 w) (* 0.5 h) 0])
                                                                         {:keys [outer-bounds inner-radius perimeter-vertices pie-fill-angle]} pie-data
                                                                         outer-rect? (= :piebounds-rectangle outer-bounds)
                                                                         cut-off? (< pie-fill-angle 360)
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
                                                                         vs-inner (if (> inner-radius 0)
                                                                                    (let [xs (/ inner-radius w)
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
                                                                               (geom/uv-trans (get-in anim-data [texture :uv-transforms 0])))
                                                                         vs (->> vs
                                                                              (geom/scale [(* 0.5 w) (* 0.5 h) 1])
                                                                              (geom/transl offset))]
                                                                     [vs uvs lines])
                                                         [[] [] []])]
                     {:geom-data geom-data
                      :line-data line-data
                      :uv-data uv-data
                      :color color}))
        userdata (assoc userdata
                        :gpu-texture gpu-texture
                        :inherit-alpha inherit-alpha
                        :material-shader material-shader
                        :blend-mode blend-mode)]
    {:node-id _node-id
     :aabb aabb
     :transform transform
     :renderable {:render-fn render-nodes
                  :passes [pass/transparent pass/selection pass/outline]
                  :user-data userdata
                  :batch-key [gpu-texture blend-mode]
                  :select-batch-key _node-id
                  :index index
                  :layer-index (if layer-index (inc layer-index) 0)}
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

(g/defnk produce-node-msg [type parent index _declared-properties]
  (let [pb-renames {:x-anchor :xanchor
                    :y-anchor :yanchor
                    :outer-bounds :outerBounds
                    :inner-radius :innerRadius
                    :perimeter-vertices :perimeterVertices
                    :pie-fill-angle :pieFillAngle}
        v3-fields [:position :rotation :scale :size]
        props (:properties _declared-properties)
        msg (-> (into {:parent parent
                       :type type
                       :index index}
                      (map (fn [[k v]] [k (:value v)])
                           (filter (fn [[k v]] (and (get v :visible true)
                                                    (not (contains? (set (keys pb-renames)) k))))
                                   props)))
              (into (map (fn [[k v]] [v (get-in props [k :value])]) pb-renames)))
        msg (reduce (fn [msg k] (update msg k v3->v4)) msg v3-fields)]
    msg))

(defn- attach-gui-node [self parent gui-node type]
  (concat
    (g/connect parent :id gui-node :parent)
    (g/connect gui-node :_node-id self :nodes)
    (g/connect gui-node :node-outline parent :child-outlines)
    (g/connect gui-node :scene parent :child-scenes)
    (g/connect gui-node :index parent :child-indices)
    (g/connect gui-node :pb-msg self :node-msgs)
    (g/connect gui-node :id self :node-ids)
    (g/connect self :layer-ids gui-node :layer-ids)
    (g/connect self :textures gui-node :textures)
    (case type
      (:type-box :type-pie) (concat
                              (g/connect self :textures gui-node :textures)
                              (g/connect self :material-shader gui-node :material-shader))
      :type-text (g/connect self :font-ids gui-node :font-ids)
      [])))

(def GuiSceneNode nil)

(defn- node->gui-scene [node]
  (if (g/node-instance? GuiSceneNode node)
    node
    (let [[_ _ scene _] (first (filter (fn [[_ label _ _]] (= label :_node-id)) (g/outputs node)))]
      scene)))

(def ^:private font-connections [[:name :font-input]
                                 [:gpu-texture :gpu-texture]
                                 [:font-map :font-map]
                                 [:font-data :font-data]
                                 [:font-shader :material-shader]])
(def ^:private layer-connections [[:name :layer-input]
                                  [:index :layer-index]])

(g/defnode GuiNode
  (inherits scene/ScalableSceneNode)
  (inherits outline/OutlineNode)

  (property index g/Int (dynamic visible (g/always false)) (default 0))
  (property type g/Keyword (dynamic visible (g/always false)))
  (property animation g/Str (dynamic visible (g/always false)) (default ""))

  (property id g/Str (default ""))
  (property size types/Vec3 (dynamic visible box-pie-text?) (default [0 0 0]))
  (property color types/Color (dynamic visible box-pie-text?) (default [1 1 1 1]))
  (property alpha g/Num (default 1.0)
    (value (g/fnk [color] (get color 3)))
    (set (fn [basis self _ new-value]
          (g/update-property self :color (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  ; Template
  (property template g/Str (dynamic visible template?))

  (property inherit-alpha g/Bool (default true))

  ; Text
  (property text g/Str (dynamic visible text?))
  (property line-break g/Bool (dynamic visible text?) (default false))
  (property font g/Str (dynamic visible text?)
    (dynamic edit-type (g/fnk [font-ids] (properties/->choicebox (map first font-ids))))
    (value (g/fnk [font-input] font-input))
    (set (fn [basis self _ new-value]
           (let [font-ids (g/node-value self :font-ids :basis basis)]
             (concat
               (for [label (map second font-connections)]
                 (g/disconnect-sources self label))
               (if (contains? font-ids new-value)
                 (let [font-node (font-ids new-value)]
                   (for [[from to] font-connections]
                     (g/connect font-node from self to)))
                 []))))))
  (property outline types/Color (dynamic visible text?) (default [1 1 1 1]))
  (property outline-alpha g/Num (dynamic visible text?) (default 1.0)
    (value (g/fnk [outline] (get outline 3)))
    (set (fn [basis self _ new-value]
          (g/update-property self :outline (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))
  (property shadow types/Color (dynamic visible text?) (default [1 1 1 1]))
  (property shadow-alpha g/Num (dynamic visible text?) (default 1.0)
    (value (g/fnk [shadow] (get shadow 3)))
    (set (fn [basis self _ new-value]
          (g/update-property self :shadow (fn [v] (assoc v 3 new-value)))))
    (dynamic edit-type (g/always {:type :slider
                                  :min 0.0
                                  :max 1.0
                                  :precision 0.01})))

  (property texture g/Str
            (dynamic edit-type (g/fnk [textures] (properties/->choicebox (cons "" (keys textures)))))
            (dynamic visible box-pie?)
            (value (g/fnk [texture-input animation]
                     (str texture-input (if (and animation (not (empty? animation))) (str "/" animation) ""))))
            (set (fn [basis self _ ^String new-value]
                   (let [textures (g/node-value self :textures :basis basis)
                         animation (let [sep (.indexOf new-value "/")]
                                     (if (>= sep 0) (subs new-value (inc sep)) ""))]
                     (concat
                       (g/set-property self :animation animation)
                       (for [label [:texture-input :gpu-texture :anim-data]]
                         (g/disconnect-sources self label))
                       (if (contains? textures new-value)
                         (let [tex-node (textures new-value)]
                           (concat
                             (g/connect tex-node :name self :texture-input)
                             (g/connect tex-node :gpu-texture self :gpu-texture)
                             (g/connect tex-node :anim-data self :anim-data)))
                         []))))))
  (property slice9 types/Vec4 (dynamic visible box?) (default [0 0 0 0]))

  ; Pie
  (property outer-bounds g/Keyword (default :piebounds-ellipse)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$PieBounds)))
            (dynamic visible pie?))
  (property inner-radius g/Num (dynamic visible pie?) (default 0.0))
  (property perimeter-vertices g/Num (dynamic visible pie?) (default 10.0))
  (property pie-fill-angle g/Num (dynamic visible pie?) (default 360.0))

  (property layer g/Str
            (dynamic edit-type (g/fnk [layer-ids] (properties/->choicebox (cons "" (map first layer-ids)))))
            (value (g/fnk [layer-input] (or layer-input "")))
            (set (fn [basis self _ new-value]
                   (let [layer-ids (g/node-value self :layer-ids :basis basis)]
                     (concat
                       (for [label (map second layer-connections)]
                         (g/disconnect-sources self label))
                       (if (contains? layer-ids new-value)
                         (let [layer-node (layer-ids new-value)]
                           (for [[from to] layer-connections]
                             (g/connect layer-node from self to)))
                         []))))))
  (property blend-mode g/Keyword (default :blend-mode-alpha)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$BlendMode)))
            (dynamic visible box-pie-text?))

  ; Box/Pie/Text
  (property adjust-mode g/Keyword (default :adjust-mode-fit)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$AdjustMode)))
            (dynamic visible box-pie-text?))
  (property pivot g/Keyword (default :pivot-center)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$Pivot)))
            (dynamic visible box-pie-text?))
  (property x-anchor g/Keyword (default :xanchor-none)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$XAnchor)))
            (dynamic visible box-pie-text?))
  (property y-anchor g/Keyword (default :yanchor-none)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$YAnchor)))
            (dynamic visible box-pie-text?))

  ; Box/Pie
  (property clipping-mode g/Keyword (default :clipping-mode-none)
            (dynamic edit-type (g/always (properties/->pb-choicebox Gui$NodeDesc$ClippingMode)))
            (dynamic visible box-pie?))
  (property clipping-visible g/Bool (dynamic visible box-pie?) (default true))
  (property clipping-inverted g/Bool (dynamic visible box-pie?) (default false))

  (display-order [:id scene/ScalableSceneNode])

  (input parent g/Str)
  (input layer-ids {g/Str g/NodeID})
  (input layer-input g/Str)
  (input layer-index g/Int)
  (input texture-input g/Str)
  (input font-input g/Str)
  (input font-map g/Any)
  (input font-data font/FontData)
  (input gpu-texture g/Any)
  (input anim-data g/Any)
  (input textures {g/Str g/NodeID})
  (input font-ids {g/Str g/NodeID})
  (input material-shader ShaderLifecycle)
  (input child-scenes g/Any :array)
  (input child-indices g/Int :array)
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id id index child-outlines type]
      (let [reqs (if (= type :type-template)
                   []
                   [{:node-type GuiNode
                     :tx-attach-fn (fn [target source]
                                     (let [scene (node->gui-scene target)
                                           type (g/node-value source :type)]
                                       (concat
                                         (g/update-property source :id outline/resolve-id (g/node-value scene :node-ids))
                                         (attach-gui-node scene target source type))))}])]
        {:node-id _node-id
         :label id
         :index index
         :icon (case type
                 :type-box box-icon
                 :type-text text-icon
                 :type-pie pie-icon
                 :type-template template-icon)
         :child-reqs reqs
         :copy-include-fn (fn [node]
                            (let [node-id (g/node-id node)]
                              (and (g/node-instance? GuiNode node-id)
                                (not= node-id (g/node-value node-id :parent)))))
         :children (vec (sort-by :index child-outlines))})))
  (output pb-msg g/Any produce-node-msg)
  (output aabb g/Any (g/fnk [pivot size type font-map text line-break]
                            (let [size (if (= type :type-text)
                                         (font/measure font-map text line-break (first size))
                                         size)
                                  offset-fn (partial mapv + (pivot-offset pivot size))
                                  [min-x min-y _] (offset-fn [0 0 0])
                                  [max-x max-y _] (offset-fn size)]
                              (-> (geom/null-aabb)
                                (geom/aabb-incorporate min-x min-y 0)
                                (geom/aabb-incorporate max-x max-y 0)))))
  (output scene g/Any :cached produce-node-scene)
  (output pie-data {g/Keyword g/Any} (g/fnk [outer-bounds inner-radius perimeter-vertices pie-fill-angle]
                                       {:outer-bounds outer-bounds :inner-radius inner-radius
                                        :perimeter-vertices perimeter-vertices :pie-fill-angle pie-fill-angle}))
  (output text-data {g/Keyword g/Any} (g/fnk [text font-data line-break outline shadow size]
                                        {:text text :font-data font-data
                                         :line-break line-break :outline outline :shadow shadow :max-width (first size)})))

(g/defnode ImageTextureNode
  (input image BufferedImage)
  (output packed-image BufferedImage (g/fnk [image] image))
  (output anim-data g/Any (g/fnk [^BufferedImage image]
                            {nil {:width (.getWidth image)
                                  :height (.getHeight image)
                                  :frames [{:tex-coords [[0 1] [0 0] [1 0] [1 1]]}]
                                  :uv-transforms [(TextureSetGenerator$UVTransform.)]}})))

(g/defnode TextureNode
  (inherits outline/OutlineNode)

  (property name g/Str)
  (property texture (g/protocol resource/Resource)
            (value (gu/passthrough texture-resource))
            (set (project/gen-resource-setter [[:resource :texture-resource]
                                               [:packed-image :image]
                                               [:anim-data :anim-data]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource texture)))
  
  (input texture-resource (g/protocol resource/Resource))
  (input image BufferedImage)
  (input anim-data g/Any)
  (input image-texture g/NodeID :cascade-delete)
  (input samplers [{g/Keyword g/Any}])

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any (g/fnk [dep-build-targets] dep-build-targets))
  
  (output anim-data g/Any :cached (g/fnk [_node-id name anim-data]
                                    (into {} (map (fn [[id data]] [(if id (format "%s/%s" name id) name) data]) anim-data))))
  (output node-outline outline/OutlineData (g/fnk [_node-id name]
                                             {:node-id _node-id
                                              :label name
                                              :icon texture-icon}))
  (output pb-msg g/Any (g/fnk [name texture-resource]
                         {:name name
                          :texture (proj-path texture-resource)}))
  (output texture-data g/Any (g/fnk [_node-id anim-data]
                               [_node-id (keys anim-data)]))
  (output gpu-texture g/Any :cached (g/fnk [_node-id image samplers]
                                           (let [params (material/sampler->tex-params (first samplers))]
                                             (texture/set-params (texture/image-texture _node-id image) params)))))

(g/defnode FontNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property font (g/protocol resource/Resource)
            (value (gu/passthrough font-resource))
            (set (project/gen-resource-setter [[:resource :font-resource]
                                               [:font-map :font-map]
                                               [:font-data :font-data]
                                               [:gpu-texture :gpu-texture]
                                               [:material-shader :font-shader]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource font)))
            
  (input font-resource (g/protocol resource/Resource))
  (input font-map g/Any)
  (input font-data font/FontData)
  (input font-shader ShaderLifecycle)
  (input gpu-texture g/Any)

  (input dep-build-targets g/Any)
  (output dep-build-targets g/Any :cached (g/fnk [dep-build-targets] dep-build-targets))
  
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon font-icon}))
  (output pb-msg g/Any (g/fnk [name font-resource]
                              {:name name
                               :font (proj-path font-resource)}))
  (output font-map g/Any (g/fnk [font-map] font-map))
  (output font-data font/FontData (g/fnk [font-data] font-data))
  (output gpu-texture g/Any (g/fnk [gpu-texture] gpu-texture))
  (output font-shader ShaderLifecycle (g/fnk [font-shader] font-shader))
  (output font-id {g/Str g/NodeID} (g/fnk [_node-id name] {name _node-id})))

(g/defnode LayerNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property index g/Int (dynamic visible (g/always false)) (default 0))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name index]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon layer-icon
                                                           :index index}))
  (output pb-msg g/Any (g/fnk [name index]
                              {:name name
                               :index index}))
  (output layer-id {g/Str g/NodeID} (g/fnk [_node-id name] {name _node-id})))

(g/defnode LayoutNode
  (inherits outline/OutlineNode)
  (property name g/Str)
  (property nodes g/Any)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id name]
                                                          {:node-id _node-id
                                                           :label name
                                                           :icon layout-icon}))
  (output pb-msg g/Any
          (g/fnk [name nodes]
                 {:name name
                  :nodes nodes})))

(g/defnode NodesNode
  (inherits outline/OutlineNode)

  (property id g/Str (default "") (dynamic visible (g/always false)))
  (input child-scenes g/Any :array)
  (input child-indices g/Int :array)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                          {:node-id _node-id
                                                           :label "Nodes"
                                                           :icon virtual-icon
                                                           :order 0
                                                           :child-reqs [{:node-type GuiNode
                                                                         :tx-attach-fn (fn [target source]
                                                                                         (let [scene (node->gui-scene target)
                                                                                               type (g/node-value source :type)]
                                                                                           (concat
                                                                                             (g/update-property source :id outline/resolve-id (g/node-value scene :node-ids))
                                                                                             (attach-gui-node scene target source type))))}]
                                                           :children (vec (sort-by :index child-outlines))}))
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (map :aabb child-scenes))
                                      :children child-scenes})))

(g/defnode TexturesNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                          {:node-id _node-id
                                                           :label "Textures"
                                                           :icon virtual-icon
                                                           :order 1
                                                           :children child-outlines})))

(g/defnode FontsNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                          {:node-id _node-id
                                                           :label "Fonts"
                                                           :icon virtual-icon
                                                           :order 2
                                                           :children child-outlines})))

(g/defnode LayersNode
  (inherits outline/OutlineNode)
  (input child-indices g/Int :array)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                          {:node-id _node-id
                                                           :label "Layers"
                                                           :icon virtual-icon
                                                           :order 3
                                                           :children (vec (sort-by :index child-outlines))})))

(g/defnode LayoutsNode
  (inherits outline/OutlineNode)
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                          {:node-id _node-id
                                                           :label "Layouts"
                                                           :icon virtual-icon
                                                           :order 4
                                                           :children child-outlines})))

(defn- apply-alpha [parent-alpha scene]
  (let [scene-alpha (get-in scene [:renderable :user-data :color 3] 1.0)]
    (if (get-in scene [:renderable :user-data :inherit-alpha])
      (let [alpha (* parent-alpha scene-alpha)]
        (-> scene
          (assoc-in [:renderable :user-data :color 3] alpha)
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

(g/defnk produce-pb-msg [script-resource material-resource adjust-reference node-msgs layer-msgs font-msgs texture-msgs layout-msgs]
  {:script (proj-path script-resource)
   :material (proj-path material-resource)
   :adjust-reference adjust-reference
   :nodes (map #(dissoc % :index) (sort-by :index node-msgs))
   :layers (map #(dissoc % :index) (sort-by :index layer-msgs))
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

(g/defnk produce-outline [_node-id child-outlines]
  {:node-id _node-id
   :label (:label pb-def)
   :icon (:icon pb-def)
   :children (vec (sort-by :order child-outlines))})

(g/defnode GuiSceneNode
  (inherits project/ResourceNode)

  (property script (g/protocol resource/Resource)
            (value (gu/passthrough script-resource))
            (set (project/gen-resource-setter [[:resource :script-resource]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource script)))

  
  (property material (g/protocol resource/Resource)
    (value (gu/passthrough material-resource))
    (set (project/gen-resource-setter [[:resource :material-resource]
                                       [:shader :material-shader]
                                       [:samplers :samplers]
                                       [:build-targets :dep-build-targets]]))
    (validate (validation/validate-resource material)))
  
  (property adjust-reference g/Keyword (dynamic edit-type (g/always (properties/->pb-choicebox Gui$SceneDesc$AdjustReference))))
  (property pb g/Any (dynamic visible (g/always false)))
  (property def g/Any (dynamic visible (g/always false)))

  (input script-resource (g/protocol resource/Resource))

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
  (input child-scenes g/Any :array)
  (input node-ids g/Str :array)
  (input texture-names g/Str :array)
  (input font-names g/Str :array)
  (input layer-names g/Str :array)
  (input layout-names g/Str :array)

  (input texture-data g/Any :array)
  (input font-ids {g/Str g/NodeID} :array)
  (input layer-ids {g/Str g/NodeID} :array)

  (input material-resource (g/protocol resource/Resource))
  (input material-shader ShaderLifecycle)
  (output material-shader ShaderLifecycle (g/fnk [material-shader] material-shader))
  (input samplers [{g/Keyword g/Any}])
  (output samplers [{g/Keyword g/Any}] (g/fnk [samplers] samplers))
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
  (output node-outline outline/OutlineData :cached produce-outline)
  (output scene-dims g/Any :cached (g/fnk [project-settings]
                                          (let [w (get project-settings ["display" "width"])
                                                h (get project-settings ["display" "height"])]
                                            {:width w :height h})))
  (output layers [g/Str] :cached (g/fnk [layers] layers))
  (output textures {g/Str g/NodeID} :cached (g/fnk [texture-data]
                                              (into {} (mapcat (fn [[_node-id anims]] (map #(vector % _node-id) anims)) texture-data))))
  (output font-ids {g/Str g/NodeID} :cached (g/fnk [font-ids] (into {} font-ids)))
  (output layer-ids {g/Str g/NodeID} :cached (g/fnk [layer-ids] (into {} layer-ids))))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

(defn- attach-font
  ([self fonts-node font]
    (attach-font self fonts-node font false))
  ([self fonts-node font default?]
    (concat
      (g/connect font :_node-id self :nodes)
      (g/connect font :font-id self :font-ids)
      (g/connect font :dep-build-targets self :dep-build-targets)
      (if (not default?)
        (concat
          (g/connect font :name self :font-names)
          (g/connect font :pb-msg self :font-msgs)
          (g/connect font :node-outline fonts-node :child-outlines))
        []))))

(defn- attach-texture [self textures-node texture]
  (concat
    (g/connect texture :_node-id self :nodes)
    (g/connect texture :texture-data self :texture-data)
    (g/connect texture :dep-build-targets self :dep-build-targets)
    (g/connect texture :pb-msg self :texture-msgs)
    (g/connect texture :name self :texture-names)
    (g/connect texture :node-outline textures-node :child-outlines)
    (g/connect self :samplers texture :samplers)))

(defn- attach-layer [self layers-node layer]
  (concat
    (g/connect layer :_node-id self :nodes)
    (g/connect layer :layer-id self :layer-ids)
    (g/connect layer :pb-msg self :layer-msgs)
    (g/connect layer :name self :layer-names)
    (g/connect layer :node-outline layers-node :child-outlines)
    (g/connect layer :index layers-node :child-indices)))

(defn- attach-layout [self layouts-node layout]
  (concat
    (g/connect layout :_node-id self :nodes)
    (g/connect layout :node-outline layouts-node :child-outlines)
    (g/connect layout :name self :layout-names)
    (g/connect layout :pb-msg self :layout-msgs)))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

(defn make-texture-node [self parent name resource]
  (g/make-nodes (g/node-id->graph-id self) [texture [TextureNode
                                                     :name name
                                                     :texture resource]]
    (attach-texture self parent texture)))

(defn- browse [project exts]
  (first (dialogs/make-resource-dialog (project/workspace project) {:ext exts})))

(defn- resource->id [resource]
  (FilenameUtils/getBaseName ^String (resource/resource-name resource)))

(defn- add-gui-node-handler [project {:keys [scene parent node-type]}]
  (let [index (inc (reduce max 0 (g/node-value parent :child-indices)))
        id (outline/resolve-id (subs (name node-type) 5) (g/node-value scene :node-ids))]
    (g/transact
      (concat
        (g/operation-label "Add Gui Node")
        (g/make-nodes (g/node-id->graph-id scene) [gui-node [GuiNode :id id :index index :type node-type :size [200.0 100.0 0.0]]]
          (attach-gui-node scene parent gui-node node-type)
          (if (= node-type :type-text)
            (g/set-property gui-node :text "<text>" :font "")
            [])
          (project/select project [gui-node]))))))

(defn- add-texture-handler [project {:keys [scene parent node-type]}]
  (when-let [resource (browse project ["atlas" "tilesource"])]
    (let [name (outline/resolve-id (resource->id resource) (g/node-value scene :texture-names))]
      (g/transact
        (concat
          (g/operation-label "Add Texture")
          (g/make-nodes (g/node-id->graph-id scene) [node [TextureNode :name name :texture resource]]
            (attach-texture scene parent node)
            (project/select project [node])))))))

(defn- add-font-handler [project {:keys [scene parent node-type]}]
  (when-let [resource (browse project ["font"])]
    (let [name (outline/resolve-id (resource->id resource) (g/node-value scene :font-names))]
      (g/transact
        (concat
          (g/operation-label "Add Font")
          (g/make-nodes (g/node-id->graph-id scene) [node [FontNode :name name :font resource]]
            (attach-font scene parent node)
            (project/select project [node])))))))

(defn- add-layer-handler [project {:keys [scene parent node-type]}]
  (let [index (inc (reduce max 0 (g/node-value parent :child-indices)))
        name (outline/resolve-id "layer" (g/node-value scene :layer-names))]
    (g/transact
      (concat
        (g/operation-label "Add Layer")
        (g/make-nodes (g/node-id->graph-id scene) [node [LayerNode :name name :index index]]
          (attach-layer scene parent node)
          (project/select project [node]))))))

(defn- add-layout-handler [project {:keys [scene parent node-type]}]
  (let [name (outline/resolve-id "layout" (g/node-value scene :layout-names))]
    (g/transact
      (concat
        (g/operation-label "Add Layout")
        (g/make-nodes (g/node-id->graph-id scene) [node [LayoutNode :name name]]
          (attach-layout scene parent node)
          (project/select project [node]))))))

(defn- add-handler-options [node]
  (let [types (protobuf/enum-values Gui$NodeDesc$Type)
        scene (node->gui-scene node)
        node-options (if (some #(g/node-instance? % node) [GuiSceneNode GuiNode NodesNode])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :nodes-node)
                                      node)]
                         (mapv (fn [[type info]] {:label (:display-name info)
                                                  :icon (get node-icons type)
                                                  :command :add
                                                  :user-data {:handler-fn add-gui-node-handler
                                                              :scene scene
                                                              :parent parent
                                                              :node-type type}})
                           types))
                       [])
        texture-option (if (some #(g/node-instance? % node) [GuiSceneNode TexturesNode])
                         (let [parent (if (= node scene)
                                        (g/node-value scene :textures-node)
                                        node)]
                           {:label "Texture"
                            :icon texture-icon
                            :command :add
                            :user-data {:handler-fn add-texture-handler
                                        :scene scene
                                        :parent parent}}))
        font-option (if (some #(g/node-instance? % node) [GuiSceneNode FontsNode])
                      (let [parent (if (= node scene)
                                     (g/node-value scene :fonts-node)
                                     node)]
                        {:label "Font"
                         :icon font-icon
                         :command :add
                         :user-data {:handler-fn add-font-handler
                                     :scene scene
                                     :parent parent}}))
        layer-option (if (some #(g/node-instance? % node) [GuiSceneNode LayersNode])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :layers-node)
                                      node)]
                         {:label "Layer"
                          :icon layer-icon
                          :command :add
                          :user-data {:handler-fn add-layer-handler
                                      :scene scene
                                      :parent parent}}))
        layout-option (if (some #(g/node-instance? % node) [GuiSceneNode LayoutsNode])
                        (let [parent (if (= node scene)
                                       (g/node-value scene :layouts-node)
                                       node)]
                          {:label "Layout"
                           :icon layout-icon
                           :command :add
                           :user-data {:handler-fn add-layout-handler
                                       :scene scene
                                       :parent parent}}))]
    (filter some? (conj node-options texture-option font-option layer-option layout-option))))

(handler/defhandler :add :global
  (active? [selection] (and (= 1 (count selection))
                         (not-empty (add-handler-options (first selection)))))
  (run [project user-data] (when user-data ((:handler-fn user-data) project user-data)))
  (options [selection user-data]
    (when (not user-data)
      (add-handler-options (first selection)))))

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
      (g/make-nodes graph-id [fonts-node FontsNode]
                    (g/connect fonts-node :_node-id self :fonts-node)
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :node-outline self :child-outlines)
                    (g/make-nodes graph-id [font [FontNode
                                                  :name ""
                                                  :font (workspace/resolve-resource resource "/builtins/fonts/system_font.font")]]
                                  (attach-font self fonts-node font true))
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [FontNode
                                                    :name (:name font-desc)
                                                    :font (workspace/resolve-resource resource (:font font-desc))]]
                                    (attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node TexturesNode]
                    (g/connect textures-node :_node-id self :textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :node-outline self :child-outlines)
                    (for [texture-desc (:textures scene)]
                      (let [resource (workspace/resolve-resource resource (:texture texture-desc))
                            type (resource/resource-type resource)
                            outputs (g/declared-outputs (:node-type type))]
                        ;; Messy because we need to deal with legacy standalone image files
                        (if (outputs :anim-data)
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
      (g/make-nodes graph-id [layers-node LayersNode]
                    (g/connect layers-node :_node-id self :layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :node-outline self :child-outlines)
                    (loop [layer-descs (:layers scene)
                           tx-data []
                           index 0]
                      (if-let [layer-desc (first layer-descs)]
                        (let [tx-data (conj tx-data (g/make-nodes graph-id [layer [LayerNode
                                                                                :name (:name layer-desc)
                                                                                :index index]]
                                                               (attach-layer self layers-node layer)))]
                          (recur (rest layer-descs) tx-data (inc index)))
                        tx-data)))
      (g/make-nodes graph-id [layouts-node LayoutsNode]
                   (g/connect layouts-node :_node-id self :layouts-node)
                   (g/connect layouts-node :_node-id self :nodes)
                   (g/connect layouts-node :node-outline self :child-outlines)
                   (for [layout-desc (:layouts scene)]
                     (g/make-nodes graph-id [layout [LayoutNode
                                                     :name (:name layout-desc)
                                                     :nodes (:nodes layout-desc)]]
                       (attach-layout self layouts-node layout))))
      (g/make-nodes graph-id [nodes-node NodesNode]
        (g/connect nodes-node :_node-id self :nodes-node)
        (g/connect nodes-node :_node-id self :nodes)
        (g/connect nodes-node :node-outline self :child-outlines)
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
                                                            :x-anchor (:xanchor node-desc)
                                                            :y-anchor (:yanchor node-desc)
                                                            :pivot (:pivot node-desc)
                                                            :outline outline
                                                            :shadow shadow
                                                            :adjust-mode (:adjust-mode node-desc)
                                                            :line-break (:line-break node-desc)
                                                            :alpha (:alpha node-desc)
                                                            :inherit-alpha (:inherit-alpha node-desc)
                                                            :slice9 (:slice9 node-desc)
                                                            :outer-bounds (:outerBounds node-desc)
                                                            :inner-radius (:innerRadius node-desc)
                                                            :perimeter-vertices (:perimeterVertices node-desc)
                                                            :pie-fill-angle (:pieFillAngle node-desc)
                                                            :clipping-mode (:clipping-mode node-desc)
                                                            :clipping-visible (:clipping-visible node-desc)
                                                            :clipping-inverted (:clipping-inverted node-desc)
                                                            :alpha (:alpha node-desc)
                                                            :template (:template node-desc)]]
                            (let [parent (if (empty? (:parent node-desc)) nodes-node (id->node (:parent node-desc)))]
                              (attach-gui-node self parent gui-node (:type node-desc)))
                            ; Needs to be done after attaching so textures etc can be fetched
                            (g/set-property gui-node
                              :texture (:texture node-desc)
                              :font (:font node-desc)
                              :layer (:layer node-desc)))
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
                              (when (= output :node-outline) [target input]))
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

(defn- single-gui-node? [selection]
  (handler/single-selection? selection GuiNode))

(defn- single-layer-node? [selection]
  (handler/single-selection? selection LayerNode))

(handler/defhandler :move-up :global
  (enabled? [selection] (or (single-gui-node? selection) (single-layer-node? selection)))
  (run [selection] (let [selected (first selection)
                         [target input] (outline-parent selected)]
                     (outline-move! (g/node-value target input) selected -1))))

(handler/defhandler :move-down :global
  (enabled? [selection] (or (single-gui-node? selection) (single-layer-node? selection)))
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
