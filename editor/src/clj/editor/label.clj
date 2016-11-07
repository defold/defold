(ns editor.label
  (:require [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.font :as font]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.validation :as validation]
            [editor.resource :as resource]
            [editor.gl.pass :as pass]
            [editor.gl.texture :as texture]
            [editor.types :as types]
            [editor.material :as material])
  (:import [com.dynamo.label.proto Label$LabelDesc Label$LabelDesc$BlendMode Label$LabelDesc$Pivot]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]
           [editor.gl.shader ShaderLifecycle]))

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

; Vertex generation

#_(defn- gen-vertex [^Matrix4d wt ^Point3d pt x y u v]
   (.set pt x y 0)
   (.transform wt pt)
   [(.x pt) (.y pt) (.z pt) u v])

#_(defn- conj-quad! [vbuf ^Matrix4d wt ^Point3d pt width height anim-uvs]
   (let [x1 (* 0.5 width)
         y1 (* 0.5 height)
         x0 (- x1)
         y0 (- y1)
         [[u0 v0] [u1 v1] [u2 v2] [u3 v3]] anim-uvs]
     (-> vbuf
       (conj! (gen-vertex wt pt x0 y0 u0 v0))
       (conj! (gen-vertex wt pt x1 y0 u3 v3))
       (conj! (gen-vertex wt pt x0 y1 u1 v1))
       (conj! (gen-vertex wt pt x1 y0 u3 v3))
       (conj! (gen-vertex wt pt x1 y1 u2 v2))
       (conj! (gen-vertex wt pt x0 y1 u1 v1)))))

#_(defn- gen-vertex-buffer
   [renderables count]
   (let [tmp-point (Point3d.)]
     (loop [renderables renderables
           vbuf (->texture-vtx (* count 6))]
       (if-let [renderable (first renderables)]
         (let [world-transform (:world-transform renderable)
               user-data (:user-data renderable)
               anim-uvs (:anim-uvs user-data)
               anim-width (:anim-width user-data)
               anim-height (:anim-height user-data)]
           (recur (rest renderables) (conj-quad! vbuf world-transform tmp-point anim-width anim-height anim-uvs)))
         (persistent! vbuf)))))

#_(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
   (.set pt x y 0)
   (.transform wt pt)
   [(.x pt) (.y pt) (.z pt) cr cg cb 1])

#_(defn- conj-outline-quad! [vbuf ^Matrix4d wt ^Point3d pt width height cr cg cb]
   (let [x1 (* 0.5 width)
         y1 (* 0.5 height)
         x0 (- x1)
         y0 (- y1)
         v0 (gen-outline-vertex wt pt x0 y0 cr cg cb)
         v1 (gen-outline-vertex wt pt x1 y0 cr cg cb)
         v2 (gen-outline-vertex wt pt x1 y1 cr cg cb)
         v3 (gen-outline-vertex wt pt x0 y1 cr cg cb)]
     (-> vbuf (conj! v0) (conj! v1) (conj! v1) (conj! v2) (conj! v2) (conj! v3) (conj! v3) (conj! v0))))

(def outline-color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-outline-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(defn- ->color-vtx-vb [vs colors vcount]
  (let [vb (->color-vtx vcount)
        vs (mapv (comp vec concat) vs colors)]
    (doseq [v vs]
      (conj! vb v))
    (persistent! vb)))

#_(defn- gen-outline-vertex-buffer
   [renderables count]
   (let [tmp-point (Point3d.)]
     (loop [renderables renderables
            vbuf (->color-vtx (* count 8))]
       (if-let [renderable (first renderables)]
         (let [color (if (:selected renderable) selected-outline-color outline-color)
               cr (get color 0)
               cg (get color 1)
               cb (get color 2)
               world-transform (:world-transform renderable)
               user-data (:user-data renderable)
               anim-width (:anim-width user-data)
               anim-height (:anim-height user-data)]
           (recur (rest renderables) (conj-outline-quad! vbuf world-transform tmp-point anim-width anim-height cr cg cb)))
         (persistent! vbuf)))))

; Rendering

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

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])]
    (font/gen-vertex-buffer gl (get-in user-data [:text-data :font-data])
                            (map (fn [r] (let [text-data (get-in r [:user-data :text-data])
                                               alpha (get-in text-data [:color 3])]
                                           (-> text-data
                                             (assoc :world-transform (:world-transform r))
                                             (update-in [:outline 3] * alpha)
                                             (update-in [:shadow 3] * alpha))))
                                 renderables))))

