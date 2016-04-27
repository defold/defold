(ns editor.gui.visual-node
  (:require [dynamo.graph :as g]
            [editor.font :as font]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.gui.gui-scene-node :as gsn]
            [editor.gui.nodes :as nodes]
            [editor.properties :as properties]
            [editor.types :as types]
            [editor.gui.util :as gutil])
  (:import editor.gl.shader.ShaderLifecycle
           [javax.media.opengl GL GL2]
           [com.dynamo.gui.proto Gui$NodeDesc$AdjustMode Gui$NodeDesc$XAnchor Gui$NodeDesc$BlendMode Gui$NodeDesc$Pivot Gui$NodeDesc$YAnchor]))

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

(defn- ->uv-color-vtx-vb [vs uvs colors vcount]
  (let [vb (->uv-color-vtx vcount)
        vs (mapv (comp vec concat) vs uvs colors)]
    (doseq [v vs]
      (conj! vb v))
    (persistent! vb)))

(defn- premul [color]
  (let [[r g b a] color]
    [(* r a) (* g a) (* b a) a]))

(defn- gen-vb [^GL2 gl renderables]
  (let [user-data (get-in renderables [0 :user-data])]
    (cond
      (contains? user-data :geom-data)
      (let [[vs uvs colors] (reduce (fn [[vs uvs colors] renderable]
                                      (let [user-data (:user-data renderable)
                                            world-transform (:world-transform renderable)
                                            vcount (count (:geom-data user-data))]
                                        [(into vs (geom/transf-p world-transform (:geom-data user-data)))
                                         (into uvs (:uv-data user-data))
                                         (into colors (repeat vcount (premul (:color user-data))))]))
                                    [[] [] []] renderables)]
        (when (not-empty vs)
          (->uv-color-vtx-vb vs uvs colors (count vs))))

      (contains? user-data :text-data)
      (font/gen-vertex-buffer gl (get-in user-data [:text-data :font-data])
                              (map (fn [r] (let [alpha (get-in r [:user-data :color 3])
                                                 text-data (get-in r [:user-data :text-data])]
                                             (-> text-data
                                               (assoc :world-transform (:world-transform r)
                                                      :color (get-in r [:user-data :color]))
                                               (update-in [:outline 3] * alpha)
                                               (update-in [:shadow 3] * alpha))))
                                   renderables)))))

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

(defn render-nodes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (if (= pass pass/outline)
      (gsn/render-lines gl render-args renderables rcount)
      (render-tris gl render-args renderables rcount))))

(g/defnode VisualNode
  (inherits nodes/GuiNode)

  (property blend-mode g/Keyword (default :blend-mode-alpha)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$BlendMode))))
  (property adjust-mode g/Keyword (default :adjust-mode-fit)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$AdjustMode))))
  (property pivot g/Keyword (default :pivot-center)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$Pivot))))
  (property x-anchor g/Keyword (default :xanchor-none)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$XAnchor))))
  (property y-anchor g/Keyword (default :yanchor-none)
            (dynamic edit-type (g/fnk [] (properties/->pb-choicebox Gui$NodeDesc$YAnchor))))

  (input material-shader ShaderLifecycle)
  (input gpu-texture g/Any)

  (output aabb-size g/Any (g/fnk [size] size))
  (output aabb g/Any :cached (g/fnk [pivot aabb-size]
                                    (let [offset-fn (partial mapv + (gutil/pivot-offset pivot aabb-size))
                                          [min-x min-y _] (offset-fn [0 0 0])
                                          [max-x max-y _] (offset-fn aabb-size)]
                                      (-> (geom/null-aabb)
                                        (geom/aabb-incorporate min-x min-y 0)
                                        (geom/aabb-incorporate max-x max-y 0)))))
  (output scene-renderable-user-data g/Any :abstract)
  (output scene-renderable g/Any :cached
          (g/fnk [_node-id index layer-index blend-mode inherit-alpha gpu-texture material-shader scene-renderable-user-data]
                 {:render-fn render-nodes
                  :passes [pass/transparent pass/selection pass/outline]
                  :user-data (assoc scene-renderable-user-data
                                    :gpu-texture gpu-texture
                                    :inherit-alpha inherit-alpha
                                    :material-shader material-shader
                                    :blend-mode blend-mode)
                  :batch-key [gpu-texture blend-mode]
                  :select-batch-key _node-id
                  :index index
                  :layer-index (if layer-index (inc layer-index) 0)})))
