(ns editor.sprite
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.validation :as validation]
            [editor.resource :as resource]
            [internal.render.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.sprite.proto Sprite$SpriteDesc Sprite$SpriteDesc$BlendMode]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def sprite-icon "icons/16/Icons_14-Sprite.png")

; Render assets

(vtx/defvertex texture-vtx
  (vec3 position)
  (vec2 texcoord0 true))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader outline-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader outline-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

; TODO - macro of this
(def outline-shader (shader/make-shader ::outline-shader outline-vertex-shader outline-fragment-shader))

; Vertex generation

(defn- gen-vertex [^Matrix4d wt ^Point3d pt x y u v]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) u v])

(defn- conj-quad! [vbuf ^Matrix4d wt ^Point3d pt width height anim-uvs]
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

(defn- gen-vertex-buffer
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

(defn- gen-outline-vertex [^Matrix4d wt ^Point3d pt x y cr cg cb]
  (.set pt x y 0)
  (.transform wt pt)
  [(.x pt) (.y pt) (.z pt) cr cg cb 1])

(defn- conj-outline-quad! [vbuf ^Matrix4d wt ^Point3d pt width height cr cg cb]
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

(defn- gen-outline-vertex-buffer
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

(defn render-sprites [^GL2 gl render-args renderables count]
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

(g/defnk produce-save-data [_node-id resource image default-animation material blend-mode]
  {:resource resource
   :content (protobuf/map->str Sprite$SpriteDesc
              {:tile-set (resource/resource->proj-path image)
               :default-animation default-animation
               :material (resource/proj-path material)
               :blend-mode blend-mode})})

(defn anim-uvs [anim]
  (let [frame (first (:frames anim))]
    (:tex-coords frame)))

(g/defnk produce-scene
  [_node-id aabb gpu-texture animation blend-mode]
  (let [scene {:node-id _node-id
               :aabb aabb}]
    (if animation
      (let []
        (assoc scene :renderable {:render-fn render-sprites
                                  :batch-key gpu-texture
                                  :select-batch-key _node-id
                                  :user-data {:gpu-texture gpu-texture
                                              :anim-uvs (anim-uvs animation)
                                              :anim-width (:width animation 0)
                                              :anim-height (:height animation 0)
                                              :blend-mode blend-mode}
                                  :passes [pass/transparent pass/selection pass/outline]}))
     scene)))

(defn- build-sprite [self basis resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Sprite$SpriteDesc pb)}))

(g/defnk produce-build-targets [_node-id resource image default-animation material blend-mode dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:tile-set image] [:material material]])]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-sprite
      :user-data {:proto-msg {:tile-set (resource/proj-path image)
                              :default-animation default-animation
                              :material (resource/proj-path material)
                              :blend-mode blend-mode}
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(g/defnode SpriteNode
  (inherits project/ResourceNode)

  (property image (g/protocol resource/Resource)
            (value (gu/passthrough image-resource))
            (set (project/gen-resource-setter [[:resource :image-resource]
                                               [:anim-data :anim-data]
                                               [:gpu-texture :gpu-texture]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource image)))
  
  (property default-animation g/Str
            (validate (validation/validate-animation default-animation anim-data))
            (dynamic edit-type (g/fnk [anim-data] {:type :choicebox
                                                   :options (or (and anim-data (zipmap (keys anim-data) (keys anim-data))) {})})))
  (property material (g/protocol resource/Resource)
            (value (gu/passthrough material-resource))
            (set (project/gen-resource-setter [[:resource :material-resource]
                                               [:build-targets :dep-build-targets]]))
            (validate (validation/validate-resource material)))

  
  (property blend-mode g/Any (default :blend_mode_alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode Sprite$SpriteDesc$BlendMode))
            (dynamic edit-type (g/always
                                (let [options (protobuf/enum-values Sprite$SpriteDesc$BlendMode)]
                                  {:type :choicebox
                                   :options (zipmap (map first options)
                                                    (map (comp :display-name second) options))}))))

  (input image-resource (g/protocol resource/Resource))
  (input anim-data g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)

  (input material-resource (g/protocol resource/Resource))

  (output animation g/Any (g/fnk [anim-data default-animation] (get anim-data default-animation))) ; TODO - use placeholder animation
  (output aabb AABB (g/fnk [animation] (if animation
                                         (let [hw (* 0.5 (:width animation))
                                               hh (* 0.5 (:height animation))]
                                           (-> (geom/null-aabb)
                                             (geom/aabb-incorporate (Point3d. (- hw) (- hh) 0))
                                             (geom/aabb-incorporate (Point3d. hw hh 0))))
                                         (geom/null-aabb))))
  (output save-data g/Any :cached produce-save-data)
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-sprite [project self resource]
  (let [sprite   (protobuf/read-text Sprite$SpriteDesc resource)
        image    (workspace/resolve-resource resource (:tile-set sprite))
        material (workspace/resolve-resource resource (:material sprite))]
    (concat
      (g/set-property self :image image)
      (g/set-property self :default-animation (:default-animation sprite))
      (g/set-property self :material material)
      (g/set-property self :blend-mode (:blend-mode sprite)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "sprite"
                                    :node-type SpriteNode
                                    :load-fn load-sprite
                                    :icon sprite-icon
                                    :view-types [:scene]
                                    :tags #{:component}
                                    :label "Sprite"))
