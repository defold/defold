(ns editor.font
  (:require [clojure.string :as s]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.defold-project :as project]
            [editor.scene-cache :as scene-cache]
            [editor.workspace :as workspace]
            [editor.pipeline.font-gen :as font-gen]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.properties :as properties]
            [editor.material :as material]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [schema.core :as schema])
  (:import [com.dynamo.render.proto Font$FontDesc Font$FontMap Font$FontTextureFormat]
           [editor.types AABB]
           [editor.gl.shader ShaderLifecycle]
           [java.nio ByteBuffer]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix4d Point3d Vector3d]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)

(def ^:private font-file-extensions ["ttf" "otf" "fnt"])
(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private resource-fields #{:font :material})

(def ^:private alpha-slider-edit-type {:type :slider
                                       :min 0.0
                                       :max 1.0
                                       :precision 0.01})

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
    (if (and font (= "fnt" (:ext (resource/resource-type font))))
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

(defn- measure-line [glyphs text-tracking line]
  (let [w (reduce + (- text-tracking) (map (fn [c] (+ text-tracking (get-in glyphs [(int c) :advance] 0))) line))]
    (if-let [last (get glyphs (and (last line) (int (last line))))]
      (- (+ w (:left-bearing last) (:width last)) (:advance last))
      w)))

(defn- split [s i]
  (mapv s/trim [(subs s 0 i) (subs s i)]))

(defn- break-line [^String line glyphs max-width text-tracking]
  (loop [b (dec (count line))
         last-space nil]
    (if (> b 0)
      (let [c (.charAt line b)]
        (if (Character/isWhitespace c)
          (let [[l1 l2] (split line b)
                w (measure-line glyphs text-tracking l1)]
            (if (> w max-width)
              (recur (dec b) b)
              [l1 l2]))
          (recur (dec b) last-space)))
      (if last-space
        (split line last-space)
        [line nil]))))

(defn- break-lines [lines glyphs max-width text-tracking]
  (reduce (fn [result line]
            (let [w (measure-line glyphs text-tracking line)]
              (if (> w max-width)
                (let [[l1 l2] (break-line line glyphs max-width text-tracking)]
                  (if l2
                    (recur (conj result l1) l2)
                    (conj result line)))
                (conj result line)))) [] lines))

(defn- split-text [glyphs text line-break? max-width text-tracking]
  (cond-> (map s/trim (s/split-lines text))
    line-break? (break-lines glyphs max-width text-tracking)))

(defn- font-map->glyphs [font-map]
  (into {} (map (fn [g] [(:character g) g]) (:glyphs font-map))))

(defn measure
  ([font-map text]
    (measure font-map text false 0 0 1))
  ([font-map text line-break? max-width text-tracking text-leading]
    (if (or (nil? font-map) (nil? text) (empty? text))
      [0 0]
      (let [glyphs (font-map->glyphs font-map)
            line-height (+ (:max-descent font-map) (:max-ascent font-map))
            text-tracking (* line-height text-tracking)
            lines (split-text glyphs text line-break? max-width text-tracking)
            line-widths (map (partial measure-line glyphs text-tracking) lines)
            max-width (reduce max 0 line-widths)]
        [max-width (* line-height (+ 1 (* text-leading (dec (count lines)))))]))))

(g/deftype FontData {:type     schema/Keyword
                     :font-map schema/Any
                     :texture  schema/Any})

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

(defn layout-text [font-map text line-break? max-width text-tracking text-leading]
  (let [text-layout {:width max-width
                     :height 0
                     :lines []
                     :line-widths []
                     :text-tracking text-tracking
                     :text-leading text-leading}]
    (if (or (nil? font-map) (nil? text) (nil? text))
      text-layout
      (let [glyphs (font-map->glyphs font-map)
            line-height (+ (:max-descent font-map) (:max-ascent font-map))
            text-tracking (* line-height text-tracking)
            lines (split-text glyphs text line-break? max-width text-tracking)
            line-widths (mapv (partial measure-line glyphs text-tracking) lines)
            max-width (reduce max 0 line-widths)]
        (assoc text-layout
               :width max-width
               :height (* line-height (+ 1 (* text-leading (dec (count lines)))))
               :lines lines
               :line-widths line-widths)))))

(defn gen-vertex-buffer [^GL2 gl {:keys [type font-map texture]} text-entries]
  (let [cache (scene-cache/request-object! ::glyph-caches texture gl [font-map])
        w (:cache-width font-map)
        h (:cache-height font-map)
        glyph-fn (partial glyph->quad font-map (/ 1 w) (/ 1 h))
        glyphs (font-map->glyphs font-map)
        char->glyph (comp glyphs int)
        line-height (+ (:max-ascent font-map) (:max-descent font-map))
        smoothing (/ 0.25 (:sdf-spread font-map))
        sdf-edge-value 0.75
        sdf-params [sdf-edge-value (:sdf-outline font-map) smoothing (:sdf-spread font-map)]
        vs (loop [vs (transient [])
                  text-entries text-entries]
             (if-let [entry (first text-entries)]
               (let [colors (repeat (cond->> (into (:color entry) (concat (:outline entry) (:shadow entry)))
                                      (= type :distance-field) (into sdf-params)))
                     text-layout (:text-layout entry)
                     text-tracking (* line-height (:text-tracking text-layout 0))
                     text-leading (:text-leading text-layout 1)
                     offset (:offset entry)
                     xform (doto (Matrix4d.)
                             (.set (let [[x y] offset]
                                     (Vector3d. x y 0.0))))
                     _ (.mul xform ^Matrix4d (:world-transform entry) xform)
                     lines (:lines text-layout)
                     line-widths (:line-widths text-layout)
                     max-width (:width text-layout)
                     align (:align entry :center)
                     vs (loop [vs vs
                               lines lines
                               line-widths line-widths
                               line-no 0]
                          (if-let [line (first lines)]
                            (let [line-width (first line-widths)
                                  y (* line-no (- (* line-height text-leading)))
                                  vs (loop [vs vs
                                            glyphs (map char->glyph line)
                                            x (case align
                                                :left 0.0
                                                :center (* 0.5 (- max-width line-width))
                                                :right (- max-width line-width))]
                                       (if-let [glyph (first glyphs)]
                                         (let [glyph (place-glyph gl cache font-map texture glyph)
                                               cursor (doto (Matrix4d.) (.set (Vector3d. x y 0.0)))
                                               cursor (doto cursor (.mul xform cursor))
                                               vs (reduce conj! vs (map into (glyph-fn cursor glyph) colors))]
                                           (recur vs (rest glyphs) (+ x (:advance glyph) text-tracking)))
                                         vs))]
                              (recur vs (rest lines) (rest line-widths) (inc line-no)))
                            vs))]
                 (recur vs (rest text-entries)))
               (persistent! vs)))]
    (vertex-buffer vs type font-map)))

(defn render-font [^GL2 gl render-args renderables rcount]
  (let [user-data (get (first renderables) :user-data)
        gpu-texture (:texture user-data)
        font-map (:font-map user-data)
        max-width (:cache-width font-map)
        text-layout (layout-text font-map (:text user-data) true max-width 0 1)
        vertex-buffer (gen-vertex-buffer gl user-data [{:text-layout text-layout
                                                        :align :left
                                                        :offset [0.0 0.0]
                                                        :world-transform (doto (Matrix4d.) (.setIdentity))
                                                        :color [1.0 1.0 1.0 1.0] :outline [0.0 0.0 0.0 1.0] :shadow [0.0 0.0 0.0 0.0]}])
        material-shader (:shader user-data)
        type (:type user-data)
        vcount (count vertex-buffer)]
    (when (> vcount 0)
      (let [vertex-binding (vtx/use-with ::vb vertex-buffer material-shader)]
        (gl/with-gl-bindings gl render-args [material-shader vertex-binding gpu-texture]
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))))

(g/defnk produce-scene [_node-id aabb gpu-texture font-map material material-shader type preview-text]
  (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                              (validation/prop-error :fatal _node-id :material validation/prop-resource-not-exists? material "Material")]
                             (remove nil?)
                             (not-empty))]
        (g/error-aggregate errors))
      (cond-> {:node-id _node-id
               :aabb aabb}

              (and (some? font-map) (not-empty preview-text))
              (assoc :renderable {:render-fn render-font
                                  :batch-key gpu-texture
                                  :select-batch-key _node-id
                                  :user-data {:type type
                                              :texture gpu-texture
                                              :font-map font-map
                                              :shader material-shader
                                              :text preview-text}
                                  :passes [pass/transparent]}))))

