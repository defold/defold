(ns editor.label
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.material :as material]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.label.proto Label$LabelDesc Label$LabelDesc$BlendMode Label$LabelDesc$Pivot]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [javax.vecmath Point3d Vector3d]))

(set! *warn-on-reflection* true)

(def label-icon "icons/32/Icons_39-GUI-Text-node.png")

; Render assets

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

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

; Vertex generation

(def outline-color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-outline-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(defn- gen-lines-vb
  [renderables]
  (let [vcount (transduce (map (comp count :line-data :user-data)) + renderables)]
    (when (pos? vcount)
      (vtx/flip! (reduce (fn [vb {:keys [world-transform user-data selected] :as renderable}]
                           (let [line-data (:line-data user-data)
                                 [r g b a] (get user-data :line-color (if (:selected renderable) selected-outline-color outline-color))]
                             (reduce (fn [vb [x y z]]
                                       (color-vtx-put! vb x y z r g b a))
                                     vb
                                     (geom/transf-p world-transform line-data))))
                         (->color-vtx vcount)
                         renderables)))))

(defn render-lines [^GL2 gl render-args renderables rcount]
  (when-let [vb (gen-lines-vb renderables)]
    (let [vertex-binding (vtx/use-with ::lines vb line-shader)]
      (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])
        font-data (get-in user-data [:text-data :font-data])
        text-entries (mapv (fn [r] (let [text-data (get-in r [:user-data :text-data])
                                         alpha (get-in text-data [:color 3])]
                                     (-> text-data
                                         (assoc :world-transform (:world-transform r))
                                         (update-in [:outline 3] * alpha)
                                         (update-in [:shadow 3] * alpha))))
                           renderables)
        node-ids (into #{} (map :node-id) renderables)]
    (font/request-vertex-buffer gl node-ids font-data text-entries)))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [user-data (get-in renderables [0 :user-data])
        gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
        material-shader (get user-data :material-shader)
        blend-mode (get user-data :blend-mode)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (let [shader (or material-shader shader)
            vertex-binding (vtx/use-with ::tris vb shader)]
        (gl/with-gl-bindings gl render-args [shader vertex-binding gpu-texture]
          (case blend-mode
            :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
            :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))))

(defn render-labels [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (if (= pass pass/outline)
      (render-lines gl render-args renderables rcount)
      (render-tris gl render-args renderables rcount))))

; Node defs

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

(defn- v3->v4 [v]
  (conj v 0.0))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

(g/defnk produce-pb-msg [text size scale color outline shadow leading tracking pivot blend-mode line-break font material]
  {:text text
   :size (v3->v4 size)
   :scale (v3->v4 scale)
   :color color
   :outline outline
   :shadow shadow
   :leading leading
   :tracking tracking
   :pivot pivot
   :blend-mode blend-mode
   :line-break line-break
   :font (resource/resource->proj-path font)
   :material (resource/resource->proj-path material)})

(g/defnk produce-scene
  [_node-id aabb gpu-texture material-shader blend-mode pivot text-data scale]
  (let [scene {:node-id _node-id
               :aabb aabb
               :transform (math/->mat4-scale scale)}]
    (if text-data
      (let [min (types/min-p aabb)
            max (types/max-p aabb)
            size [(- (.x max) (.x min)) (- (.y max) (.y min)) 0]
            [w h _] size
            offset (pivot-offset pivot size)
            lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
        (assoc scene :renderable {:render-fn render-labels
                                  :batch-key {:blend-mode blend-mode :gpu-texture gpu-texture :material-shader material-shader}
                                  :select-batch-key _node-id
                                  :user-data {:material-shader material-shader
                                              :blend-mode blend-mode
                                              :gpu-texture gpu-texture
                                              :line-data lines
                                              :text-data text-data}
                                  :passes [pass/transparent pass/selection pass/outline]}))
      scene)))

(defn- build-label [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Label$LabelDesc pb)}))

(g/defnk produce-build-targets [_node-id resource font material pb-msg dep-build-targets]
  (or (when-let [errors (->> [[font :font "Font"]
                              [material :material "Material"]]
                          (keep (fn [[v prop-kw name]]
                                  (validation/prop-error :fatal _node-id prop-kw validation/prop-nil? v name)))
                          not-empty)]
        (g/error-aggregate errors))
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
            dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:font font] [:material material]])]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-label
          :user-data {:proto-msg pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets}])))

