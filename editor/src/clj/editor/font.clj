(ns editor.font
  (:require [clojure.string :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]            
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.scene-cache :as scene-cache]
            [editor.workspace :as workspace]
            [editor.pipeline.font-gen :as font-gen]
            [editor.image :as image]
            [editor.resource :as resource]
            [editor.properties :as properties]
            [editor.material :as material]
            [editor.validation :as validation]
            [editor.gl.pass :as pass])
  (:import [com.dynamo.render.proto Font$FontDesc Font$FontMap Font$FontTextureFormat]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [editor.gl.shader ShaderLifecycle]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [java.nio ByteBuffer]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Vector3d]
           [com.jogamp.opengl.util.texture Texture TextureData]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)

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

(def ^:private vertex-order [0 1 2 1 3 2])

(defn- glyph->quad [font-map su sv ^Matrix4d transform glyph]
  (let [padding (:glyph-padding font-map)
        u0 (* su (+ (:x glyph) padding))
        v0 (* sv (+ (:y glyph) padding))
        u1 (+ u0 (* su (:width glyph)))
        v1 (+ v0 (* sv (+ (:ascent glyph) (:descent glyph))))
        x0 (:left-bearing glyph)
        x1 (+ x0 (:width glyph))
        y1 (:ascent glyph)
        y0 (- y1 (:ascent glyph) (:descent glyph))
        p (Point3d.)
        vs (vec (for [[y v] [[y0 v1] [y1 v0]]
                      [x u] [[x0 u0] [x1 u1]]]
                  (do
                    (.set p x y 0.0)
                    (.transform transform p)
                    [(.x p) (.y p) (.z p) u v])))]
    (mapv #(get vs %) vertex-order)))

(defn- font-type [font output-format]
  (if (= output-format :type-bitmap)
    (if (= "fnt" (:ext (resource/resource-type font)))
      :bitmap
      :defold)
    :distance-field))

(defn- vertex-buffer [vs type font-map]
  (let [vcount (count vs)]
    (when (> vcount 0)
      (let [vb (case type
                 (:defold :bitmap) (->DefoldVertex vcount)
                 :distance-field (->DFVertex vcount))]
        (reduce conj! vb vs)
        (persistent! vb)))))

(defn- measure-line [glyphs line]
  (let [w (reduce + 0 (map (fn [c] (get-in glyphs [(int c) :advance] 0)) line))]
    (if-let [last (get glyphs (and (last line) (int (last line))))]
      (- (+ w (:left-bearing last) (:width last)) (:advance last))
      w)))

(defn- split [s i]
  (mapv s/trim [(subs s 0 i) (subs s i)]))

(defn- break-line [^String line glyphs max-width]
  (loop [b (dec (count line))
         last-space nil]
    (if (> b 0)
      (let [c (.charAt line b)]
        (if (Character/isWhitespace c)
          (let [[l1 l2] (split line b)
                w (measure-line glyphs l1)]
            (if (> w max-width)
              (recur (dec b) b)
              [l1 l2]))
          (recur (dec b) last-space)))
      (if last-space
        (split line last-space)
        [line nil]))))

(defn- break-lines [lines glyphs max-width]
  (reduce (fn [result line]
            (let [w (measure-line glyphs line)]
              (if (> w max-width)
                (let [[l1 l2] (break-line line glyphs max-width)]
                  (if l2
                    (recur (conj result l1) l2)
                    (conj result line)))
                (conj result line)))) [] lines))

(defn- split-text [glyphs text line-break? max-width]
  (cond-> (map s/trim (s/split-lines text))
    line-break? (break-lines glyphs max-width)))

(defn- font-map->glyphs [font-map]
  (into {} (map (fn [g] [(:character g) g]) (:glyphs font-map))))

(defn measure
  ([font-map text]
    (measure font-map text false 0))
  ([font-map text line-break? max-width]
    (if (or (nil? font-map) (nil? text) (empty? text))
      [0 0]
      (let [glyphs (font-map->glyphs font-map)
            lines (split-text glyphs text line-break? max-width)
            line-height (+ (:max-descent font-map) (:max-ascent font-map))
            line-widths (map (partial measure-line glyphs) lines)
            max-width (reduce max 0 line-widths)]
        [max-width (* (count lines) line-height)]))))

(def FontData {:type g/Keyword
               :font-map g/Any
               :texture g/Any})

(defn- cell->coords [cell font-map]
  (let [cw (:cache-cell-width font-map)
        ch (:cache-cell-height font-map)
        w (int (/ (:cache-width font-map) cw))
        h (int (/ (:cache-height font-map) ch))]
    [(* (mod cell w) cw)
     (* (int (/ cell w)) ch)]))

(defn- place-glyph [^GL2 gl cache font-map texture glyph]
  (if-let [cell (scene-cache/request-object! ::glyph-cells [texture glyph] gl [cache font-map texture glyph])]
    (let [[x y] (cell->coords cell font-map)]
      (assoc glyph :x x :y y))
    (assoc glyph :width 0)))

(defn gen-vertex-buffer [^GL2 gl {:keys [type font-map texture]} text-entries]
  (let [cache (scene-cache/request-object! ::glyph-caches texture gl [font-map])
        w (:cache-width font-map)
        h (:cache-height font-map)
        glyph-fn (partial glyph->quad font-map (/ 1 w) (/ 1 h))
        glyphs (font-map->glyphs font-map)
        char->glyph (comp glyphs int)
        line-height (+ (:max-ascent font-map) (:max-descent font-map))
        sdf-params [(:sdf-scale font-map) (:sdf-offset font-map) (:sdf-outline font-map) 1.0]
        vs (loop [vs (transient [])
                  text-entries text-entries]
             (if-let [entry (first text-entries)]
               (let [colors (repeat (cond->> (into (:color entry) (concat (:outline entry) (:shadow entry)))
                                      (= type :distance-field) (into sdf-params)))
                     offset (:offset entry)
                     xform (doto (Matrix4d.)
                             (.set (let [[x y] offset]
                                     (Vector3d. x y 0.0))))
                     _ (.mul xform ^Matrix4d (:world-transform entry) xform)
                     lines (split-text glyphs (:text entry) (:line-break entry) (:max-width entry))
                     vs (loop [vs vs
                               lines lines
                               line-no 0]
                          (if-let [line (first lines)]
                            (let [y (* line-no (- line-height))
                                  vs (loop [vs vs
                                            glyphs (map char->glyph line)
                                            x 0.0]
                                       (if-let [glyph (first glyphs)]
                                         (let [glyph (place-glyph gl cache font-map texture glyph)
                                               cursor (doto (Matrix4d.) (.set (Vector3d. x y 0.0)))
                                               cursor (doto cursor (.mul xform cursor))
                                               vs (reduce conj! vs (map into (glyph-fn cursor glyph) colors))]
                                           (recur vs (rest glyphs) (+ x (:advance glyph))))
                                         vs))]
                              (recur vs (rest lines) (inc line-no)))
                            vs))]
                 (recur vs (rest text-entries)))
               (persistent! vs)))]
    (vertex-buffer vs type font-map)))

(defn render-font [^GL2 gl render-args renderables rcount]
  (let [user-data (get (first renderables) :user-data)
        gpu-texture (:texture user-data)
        font-map (:font-map user-data)
        vertex-buffer (gen-vertex-buffer gl user-data [{:text (:text user-data)
                                                        :offset [0.0 0.0]
                                                        :world-transform (doto (Matrix4d.) (.setIdentity))
                                                        :color [1.0 1.0 1.0 1.0] :outline [0.0 0.0 0.0 1.0] :shadow [0.0 0.0 0.0 0.0]
                                                        :line-break true :max-width (:cache-width font-map)}])
        material-shader (:shader user-data)
        type (:type user-data)
        vcount (count vertex-buffer)]
    (when (> vcount 0)
      (let [vertex-binding (vtx/use-with ::vb vertex-buffer material-shader)]
        (gl/with-gl-bindings gl render-args [material-shader vertex-binding gpu-texture]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))))

(g/defnk produce-scene [_node-id aabb gpu-texture font font-map material-shader type preview-text]
  {:node-id _node-id
   :aabb aabb
   :renderable {:render-fn render-font
                :batch-key gpu-texture
                :select-batch-key _node-id
                :user-data {:type type
                            :texture gpu-texture
                            :font-map font-map
                            :shader material-shader
                            :text preview-text}
                :passes [pass/transparent]}})

(g/defnk produce-pb-msg [font material size antialias alpha outline-alpha outline-width
                         shadow-alpha shadow-blur shadow-x shadow-y extra-characters output-format
                         all-chars cache-width cache-height]
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
   :output-format output-format
   :all-chars all-chars
   :cache-width cache-width
   :cache-height cache-height})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str Font$FontDesc pb-msg)})

