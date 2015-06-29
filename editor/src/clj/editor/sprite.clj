(ns editor.sprite
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
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

(def sprite-icon "icons/pictures.png")

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

(def shader (shader/make-shader vertex-shader fragment-shader))

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

(def outline-shader (shader/make-shader outline-vertex-shader outline-fragment-shader))

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
      (let [outline-vertex-binding (vtx/use-with (gen-outline-vertex-buffer renderables count) outline-shader)]
        (gl/with-enabled gl [outline-shader outline-vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 (* count 8))))

      (= pass pass/transparent)
      (let [vertex-binding (vtx/use-with (gen-vertex-buffer renderables count) shader)
            user-data (:user-data (first renderables))
            gpu-texture (:gpu-texture user-data)
            blend-mode (:blend-mode user-data)]
        (gl/with-enabled gl [gpu-texture shader vertex-binding]
          (case blend-mode
            :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
            :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
          (shader/set-uniform shader gl "texture" 0)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* count 6))
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))

      (= pass pass/selection)
      (let [vertex-binding (vtx/use-with (gen-vertex-buffer renderables count) shader)]
        (gl/with-enabled gl [shader vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* count 6)))))))

; Node defs

(g/defnk produce-save-data [resource image default-animation material blend-mode]
  {:resource resource
   :content (protobuf/map->str Sprite$SpriteDesc
              {:tile-set (workspace/proj-path image)
               :default-animation default-animation
               :material (workspace/proj-path material)
               :blend-mode blend-mode})})

(defn anim-uvs [anim]
  (let [frame (first (:frames anim))]
    (:tex-coords frame)))

(g/defnk produce-scene
  [node-id aabb gpu-texture animation blend-mode]
  (let [scene {:node-id node-id
               :aabb aabb}]
    (if animation
      (let []
        (assoc scene :renderable {:render-fn render-sprites
                                  :batch-key gpu-texture
                                  :select-batch-key node-id
                                  :user-data {:gpu-texture gpu-texture
                                              :anim-uvs (anim-uvs animation)
                                              :anim-width (:width animation 0)
                                              :anim-height (:height animation 0)
                                              :blend-mode blend-mode}
                                  :passes [pass/transparent pass/selection pass/outline]}))
     scene)))

(defn- build-sprite [self basis resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (workspace/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes Sprite$SpriteDesc pb)}))

(g/defnk produce-build-targets [node-id resource image default-animation material blend-mode dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:tile-set image] [:material material]])]
    [{:node-id node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-sprite
      :user-data {:proto-msg {:tile-set (workspace/proj-path image)
                              :default-animation default-animation
                              :material (workspace/proj-path material)
                              :blend-mode blend-mode}
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(defn- connect-atlas [project self image]
  (if-let [atlas-node (project/get-resource-node project image)]
    (let [outputs (-> atlas-node g/node-type g/output-labels)]
      (if (every? #(contains? outputs %) [:anim-data :gpu-texture :build-targets])
        [(g/connect atlas-node :anim-data self :anim-data)
         (g/connect atlas-node :gpu-texture self :gpu-texture)
         (g/connect atlas-node :build-targets self :dep-build-targets)]
        []))
    []))

(defn- disconnect-all [self label]
  (let [sources (g/sources-of self label)]
    (for [[src-node src-label] sources]
      (g/disconnect src-node src-label self label))))

(defn reconnect [transaction graph self label kind labels]
  (when (some #{:image} labels)
    (let [image (:image self)
          project (project/get-project self)]
      (concat
        (disconnect-all self :anim-data)
        (disconnect-all self :gpu-texture)
        (disconnect-all self :dep-build-targets)
        (connect-atlas project self image)))))

(g/defnode SpriteNode
  (inherits project/ResourceNode)

  (property image (g/maybe (g/protocol workspace/Resource)))
  (property default-animation g/Str)
  (property material (g/protocol workspace/Resource))
  (property blend-mode g/Any (default :BLEND_MODE_ALPHA) #_(tag Sprite$SpriteDesc$BlendMode))

  (trigger reconnect :property-touched #'reconnect)

  (input anim-data g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)

  (output animation g/Any (g/fnk [anim-data default-animation] (get anim-data default-animation))) ; TODO - use placeholder animation
  (output aabb AABB (g/fnk [animation] (if animation
                                         (let [hw (* 0.5 (:width animation))
                                               hh (* 0.5 (:height animation))]
                                           (-> (geom/null-aabb)
                                             (geom/aabb-incorporate (Point3d. (- hw) (- hh) 0))
                                             (geom/aabb-incorporate (Point3d. hw hh 0))))
                                         (geom/null-aabb))))
  (output outline g/Any :cached (g/fnk [node-id] {:node-id node-id :label "Sprite" :icon sprite-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-sprite [project self input]
  (let [sprite (protobuf/read-text Sprite$SpriteDesc input)
        resource (:resource self)
        image (workspace/resolve-resource resource (:tile-set sprite))
        material (workspace/resolve-resource resource (:material sprite))]
    (concat
      (g/set-property self :image image)
      (g/set-property self :default-animation (:default-animation sprite))
      (g/set-property self :material material)
      (g/set-property self :blend-mode (:blend-mode sprite))
      (connect-atlas project self image)
      (if-let [material-node (project/get-resource-node project material)]
        (g/connect material-node :build-targets self :dep-build-targets)
        []))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "sprite"
                                    :node-type SpriteNode
                                    :load-fn load-sprite
                                    :icon sprite-icon
                                    :view-types [:scene]
                                    :tags #{:component}
                                    :template "templates/template.sprite"))