(g/defnk produce-pb-msg [pb font material size antialias alpha outline-alpha outline-width
                         shadow-alpha shadow-blur shadow-x shadow-y extra-characters output-format
                         all-chars cache-width cache-height]
  ;; The reason we use the originally loaded pb is to retain any default values
  ;; This is important for the saved file to not contain more data than it would
  ;; have when saved with Editor1
  (merge pb
         {:font (resource/resource->proj-path font)
          :material (resource/resource->proj-path material)
          :size size
          :antialias antialias
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
          :cache-height cache-height}))

(g/defnk produce-font-map [_node-id font type pb-msg]
  (or (when-let [errors (->> (concat [(validation/prop-error :fatal _node-id :font validation/prop-nil? font "Font")
                                      (validation/prop-error :fatal _node-id :font validation/prop-resource-not-exists? font "Font")
                                      (validation/prop-error :fatal _node-id :cache-width validation/prop-negative? (:cache-width pb-msg) "Cache Width")
                                      (validation/prop-error :fatal _node-id :cache-height validation/prop-negative? (:cache-height pb-msg) "Cache Height")]

                                     (when (or (= type :defold) (= type :distance-field))
                                       [(validation/prop-error :fatal _node-id :size validation/prop-zero-or-below? (:size pb-msg) "Size")
                                        (validation/prop-error :fatal _node-id :outline-width validation/prop-negative? (:outline-width pb-msg) "Outline Width")
                                        (validation/prop-error :fatal _node-id :alpha validation/prop-negative? (:alpha pb-msg) "Alpha")
                                        (validation/prop-error :fatal _node-id :outline-alpha validation/prop-negative? (:outline-alpha pb-msg) "Outline Alpha")])

                                     (when (= type :defold)
                                       [(validation/prop-error :fatal _node-id :shadow-alpha validation/prop-negative? (:shadow-alpha pb-msg) "Shadow Alpha")
                                        (validation/prop-error :fatal _node-id :shadow-blur validation/prop-negative? (:shadow-blur pb-msg) "Shadow Blur")]))
                             (remove nil?)
                             (not-empty))]
        (g/error-aggregate errors))
      (let [resolver (partial workspace/resolve-resource font)]
        (try
          (font-gen/generate pb-msg font resolver)
          (catch Exception error
            (g/->error _node-id :font :fatal font (str "Failed to generate bitmap from Font. " (.getMessage error))))))))