(g/defnk produce-font-map [_node-id font pb-msg]
  (let [project (project/get-project _node-id)
        workspace (project/workspace project)
        resolver (partial workspace/resolve-workspace-resource workspace)]
    (font-gen/generate pb-msg font resolver)))

(defn- build-font [self basis resource dep-resources user-data]
  (let [project (project/get-project self)
        workspace (project/workspace project)
        font-map (assoc (:font-map user-data) :textures [(resource/proj-path (second (first dep-resources)))])]
    {:resource resource :content (protobuf/map->bytes Font$FontMap font-map)}))

(g/defnk produce-build-targets [_node-id resource font-resource font-map dep-build-targets]
  (let [project        (project/get-project _node-id)
        workspace      (project/workspace project)]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-font
      :user-data {:font-map font-map}
      :deps (flatten dep-build-targets)}]))

(g/defnode FontSourceNode
  (inherits project/ResourceNode))

(g/defnk produce-font-type [font output-format]
  (font-type font output-format))

(defn- char->string [c]
  (String. (Character/toChars c)))

(g/defnk produce-preview-text [font-map]
  (let [char->glyph (into {} (map (fn [g] [(:character g) g]) (:glyphs font-map)))
        chars (filter (fn [c] (and (contains? char->glyph c) (> (:width (get char->glyph c)) 0))) (range 255))
        cache-width (:cache-width font-map)
        lines (loop [lines []
                     chars chars]
                (if (not-empty chars)
                  (let [[line chars] (loop [line []
                                            chars chars
                                            w 0]
                                       (if (and (not-empty chars) (< w cache-width))
                                         (let [c (first chars)
                                               g (char->glyph c)]
                                           (recur (conj line (first chars)) (rest chars) (+ w (int (:advance g)))))
                                         [line chars]))]
                    (recur (conj lines line) chars))
                  lines))
        text (s/join " " (map (fn [l] (s/join (map char->string l))) lines))]
    text))