(defn render-tris [^GL2 gl render-args renderables rcount]
  (let [user-data (get-in renderables [0 :user-data])
        gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
        material-shader (get user-data :material-shader)
        blend-mode (get user-data :blend-mode)
        vb (gen-vb gl renderables)
        vcount (count vb)]
    (when (> vcount 0)
      (let [shader material-shader #_(if (types/selection? (:pass render-args))
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

(defn render-labels [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (if (= pass pass/outline)
      (render-lines gl render-args renderables rcount)
      (render-tris gl render-args renderables rcount))))

#_(defn render-sprites [^GL2 gl render-args renderables count]
   (let [pass (:pass render-args)]
     (cond
       (= pass pass/outline)
       (let [outline-vertex-binding (vtx/use-with ::sprite-outline (gen-outline-vertex-buffer renderables count) outline-shader)]
         (gl/with-gl-bindings gl render-args [outline-shader outline-vertex-binding]
           (gl/gl-draw-arrays gl GL/GL_LINES 0 (* count 8))))

       (= pass pass/transparent)
       (let [vertex-binding (vtx/use-with ::sprite-trans (gen-vertex-buffer renderables count) shader)
             user-data (:user-data (first renderables))
             gpu-texture (:gpu-texture user-data)
             blend-mode (:blend-mode user-data)]
         (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
           (case blend-mode
             :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
             (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
             :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
           (shader/set-uniform shader gl "texture" 0)
           (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* count 6))
           (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))

       (= pass pass/selection)
       (let [vertex-binding (vtx/use-with ::sprite-selection (gen-vertex-buffer renderables count) shader)]
         (gl/with-gl-bindings gl render-args [shader vertex-binding]
           (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* count 6)))))))

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
  (conj v 0))

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

(g/defnk produce-save-data [_node-id resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Label$LabelDesc pb-msg)})

(g/defnk produce-scene
  [_node-id aabb gpu-texture material-shader blend-mode pivot text-data]
  (let [scene {:node-id _node-id
               :aabb aabb}]
    (if text-data
      (let [min (types/min-p aabb)
            max (types/max-p aabb)
            size [(- (.x max) (.x min)) (- (.y max) (.y min)) 0]
            [w h _] size
            offset (pivot-offset pivot size)
            lines (mapv conj (apply concat (take 4 (partition 2 1 (cycle (geom/transl offset [[0 0] [w 0] [w h] [0 h]]))))) (repeat 0))]
        (assoc scene :renderable {:render-fn render-labels
                                  :batch-key {:gpu-texture gpu-texture :blend-mode blend-mode}
                                  :select-batch-key _node-id
                                  :user-data {:material-shader material-shader
                                              :blend-mode blend-mode
                                              :gpu-texture gpu-texture
                                              :line-data lines
                                              :text-data text-data}
                                  :passes [pass/transparent pass/selection pass/outline]}))
      scene)))

(defn- build-label [self basis resource dep-resources user-data]
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
  (inherits project/ResourceNode)

  (property text g/Str)
  (property size types/Vec3)
  (property scale types/Vec3)
  (property color types/Color)
  (property outline types/Color)
  (property shadow types/Color)
  (property leading g/Num)
  (property tracking g/Num)
  (property pivot g/Keyword (default :pivot-center)
            (dynamic edit-type (g/constantly
                                (let [options (protobuf/enum-values Label$LabelDesc$Pivot)]
                                  {:type :choicebox
                                   :options (zipmap (map first options)
                                                    (map (comp :display-name second) options))}))))
  (property blend-mode g/Any (default :blend_mode_alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode Label$LabelDesc$BlendMode))
            (dynamic edit-type (g/constantly
                                (let [options (protobuf/enum-values Label$LabelDesc$BlendMode)]
                                  {:type :choicebox
                                   :options (zipmap (map first options)
                                                    (map (comp :display-name second) options))}))))
  (property line-break g/Bool)
    
  (property font resource/Resource
            (value (gu/passthrough font-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
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
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
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
  (output save-data g/Any :cached produce-save-data)
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-texture g/Any :cached (g/fnk [_node-id gpu-texture material-samplers]
                                           (->> (material/sampler->tex-params (first material-samplers))
                                             (texture/set-params gpu-texture)))))

(defn load-label [project self resource]
  (let [label (protobuf/read-text Label$LabelDesc resource)
        label (reduce (fn [label k] (update label k v4->v3)) label [:size :scale])
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
  (workspace/register-resource-type workspace
                                    :ext "label"
                                    :node-type LabelNode
                                    :load-fn load-label
                                    :icon label-icon
                                    :view-types [:scene :text]
                                    :tags #{:component}
                                    :label "Label"))