(defn- build-font [resource dep-resources user-data]
  (let [font-map (assoc (:font-map user-data) :textures [(resource/proj-path (second (first dep-resources)))])]
    {:resource resource :content (protobuf/map->bytes Font$FontMap font-map)}))

(g/defnk produce-build-targets [_node-id resource font-map material dep-build-targets]
  (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                              (validation/prop-error :fatal _node-id :material validation/prop-resource-not-exists? material "Material")]
                             (remove nil?)
                             (not-empty))]
        (g/error-aggregate errors))
      [{:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-font
        :user-data {:font-map font-map}
        :deps (flatten dep-build-targets)}]))

(g/defnode FontSourceNode
  (inherits resource-node/ResourceNode))

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

(defn- bool->int [val]
  (when (some? val) (if val 1 0)))

(defn- int->bool [val]
  (when (some? val) (if (= val 0) false true)))

(defn- glyph-channels->data-format [^long channels]
  (case channels
    1 :gray
    3 :rgb
    4 :rgba))

(g/defnk output-format-defold? [font output-format]
  (let [type (font-type font output-format)]
    (= type :defold)))

(g/defnk output-format-defold-or-distance-field? [font output-format]
  (let [type (font-type font output-format)]
    (or (= type :defold) (= type :distance-field))))