(g/defnode FontNode
  (inherits project/ResourceNode)

  (property font (g/protocol resource/Resource)
    (value (gu/passthrough font-resource))
    (set (project/gen-resource-setter [[:resource :font-resource]]))
    (validate (validation/validate-resource font)))
      
  (property material (g/protocol resource/Resource)
    (value (gu/passthrough material-resource))
    (set (project/gen-resource-setter [[:resource :material-resource]
                                       [:build-targets :dep-build-targets]
                                       [:samplers :material-samplers]
                                       [:shader :material-shader]]))
    (validate (validation/validate-resource material)))

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

  (property all-chars g/Bool)
  (property cache-width g/Int)
  (property cache-height g/Int)

  (input dep-build-targets g/Any :array)
  (input font-resource (g/protocol resource/Resource))
  (input material-resource (g/protocol resource/Resource))
  (input material-samplers [{g/Keyword g/Any}])
  (input material-shader ShaderLifecycle)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Font" :icon font-icon}))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets)
  (output font-map g/Any :cached produce-font-map)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB (g/fnk [font-map preview-text]
                           (if font-map
                             (let [[w h] (measure font-map preview-text true (:cache-width font-map))
                                   h-offset (:max-ascent font-map)]
                               (-> (geom/null-aabb)
                                 (geom/aabb-incorporate (Point3d. 0 h-offset 0))
                                 (geom/aabb-incorporate (Point3d. w (- h-offset h) 0))))
                             (geom/null-aabb))))
  (output gpu-texture g/Any :cached (g/fnk [_node-id font-map material-samplers]
                                           (let [w (:cache-width font-map)
                                                 h (:cache-height font-map)
                                                 channels (:glyph-channels font-map)]
                                             (texture/empty-texture _node-id w h channels
                                                                   (material/sampler->tex-params (first material-samplers)) 0))))
  (output material-shader ShaderLifecycle (g/fnk [material-shader] material-shader))
  (output type g/Keyword produce-font-type)
  (output font-data FontData :cached (g/fnk [type gpu-texture font-map]
                                            {:type type
                                             :texture gpu-texture
                                             :font-map font-map}))
  (output preview-text g/Str :cached produce-preview-text))

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