(g/defnode LabelNode
  (inherits resource-node/ResourceNode)

  (property text g/Str
            (dynamic edit-type (g/constantly {:type :multi-line-text})))
  (property size types/Vec3)
  (property scale types/Vec3)
  (property color types/Color)
  (property outline types/Color)
  (property shadow types/Color)
  (property leading g/Num)
  (property tracking g/Num)
  (property pivot g/Keyword (default :pivot-center)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Label$LabelDesc$Pivot))))
  (property blend-mode g/Any (default :blend-mode-alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode Label$LabelDesc$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Label$LabelDesc$BlendMode))))
  (property line-break g/Bool)
    
  (property font resource/Resource
            (value (gu/passthrough font-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :font-resource]
                                            [:gpu-texture :gpu-texture]
                                            [:font-map :font-map]
                                            [:font-data :font-data]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id font]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? font "Font")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? font "Font"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["font"]})))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id material]
                                  (or (validation/prop-error :info _node-id :image validation/prop-nil? material "Material")
                                      (validation/prop-error :fatal _node-id :image validation/prop-resource-not-exists? material "Material"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext ["material"]})))

  (input font-resource resource/Resource)
  (input material-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input gpu-texture g/Any)
  (input font-map g/Any)
  (input font-data font/FontData)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output text-layout g/Any :cached (g/fnk [size font-map text line-break leading tracking]
                                           (font/layout-text font-map text line-break (first size) tracking leading)))
  (output text-data g/KeywordMap (g/fnk [text-layout font-data line-break color outline shadow aabb-size pivot]
                                        (cond-> {:text-layout text-layout
                                                 :font-data font-data
                                                 :color color
                                                 :outline outline
                                                 :shadow shadow
                                                 :align (pivot->h-align pivot)}
                                          font-data (assoc :offset (let [[x y] (pivot-offset pivot aabb-size)
                                                                         h (second aabb-size)]
                                                                     [x (+ y (- h (get-in font-data [:font-map :max-ascent])))])))))
  (output aabb-size g/Any :cached (g/fnk [text-layout]
                                         [(:width text-layout) (:height text-layout) 0]))
  (output aabb g/Any :cached (g/fnk [pivot aabb-size]
                                    (let [offset-fn (partial mapv + (pivot-offset pivot aabb-size))
                                          [min-x min-y _] (offset-fn [0 0 0])
                                          [max-x max-y _] (offset-fn aabb-size)]
                                      (-> (geom/null-aabb)
                                        (geom/aabb-incorporate min-x min-y 0)
                                        (geom/aabb-incorporate max-x max-y 0)))))
  (output save-value g/Any (gu/passthrough pb-msg))
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets)
  (output tex-params g/Any :cached (g/fnk [material-samplers]
                                     (some-> material-samplers first material/sampler->tex-params)))
  (output gpu-texture g/Any :cached (g/fnk [_node-id gpu-texture tex-params]
                                      (texture/set-params gpu-texture tex-params))))

(defmethod scene-tools/manip-scalable? ::LabelNode [_node-id] true)

(defmethod scene-tools/manip-scale ::LabelNode [evaluation-context node-id ^Vector3d delta]
  (let [[sx sy sz] (g/node-value node-id :scale evaluation-context)
        new-scale [(* sx (.x delta)) (* sy (.y delta)) (* sz (.z delta))]]
    (g/set-property node-id :scale (properties/round-vec new-scale))))

(defn load-label [project self resource label]
  (let [label (reduce (fn [label k] (update label k v4->v3)) label [:size :scale])
        font (workspace/resolve-resource resource (:font label))
        material (workspace/resolve-resource resource (:material label))]
    (concat
      (g/set-property self
                      :text (:text label)
                      :size (:size label)
                      :scale (:scale label)
                      :color (:color label)
                      :outline (:outline label)
                      :shadow (:shadow label)
                      :leading (:leading label)
                      :tracking (:tracking label)
                      :pivot (:pivot label)
                      :blend-mode (:blend-mode label)
                      :line-break (:line-break label)
                      :font font
                      :material material))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "label"
    :node-type LabelNode
    :ddf-type Label$LabelDesc
    :load-fn load-label
    :icon label-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}
    :label "Label"))
