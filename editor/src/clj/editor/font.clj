(ns editor.font
  (:require [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.pipeline.font-gen :as font-gen]
            [editor.image :as image]
            [editor.resource :as resource]
            [editor.properties :as properties]
            [editor.material :as material]
            [internal.render.pass :as pass])
  (:import [com.dynamo.render.proto Font$FontDesc Font$FontMap Font$FontTextureFormat]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [editor.gl.shader ShaderLifecycle]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private resource-fields #{:font :material})

(vtx/defvertex DefoldVertex
  (vec3 position)
  (vec2 texcoord0)
  (vec4 face_color)
  (vec4 outline_color)
  (vec4 shadow_color))

(vtx/defvertex DFVertex
  (vec3 position)
  (vec2 texcoord0)
  (vec4 sdf_params)
  (vec4 face_color)
  (vec4 outline_color)
  (vec4 shadow_color))

(defn render-font [^GL2 gl render-args renderables rcount]
  (let [user-data (get (first renderables) :user-data)
        gpu-texture (:gpu-texture user-data)
        vertex-buffer (:vertex-buffer user-data)
        material-shader (:shader user-data)
        type (:type user-data)
        vcount (count vertex-buffer)]
    (when (> vcount 0)
      (let [vertex-binding (vtx/use-with ::vb vertex-buffer material-shader)]
        (gl/with-gl-bindings gl render-args [material-shader vertex-binding gpu-texture]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))))

(def ^:private vertex-order [0 1 2 1 3 2])

(defn- glyph->quad [w h su sv glyph]
  (let [u0 (* su (+ (:x glyph) (:left-bearing glyph)))
        v0 (* sv (- (:y glyph) (:ascent glyph)))
        u1 (+ u0 (* su (:width glyph)))
        v1 (+ v0 (* sv (+ (:ascent glyph) (:descent glyph))))
        x0 (- (+ (:x glyph) (:left-bearing glyph)) (* 0.5 w))
        x1 (+ x0 (:width glyph))
        y1 (- (+ (- h (:y glyph)) (:ascent glyph)) (* 0.5 h))
        y0 (- y1 (:ascent glyph) (:descent glyph))
        vs [[x0 y0 0 u0 v1]
            [x1 y0 0 u1 v1]
            [x0 y1 0 u0 v0]
            [x1 y1 0 u1 v0]]]
    (mapv #(get vs %) vertex-order)))

(defn- font-type [font output-format]
  (if (= output-format :type-bitmap)
    (if (= "fnt" (:ext (resource/resource-type font)))
      :bitmap
      :defold)
    :distance-field))

(defn- vertex-buffer [vs type font-map]
  (let [colors [1.0 1.0 1.0 1.0 0.0 0.0 0.0 1.0 0.0 0.0 0.0 1.0]
        df-colors (into [(:sdf-scale font-map) (:sdf-offset font-map) (:sdf-outline font-map) 1.0] colors)]
    (case type
           (:defold :bitmap) (let [vb (->DefoldVertex (count vs))]
                               (doseq [v vs]
                                 (conj! vb (concat v colors)))
                               (persistent! vb))
           :distance-field (let [vb (->DFVertex (count vs))]
                             (doseq [v vs]
                               (conj! vb (concat v df-colors)))
                             (persistent! vb)))))

(g/defnk produce-scene [_node-id aabb gpu-texture font font-map ^BufferedImage font-image material-shader type]
  (let [w (.getWidth font-image)
        h (.getHeight font-image)
        vs [[0 0 0 0 1]
            [w 0 0 1 1]
            [0 h 0 0 0]
            [w h 0 1 0]]
        vs (mapv #(get vs %) [0 1 2 1 3 2])
        glyph-fn (partial glyph->quad w h (/ 1 w) (/ 1 h))
        vs (concat (reduce into [] (map glyph-fn (:glyphs font-map))))]
    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-font
                  :batch-key gpu-texture
                  :select-batch-key _node-id
                  :user-data {:type type
                              :gpu-texture gpu-texture
                              :vertex-buffer (vertex-buffer vs type font-map)
                              :shader material-shader}
                  :passes [pass/transparent]}}))

(g/defnk produce-pb-msg [font material size antialias alpha outline-alpha outline-width
                         shadow-alpha shadow-blur shadow-x shadow-y extra-characters output-format]
  {:font (resource/resource->proj-path font)
   :material (resource/resource->proj-path material)
   :size size
   :antialias size
   :alpha alpha
   :outline-alpha outline-alpha
   :outline-width outline-width
   :shadow-alpha shadow-alpha
   :shadow-blur shadow-blur
   :shadow-x shadow-x
   :shadow-y shadow-y
   :extra-characters extra-characters
   :output-format output-format})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Font$FontDesc pb-msg)})

(g/defnk produce-font-map-image [_node-id font pb-msg]
  (let [project (project/get-project _node-id)
        workspace (project/workspace project)
        resolver (partial workspace/resolve-workspace-resource workspace)]
    (font-gen/generate pb-msg font resolver)))

(defn- build-font [self basis resource dep-resources user-data]
  (let [project (project/get-project self)
        workspace (project/workspace project)
        font-map (assoc (:font-map user-data) :textures [(workspace/proj-path (second (first dep-resources)))])]
    {:resource resource :content (protobuf/map->bytes Font$FontMap font-map)}))

(g/defnk produce-build-targets [_node-id resource font-resource font-map font-image dep-build-targets]
  (let [project        (project/get-project _node-id)
        workspace      (project/workspace project)
        texture-target (image/make-texture-build-target workspace _node-id font-image)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-font
      :user-data {:font-map font-map}
      :deps (cons texture-target (flatten dep-build-targets))}]))

(g/defnode FontSourceNode
  (inherits project/ResourceNode))

(g/defnk produce-font-type [font output-format]
  (font-type font output-format))

(g/defnode FontNode
  (inherits project/ResourceNode)

  (property font (g/protocol resource/Resource)
    (value (g/fnk [font-resource] font-resource))
    (set (project/gen-resource-setter [[:resource :font-resource]])))
  (property material (g/protocol resource/Resource)
    (value (g/fnk [material-resource] material-resource))
    (set (project/gen-resource-setter [[:resource :material-resource]
                                       [:build-targets :dep-build-targets]
                                       [:sampler-data :material-sampler]
                                       [:shader :material-shader]])))
  (property size g/Int (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                      (or (= type :defold) (= type :distance-field))))))
  (property antialias g/Int (dynamic visible (g/always false)))
  (property alpha g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                       (= type :defold)))))
  (property outline-alpha g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                               (= type :defold)))))
  (property outline-width g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                               (or (= type :defold) (= type :distance-field))))))
  (property shadow-alpha g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                              (= type :defold)))))
  (property shadow-blur g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                             (= type :defold)))))
  (property shadow-x g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                          (= type :defold)))))
  (property shadow-y g/Num (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                          (= type :defold)))))
  (property extra-characters g/Str (dynamic visible (g/fnk [font output-format] (let [type (font-type font output-format)]
                                                                                  (or (= type :defold) (= type :distance-field))))))
  (property output-format g/Keyword
    (dynamic edit-type (g/always (properties/->pb-choicebox Font$FontTextureFormat))))

  (input dep-build-targets g/Any :array)
  (input font-resource (g/protocol resource/Resource))
  (input material-resource (g/protocol resource/Resource))
  (input material-sampler {g/Keyword g/Any})
  (input material-shader ShaderLifecycle)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Font" :icon font-icon}))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output font-map-image g/Any :cached produce-font-map-image)
  (output font-map g/Any (g/fnk [font-map-image] (:font-map font-map-image)))
  (output font-image g/Any (g/fnk [font-map-image] (:image font-map-image)))
  (output scene g/Any :cached produce-scene)
  (output aabb AABB (g/fnk [^BufferedImage font-image]
                      (if font-image
                        (let [hw (* 0.5 (.getWidth font-image))
                              hh (* 0.5 (.getHeight font-image))]
                          (-> (geom/null-aabb)
                            (geom/aabb-incorporate (Point3d. (- hw) (- hh) 0))
                            (geom/aabb-incorporate (Point3d. hw hh 0))))
                        (geom/null-aabb))))
  (output gpu-texture g/Any :cached (g/fnk [_node-id font-image material-sampler]
                                      (first (material/make-textures [{:image-id _node-id :image font-image}] material-sampler))))
  (output type g/Keyword produce-font-type))

(defn load-font [project self resource]
  (let [font (protobuf/read-text Font$FontDesc resource)
        props (keys font)]
    (for [prop props
          :let [value (cond->> (get font prop)
                        (resource-fields prop) (workspace/resolve-resource resource))]]
      (g/set-property self prop value))))

(defn register-resource-types [workspace]
  (concat
    (workspace/register-resource-type workspace
                                     :ext "font"
                                     :label "Font"
                                     :node-type FontNode
                                     :load-fn load-font
                                     :icon font-icon
                                     :view-types [:scene])
    (workspace/register-resource-type workspace
                                      :ext ["ttf" "otf" "fnt"]
                                      :label "Font"
                                      :node-type FontSourceNode
                                      :icon font-icon
                                      :view-types [:default])))