(defn- make-glyph-cache [^GL2 gl params]
  (let [[font-map] params
        {:keys [cache-width cache-height cache-cell-width cache-cell-height]} font-map
        size (* (int (/ cache-width cache-cell-width)) (int (/ cache-height cache-cell-height)))]
    {:width cache-width
     :height cache-height
     :cell-width cache-cell-width
     :cell-height cache-cell-height
     :cache (atom {:free (range size)})}))

(defn- update-glyph-cache [^GL2 gl glyph-cache params]
  (make-glyph-cache gl params))

(defn- destroy-glyph-caches [^GL2 gl font-caches _])

(scene-cache/register-object-cache! ::glyph-caches make-glyph-cache update-glyph-cache destroy-glyph-caches)

(defn- make-glyph-cell [^GL2 gl params]
  (let [[cache font-map texture glyph] params
        cache (:cache cache)
        cw (:cache-cell-width font-map)
        ch (:cache-cell-height font-map)
        w (int (/ (:cache-width font-map) cw))
        h (int (/ (:cache-height font-map) ch))]
    (let [cell (first (:free @cache))]
      (when cell
        (let [x (* (mod cell w) cw)
              y (* (int (/ cell w)) ch)
              p (* 2 (:glyph-padding font-map))
              w (+ (:width glyph) p)
              h (+ (:ascent glyph) (:descent glyph) p)
              ^ByteBuffer src-data (-> ^ByteBuffer (.asReadOnlyByteBuffer ^ByteString (:glyph-data font-map))
                                     ^ByteBuffer (.position (:glyph-data-offset glyph))
                                     (.slice)
                                     (.limit (:glyph-data-size glyph)))
              tgt-data (doto (ByteBuffer/allocateDirect (:glyph-data-size glyph))
                         (.put src-data)
                         (.flip))]
          (when (> (:glyph-data-size glyph) 0)
            (texture/tex-sub-image gl texture tgt-data x y w h (:glyph-channels font-map)))))
      (swap! cache update :free rest)
      cell)))

(defn- update-glyph-cell [^GL2 gl cell params]
  cell)

(defn- destroy-glyph-cells [^GL2 gl glyphs params]
  (doseq [[glyph params] (map vector glyphs params)
          :let [[cache _ _ _] params]]
    (swap! (:cache cache) update :free conj glyph)))

(scene-cache/register-object-cache! ::glyph-cells make-glyph-cell update-glyph-cell destroy-glyph-cells)