(g/defnode FontNode
  (inherits resource-node/ResourceNode)

  (property font resource/Resource
    (value (gu/passthrough font-resource))
    (set (fn [_evaluation-context self old-value new-value]
           (project/resource-setter self old-value new-value
                                    [:resource :font-resource])))
    (dynamic error (g/fnk [_node-id font-resource]
                          (or (validation/prop-error :fatal _node-id :font validation/prop-nil? font-resource "Font")
                              (validation/prop-error :fatal _node-id :font validation/prop-resource-not-exists? font-resource "Font"))))
    (dynamic edit-type (g/constantly
                         {:type resource/Resource
                          :ext font-file-extensions})))

  (property material resource/Resource
    (value (gu/passthrough material-resource))
    (set (fn [_evaluation-context self old-value new-value]
           (project/resource-setter self old-value new-value
                                    [:resource :material-resource]
                                    [:build-targets :dep-build-targets]
                                    [:samplers :material-samplers]
                                    [:shader :material-shader])))
    (dynamic error (g/fnk [_node-id material-resource]
                          (or (validation/prop-error :fatal _node-id :font validation/prop-nil? material-resource "Material")
                              (validation/prop-error :fatal _node-id :font validation/prop-resource-not-exists? material-resource "Material"))))
    (dynamic edit-type (g/constantly
                         {:type resource/Resource
                          :ext ["material"]})))

  (property output-format g/Keyword
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Font$FontTextureFormat))))
  (property size g/Int
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? size)))

  (property antialias g/Int (dynamic visible (g/constantly false)))
  (property antialiased g/Bool
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic label (g/constantly "Antialias"))
            (value (g/fnk [antialias] (int->bool antialias)))
            (set (fn [_evaluation-context self old-value new-value]
                   (g/set-property self :antialias (bool->int new-value)))))
  (property alpha g/Num
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type)))
  (property outline-alpha g/Num
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? outline-alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type)))
  (property outline-width g/Num
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? outline-width)))
  (property shadow-alpha g/Num
            (dynamic visible output-format-defold?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? shadow-alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type)))
  (property shadow-blur g/Num
            (dynamic visible output-format-defold?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? shadow-blur)))
  (property shadow-x g/Num
            (dynamic visible output-format-defold?))
  (property shadow-y g/Num
            (dynamic visible output-format-defold?))
  (property extra-characters g/Str
            (dynamic visible output-format-defold-or-distance-field?))
  (property all-chars g/Bool
            (dynamic visible output-format-defold-or-distance-field?))
  (property cache-width g/Int
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? cache-width)))
  (property cache-height g/Int
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? cache-height)))

  (property pb g/Any (dynamic visible (g/constantly false)))

  (input dep-build-targets g/Any :array)
  (input font-resource resource/Resource)
  (input material-resource resource/Resource)
  (input material-samplers [g/KeywordMap])
  (input material-shader ShaderLifecycle)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Font" :icon font-icon}))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output font-map g/Any :cached produce-font-map)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB (g/fnk [font-map preview-text]
                           (if font-map
                             (let [[w h] (measure font-map preview-text true (:cache-width font-map) 0 1)
                                   h-offset (:max-ascent font-map)]
                               (-> (geom/null-aabb)
                                 (geom/aabb-incorporate (Point3d. 0 h-offset 0))
                                 (geom/aabb-incorporate (Point3d. w (- h-offset h) 0))))
                             (geom/null-aabb))))
  (output gpu-texture g/Any :cached (g/fnk [_node-id font-map material-samplers]
                                           (when font-map
                                             (let [w (:cache-width font-map)
                                                   h (:cache-height font-map)
                                                   channels (:glyph-channels font-map)
                                                   data-format (glyph-channels->data-format channels)]
                                               (texture/empty-texture _node-id w h data-format
                                                                      (material/sampler->tex-params (first material-samplers)) 0)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output type g/Keyword produce-font-type)
  (output font-data FontData :cached (g/fnk [type gpu-texture font-map]
                                            {:type type
                                             :texture gpu-texture
                                             :font-map font-map}))
  (output preview-text g/Str :cached produce-preview-text))

(defn load-font [project self resource font]
  (let [props (keys font)]
    (concat
      (g/set-property self :pb font)
      (for [prop props
            :let [value (cond->> (get font prop)
                          (resource-fields prop) (workspace/resolve-resource resource))]]
        (g/set-property self prop value)))))

(defn register-resource-types [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :textual? true
      :ext "font"
      :label "Font"
      :node-type FontNode
      :ddf-type Font$FontDesc
      :load-fn load-font
      :icon font-icon
      :view-types [:scene :text])
    (workspace/register-resource-type workspace
      :ext font-file-extensions
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
            (texture/tex-sub-image gl texture tgt-data x y w h (glyph-channels->data-format (:glyph-channels font-map))))))
      (swap! cache update :free rest)
      cell)))

(defn- update-glyph-cell [^GL2 gl cell params]
  cell)

(defn- destroy-glyph-cells [^GL2 gl glyphs params]
  (doseq [[glyph params] (map vector glyphs params)
          :let [[cache _ _ _] params]]
    (swap! (:cache cache) update :free conj glyph)))

(scene-cache/register-object-cache! ::glyph-cells make-glyph-cell update-glyph-cell destroy-glyph-cells)
