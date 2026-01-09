;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.font
  (:require [clojure.string :as s]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.pipeline :as pipeline]
            [editor.pipeline.font-gen :as font-gen]
            [editor.pipeline.fontc :as fontc]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.scene-cache :as scene-cache]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [schema.core :as schema]
            [service.log :as log])
  (:import [com.dynamo.bob.font BMFont]
           [com.dynamo.render.proto Font$FontDesc Font$FontMap Font$FontRenderMode Font$FontTextureFormat Font$GlyphBank]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.vertex2 VertexBuffer]
           [editor.types AABB]
           [java.io InputStream]
           [java.nio ByteBuffer]
           [java.nio.file Paths]
           [javax.vecmath Matrix4d Point3d Vector3d Vector4d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private layer-mask-face 0x1)
(def ^:private layer-mask-outline 0x2)
(def ^:private layer-mask-shadow 0x4)

(def ^:private font-file-extensions ["ttf" "otf" "fnt"])
(def ^:private font-icon "icons/32/Icons_28-AT-Font.png")
(def ^:private font-label (localization/message "resource.type.font"))

(def ^:private alpha-slider-edit-type {:type :slider
                                       :min 0.0
                                       :max 1.0
                                       :precision 0.01})

(def ^:private shadows-outline-slider-edit-type {:type :slider
                                                 :min 0.0
                                                 :max 64.0
                                                 :precision 1.0})

(vtx/defvertex ^:no-put DefoldVertex
  (vec3 position)
  (vec2 texcoord0)
  (vec4 face_color)
  (vec4 outline_color)
  (vec4 shadow_color)
  (vec3 layer_mask))

(vtx/defvertex ^:no-put DFVertex
  (vec3 position)
  (vec2 texcoord0)
  (vec4 sdf_params)
  (vec4 face_color)
  (vec4 outline_color)
  (vec4 shadow_color)
  (vec3 layer_mask))

(def ^:private vertex-order [0 1 2 1 3 2])

(defn- put-pos-uv!
  [^ByteBuffer bb x y z u v]
  (.putFloat bb x)
  (.putFloat bb y)
  (.putFloat bb z)
  (.putFloat bb u)
  (.putFloat bb v))

(defn- wrap-with-sdf-params
  [put-pos-uv-fn font-map]
  (let [{:keys [sdf-spread sdf-outline sdf-shadow]} font-map
        sdf-smoothing (/ 0.25 sdf-spread)
        sdf-edge 0.75]
    (fn [^ByteBuffer bb x y z u v]
      (put-pos-uv-fn bb x y z u v)
      (.putFloat bb sdf-edge) (.putFloat bb sdf-outline) (.putFloat bb sdf-smoothing) (.putFloat bb sdf-shadow))))

(defn- wrap-with-feature-data
  [put-pos-uv-fn color outline shadow unpacked-layer-mask]
  (let [[color-r color-g color-b color-a] color
        [outline-r outline-g outline-b outline-a] outline
        [shadow-r shadow-g shadow-b shadow-a] shadow
        [unpacked-layer-mask-r unpacked-layer-mask-g unpacked-layer-mask-b] unpacked-layer-mask]
    (fn [^ByteBuffer bb x y z u v]
      (put-pos-uv-fn bb x y z u v)
      (.putFloat bb color-r) (.putFloat bb color-g) (.putFloat bb color-b) (.putFloat bb color-a)
      (.putFloat bb outline-r) (.putFloat bb outline-g) (.putFloat bb outline-b) (.putFloat bb outline-a)
      (.putFloat bb shadow-r) (.putFloat bb shadow-g) (.putFloat bb shadow-b) (.putFloat bb shadow-a)
      (.putFloat bb unpacked-layer-mask-r) (.putFloat bb unpacked-layer-mask-g) (.putFloat bb unpacked-layer-mask-b))))

(defn- make-put-glyph-quad-fn
  [font-map]
  (let [^int w (:cache-width font-map)
        ^int h (:cache-height font-map)
        ^long padding (:glyph-padding font-map)
        su (/ 1.0 w)
        sv (/ 1.0 h)]
    (fn [^VertexBuffer vbuf glyph ^Matrix4d transform put-pos-uv-fn]
      (let [u0 (* su (+ ^long (:x glyph) padding))
            v0 (* sv (+ ^long (:y glyph) padding))
            u1 (+ u0 (* su ^long (:width glyph)))
            v1 (+ v0 (* sv (+ ^long (:ascent glyph) ^long (:descent glyph))))
            ^double x0 (:left-bearing glyph)
            x1 (+ ^double x0 ^long (:width glyph))
            ^double y1 (:ascent glyph)
            y0 (- y1 ^long (:ascent glyph) ^long (:descent glyph))]
        (let [^ByteBuffer bb (.buf vbuf)
              p (Point3d.)
              vs (vec (for [[y v] [[y0 v1] [y1 v0]]
                            [x u] [[x0 u0] [x1 u1]]]
                        (do
                          (.set p x y 0.0)
                          (.transform transform p)
                          [(.x p) (.y p) (.z p) u v])))]
          (run! (fn [idx]
                  (let [[x y z u v] (nth vs idx)]
                    (put-pos-uv-fn bb x y z u v))) vertex-order)
          vbuf)))))

(defn- font-type [font output-format]
  (if (= output-format :type-bitmap)
    (if (and font (= "fnt" (:ext (resource/resource-type font))))
      :bitmap
      :defold)
    :distance-field))

(defn- measure-line [is-monospaced padding glyphs text-tracking ^String line]
  (let [w (transduce (comp
                       (map #(:advance (glyphs (int %)) 0.0))
                       (interpose text-tracking))
                     +
                     0.0
                     line)
        len (.length line)]
    (if is-monospaced
      (+ w padding)
      (if-let [last (get glyphs (and (pos? len) (int (.charAt line (dec len)))))]
        (- (+ w (:left-bearing last) (:width last)) (:advance last))
        w))))

(defn- split-text [glyphs ^String text line-break? max-width text-tracking]
  (if line-break?
    ;; Rules for line breaks:
    ;; 1. if a single line without spaces exceeds max width, let it be so
    ;; 2. always split lines on \r?\n
    ;; 3. remove trailing empty lines
    ;; 4. trim start and end of a single line
    ;; 5. if a line is too long, try splitting it on whitespace (including zero-width
    ;;    space)
    (let [text-length (.length text)
          last-index (dec text-length)
          zero-width-space (char 0x200B)
          ;; remove trailing whitespace from sb and add to lines
          add-line! (fn [lines ^StringBuilder sb]
                      (let [line-length (.length sb)]
                        (if (zero? line-length)
                          (conj! lines "")
                          (let [last-non-ws-index
                                ;; Note: normally, this loop would be a subject to IOOB
                                ;; exception, but here we have an invariant that
                                ;; the accumulated string in sb is either empty or does
                                ;; not start with a white space char
                                (loop [i (dec line-length)]
                                  (let [ch (.charAt sb i)]
                                    (if (or (Character/isWhitespace ch)
                                            (= zero-width-space ch))
                                      (recur (dec i))
                                      i)))
                                s (.substring sb 0 (inc last-non-ws-index))]
                            (.setLength sb 0)
                            (conj! lines s)))))
          ;; remove trailing empty lines and make persistent
          clean-up-lines! (fn [lines]
                            (persistent!
                              (loop [lines lines]
                                (let [lines-length (count lines)]
                                  (if (and (pos? lines-length)
                                           (= "" (lines (dec lines-length))))
                                    (recur (pop! lines))
                                    lines)))))
          sb (StringBuilder.)]
      (loop [i 0
             lines (transient [])
             known-white-space-index -1
             line-width 0.0]
        ;; if end of string, add the line and clean up the lines
        (if (= i text-length)
          (clean-up-lines! (add-line! lines sb))
          (let [ch (.charAt text i)]
            ;; if newline, do new line
            (if (or (= \return ch) (= \newline ch))
              (let [rn (and (= \return ch) (< i last-index) (= \newline (.charAt text (inc i))))]
                (recur (cond-> (inc i) rn inc)
                       (add-line! lines sb)
                       -1
                       0.0))
              ;; If white space and current line is empty, continue to next char
              (let [white-space (or (Character/isWhitespace ch) (= zero-width-space ch))]
                (if (and white-space (zero? (.length sb)))
                  (recur (inc i) lines -1 0.0)
                  ;; Append a char to the line
                  (let [glyph (glyphs (int ch))
                        line-width-with-tracking (cond-> line-width (pos? (.length sb)) (+ text-tracking))
                        end-line-width (-> line-width-with-tracking
                                           (+ (:width glyph 0.0))
                                           (+ (:left-bearing glyph 0.0)))]
                    (.append sb ch)
                    (cond
                      ;; If the char is whitespace, save its index and continue
                      white-space
                      (recur (inc i) lines (dec (.length sb)) (+ line-width-with-tracking (:advance glyph 0.0)))

                      ;; If there is no white space index, we never wrap: continue
                      ;; Additionally, if we don't exceed the line limit, we also continue
                      (or (neg? known-white-space-index)
                          (<= end-line-width max-width))
                      (recur (inc i) lines known-white-space-index (+ line-width-with-tracking (:advance glyph 0.0)))

                      ;; At this point, we know there is whitespace in this line, and also
                      ;; we exceed the line limit. We wrap the line after the last known
                      ;; whitespace index
                      :else
                      (let [wrapped-text-length (- (.length sb) (inc known-white-space-index))]
                        (.setLength sb known-white-space-index)
                        (recur (- i wrapped-text-length) (add-line! lines sb) -1 0.0)))))))))))
    (s/split-lines text)))

(defn- font-map->glyphs [font-map]
  (into {} (map (fn [g] [(:character g) g])) (:glyphs font-map)))

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
           line-widths (map (partial measure-line (:is-monospaced font-map) (:padding font-map) glyphs text-tracking) lines)
           max-width (reduce max 0 line-widths)]
       [max-width (* line-height (+ 1 (* text-leading (dec (count lines)))))]))))

(g/deftype FontData {:type     schema/Keyword
                     :font-map schema/Any
                     :texture  schema/Any})

(defn- place-glyph [glyph-cache glyph]
  (let [placed-glyph (glyph-cache glyph)]
    (if-not (= ::not-available placed-glyph)
      (assoc glyph :x (:x placed-glyph) :y (:y placed-glyph))
      (assoc glyph :width 0))))

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
            line-widths (mapv (partial measure-line (:is-monospaced font-map) (:padding font-map) glyphs text-tracking) lines)
            max-width (reduce max 0 line-widths)]
        (assoc text-layout
               :width max-width
               :height (* line-height (+ 1 (* text-leading (dec (count lines)))))
               :lines lines
               :line-widths line-widths)))))

(defn glyph-count
  [text-entries]
  (transduce (comp
               (mapcat (comp :lines :text-layout))
               (map count))
             +
             text-entries))

(defn- vertex-count
  [text-entries layer-count]
  (* 6 (glyph-count text-entries) layer-count))

(defn- get-layers-in-mask
  [layer-mask]
  (map (fn [bit]
         (min (bit-and layer-mask bit) 1))
       [layer-mask-face layer-mask-outline layer-mask-shadow]))

(defn- count-layers-in-mask
  [layer-mask]
  (Integer/bitCount layer-mask))

(defn- make-vbuf
  [type text-entries layer-mask]
  (let [layer-count (count-layers-in-mask layer-mask)
        vcount (vertex-count text-entries layer-count)]
    (case type
      (:defold :bitmap) (->DefoldVertex vcount)
      :distance-field   (->DFVertex vcount))))


(defn- fill-vertex-buffer-quads
  [vbuf text-entries put-pos-uv-fn line-height char->glyph glyph-cache put-glyph-quad-fn unpacked-layer-mask text-cursor-offset alpha outline-alpha shadow-alpha]
  (reduce (fn [vbuf entry]
            (let [alpha (or alpha 1.0)
                  outline-alpha (or outline-alpha 1.0)
                  shadow-alpha (or shadow-alpha 1.0)
                  put-pos-uv-fn (wrap-with-feature-data put-pos-uv-fn
                                                        (mapv (partial * alpha) (:color entry))
                                                        (update (:outline entry) 3 (partial * outline-alpha))
                                                        (update (:shadow entry) 3 (partial * shadow-alpha))
                                                        unpacked-layer-mask)
                  text-layout (:text-layout entry)
                  text-tracking (* line-height ^double (:text-tracking text-layout 0))
                  text-cursor-offset (if (nil? text-cursor-offset)
                                       {:x 0, :y 0}
                                       text-cursor-offset)
                  ^double text-leading (:text-leading text-layout 1)
                  offset (:offset entry)
                  xform (doto (Matrix4d.)
                          (.set (let [[x y] offset]
                                  (Vector3d. x y 0.0))))
                  _ (.mul xform ^Matrix4d (:world-transform entry) xform)
                  ^double max-width (:width text-layout)
                  align (:align entry :center)]
              (loop [vbuf vbuf
                     [line & lines] (:lines text-layout)
                     [^double line-width & line-widths] (:line-widths text-layout)
                     line-no 0]
                (if line
                  (let [y (* line-no (- (* line-height text-leading)))]
                    (loop [vbuf vbuf
                           [glyph & glyphs] (map char->glyph line)
                           x (case align
                               :left 0.0
                               :center (* 0.5 (- max-width line-width))
                               :right (- max-width line-width))]
                      (if glyph
                        (let [glyph (place-glyph glyph-cache glyph)]
                          (if (and (:x glyph) (:y glyph))
                            (let [cursor (doto (Matrix4d.) (.set (Vector3d. (+ x (:x text-cursor-offset)) (+ y (:y text-cursor-offset)) 0.0)))
                                  cursor (doto cursor (.mul xform cursor))]
                              (recur (put-glyph-quad-fn vbuf glyph cursor put-pos-uv-fn)
                                     glyphs
                                     (+ x ^double (:advance glyph) text-tracking)))
                            vbuf))
                        vbuf))
                    (recur vbuf lines line-widths (inc line-no)))
                  vbuf))))
          vbuf
          text-entries))

(defn- fill-vertex-buffer
  [^GL2 gl vbuf {:keys [type font-map texture] :as font-data} text-entries glyph-cache]
  (let [put-glyph-quad-fn (make-put-glyph-quad-fn font-map)
        put-pos-uv-fn (cond-> put-pos-uv!
                              (= type :distance-field)
                              (wrap-with-sdf-params font-map))
        [_ outline-enabled shadow-enabled] (mapv protobuf/int->boolean (get-layers-in-mask (:layer-mask font-map)))
        layer-mask-enabled (> (count-layers-in-mask (:layer-mask font-map)) 1)
        char->glyph (comp (font-map->glyphs font-map) int)
        line-height (+ ^long (:max-ascent font-map) ^long (:max-descent font-map))
        face-mask (if layer-mask-enabled
                    [1 0 0]
                    [1 1 1])
        shadow-offset {:x (if (:is-monospaced font-map)
                            (- (:shadow-x font-map) (* (:padding font-map) 0.5))
                            (:shadow-x font-map))
                       :y (:shadow-y font-map)}
        alpha (:alpha font-map)
        outline-alpha (:outline-alpha font-map)
        shadow-alpha (:shadow-alpha font-map)
        font-offset {:x (if (:is-monospaced font-map)
                          (- 0 (* (:padding font-map) 0.5))
                          0)
                     :y 0}]
    (when (and layer-mask-enabled shadow-enabled) (fill-vertex-buffer-quads vbuf text-entries put-pos-uv-fn line-height char->glyph glyph-cache put-glyph-quad-fn [0 0 1] shadow-offset alpha outline-alpha shadow-alpha))
    (when (and layer-mask-enabled outline-enabled) (fill-vertex-buffer-quads vbuf text-entries put-pos-uv-fn line-height char->glyph glyph-cache put-glyph-quad-fn [0 1 0] font-offset alpha outline-alpha shadow-alpha))
    (fill-vertex-buffer-quads vbuf text-entries put-pos-uv-fn line-height char->glyph glyph-cache put-glyph-quad-fn face-mask font-offset alpha outline-alpha shadow-alpha)))

(defn gen-vertex-buffer
  [^GL2 gl {:keys [type font-map] :as font-data} text-entries]
  (let [vbuf (make-vbuf type text-entries (:layer-mask font-map))
        glyph-cache (scene-cache/request-object! ::glyph-caches (:texture font-data) gl
                                                 (select-keys font-data [:font-map :texture]))]
    (vtx/flip! (fill-vertex-buffer gl vbuf font-data text-entries glyph-cache))))

(defn request-vertex-buffer
  [^GL2 gl request-id font-data text-entries]
  (let [glyph-cache (scene-cache/request-object! ::glyph-caches (:texture font-data) gl
                                                 (select-keys font-data [:font-map :texture]))
        layer-count (count-layers-in-mask (:layer-mask (:font-map font-data)))]
    (scene-cache/request-object! ::vb [request-id (:type font-data) (vertex-count text-entries layer-count)] gl
                                 {:font-data font-data
                                  :text-entries text-entries
                                  :glyph-cache glyph-cache})))

(defn get-texture-recip-uniform [font-map]
  (let [cache-width (:cache-width font-map)
        cache-height (:cache-height font-map)
        cache-width-recip (/ 1.0 cache-width)
        cache-height-recip (/ 1.0 cache-height)
        cache-cell-width-ratio (/ (:cache-cell-width font-map) cache-width)
        cache-cell-height-ratio (/ (:cache-cell-height font-map) cache-height)]
    (Vector4d. cache-width-recip cache-height-recip cache-cell-width-ratio cache-cell-height-ratio)))

(defn render-font [^GL2 gl render-args renderables rcount]
  (let [user-data (get (first renderables) :user-data)
        gpu-texture (:texture user-data)
        font-map (:font-map user-data)
        text-layout (:text-layout user-data)
        vertex-buffer (gen-vertex-buffer gl user-data [{:text-layout text-layout
                                                        :align :left
                                                        :offset [0.0 0.0]
                                                        :world-transform (doto (Matrix4d.) (.setIdentity))
                                                        :color (colors/alpha colors/defold-white-light 1.0) :outline (colors/alpha colors/mid-grey 1.0) :shadow [0.0 0.0 0.0 1.0]}])
        material-shader (:shader user-data)
        type (:type user-data)
        vcount (count vertex-buffer)]
    (when (> vcount 0)
      (let [vertex-binding (vtx/use-with ::vb vertex-buffer material-shader)
            texture-recip-uniform (get-texture-recip-uniform font-map)]
        (gl/with-gl-bindings gl render-args [material-shader vertex-binding gpu-texture]
          (shader/set-uniform material-shader gl "texture_size_recip" texture-recip-uniform)
          ;; Need to set the blend mode to alpha since alpha blending the source with GL_SRC_ALPHA and dest with GL_ONE_MINUS_SRC_ALPHA
          ;; gives us a small black border around the outline that looks different than other views..
          (gl/set-blend-mode gl :blend-mode-alpha)
          (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)
          (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))))

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
                                  :tags #{:font}
                                  :batch-key gpu-texture
                                  :select-batch-key _node-id
                                  :user-data {:type type
                                              :texture gpu-texture
                                              :font-map font-map
                                              :shader material-shader
                                              :text-layout (layout-text font-map preview-text true (:cache-width font-map) 0 1)
                                              :text preview-text}
                                  :passes [pass/transparent]}))))

(g/defnk produce-save-value
  [font material size antialias alpha outline-alpha outline-width
   shadow-alpha shadow-blur shadow-x shadow-y characters output-format
   all-chars cache-width cache-height render-mode]
  (protobuf/make-map-without-defaults Font$FontDesc
    :font (resource/resource->proj-path font)
    :material (resource/resource->proj-path material)
    :size size
    :antialias (protobuf/boolean->int antialias)
    :alpha alpha
    :outline-alpha outline-alpha
    :outline-width outline-width
    :shadow-alpha shadow-alpha
    :shadow-blur shadow-blur
    :shadow-x shadow-x
    :shadow-y shadow-y
    :characters characters
    :output-format output-format
    :all-chars all-chars
    :cache-width cache-width
    :cache-height cache-height
    :render-mode render-mode))

(defn- make-font-map [_node-id font type pb-msg font-resource-resolver]
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
      (try
        (font-gen/generate pb-msg font font-resource-resolver)
        (catch Exception error
          (let [message (str "Failed to generate bitmap from Font. " (.getMessage error))]
            (log/error :msg message :exception error)
            (g/->error _node-id :font :fatal font message))))))

(defn- make-font-resource-resolver [font resource-map]
  (let [base-path (resource/parent-proj-path (resource/proj-path font))]
    (fn [path]
      (let [proj-path (str base-path "/" path)]
        (get resource-map proj-path)))))

(g/defnk produce-font-map [_node-id font type font-resource-map save-value]
  ;; TODO(save-value-cleanup): make-font-map expects all values to be present.
  (let [font-desc (protobuf/inject-defaults Font$FontDesc save-value)]
    (make-font-map _node-id font type font-desc (make-font-resource-resolver font font-resource-map))))

(defn- build-glyph-bank [resource _dep-resources user-data]
  (let [{:keys [font-map]} user-data]
    (g/precluding-errors
      [font-map]
      (let [compressed-font-map (font-gen/compress font-map)]
        {:resource resource
         :content (protobuf/map->bytes Font$GlyphBank compressed-font-map)}))))

(defn- make-glyph-bank-build-target [workspace node-id user-data]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "glyph_bank")
     :build-fn build-glyph-bank
     :user-data user-data}))

(g/defnk produce-build-targets [_node-id resource cache-width cache-height font-map material dep-build-targets runtime-generation-build-target]
  (or (when-let [errors (->> [(validation/prop-error :fatal _node-id :material validation/prop-nil? material "Material")
                              (validation/prop-error :fatal _node-id :material validation/prop-resource-not-exists? material "Material")]
                             (remove nil?)
                             (not-empty))]
        (g/error-aggregate errors))
      (let [workspace (resource/workspace resource)
            pb-map (protobuf/make-map-without-defaults Font$FontMap
                     :material material
                     :size (:size font-map)
                     :antialias (:antialias font-map)
                     :shadow-x (:shadow-x font-map)
                     :shadow-y (:shadow-y font-map)
                     :shadow-blur (:shadow-blur font-map)
                     :shadow-alpha (:shadow-alpha font-map)
                     :alpha (:alpha font-map)
                     :outline-alpha (:outline-alpha font-map)
                     :outline-width (:outline-width font-map)
                     :layer-mask (:layer-mask font-map)
                     :output-format (:output-format font-map)
                     :render-mode (:render-mode font-map)
                     :all-chars (:all-chars font-map)
                     :characters (:characters font-map)
                     :cache-width cache-width
                     :cache-height cache-height
                     :sdf-spread (:sdf-spread font-map)
                     :sdf-outline (:sdf-outline font-map)
                     :sdf-shadow (:sdf-shadow font-map))

            [pb-map dep-build-targets]
            (if (and runtime-generation-build-target
                     (= :type-distance-field (:output-format font-map))) ; Currently, only distance field fonts can be runtime-generated.
              [(assoc pb-map :font (:resource runtime-generation-build-target))
               (conj dep-build-targets runtime-generation-build-target)]
              (let [glyph-bank-pb-fields (protobuf/field-key-set Font$GlyphBank)
                    glyph-bank-user-data {:font-map (select-keys font-map glyph-bank-pb-fields)}
                    glyph-bank-build-target (make-glyph-bank-build-target workspace _node-id glyph-bank-user-data)]
                [(assoc pb-map :glyph-bank (:resource glyph-bank-build-target))
                 (conj dep-build-targets glyph-bank-build-target)]))]

        [(pipeline/make-protobuf-build-target _node-id resource Font$FontMap pb-map dep-build-targets)])))

(g/defnode BitmapFontSourceNode
  (inherits resource-node/ResourceNode)
  (property texture resource/Resource ; Nil is valid default.
            (value (gu/passthrough texture-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :texture-resource]
                                            [:size :texture-size]))))
  (input texture-resource resource/Resource)
  (input texture-size g/Any) ; we pipe in size to provoke errors, for instance if the texture node is defective / non-existant
  (output font-resource-map g/Any
          (g/fnk [texture-resource texture-size]
            (when texture-resource
              {(resource/proj-path texture-resource) texture-resource}))))

(defn- read-bm-font
  ^BMFont [^InputStream input-stream]
  (doto (BMFont.)
    (.parse input-stream)))

(defn load-bitmap-font-source [_project self resource]
  (let [[^BMFont bm-font disk-sha256] (resource/read-source-value+sha256-hex resource read-bm-font)]
    (let [;; this weird dance stolen from Fontc.java
          texture-file-name (-> bm-font
                                (.page)
                                (.get 0)
                                (FilenameUtils/normalize)
                                (Paths/get (into-array String []))
                                (.getFileName)
                                (.toString))
          texture-resource (workspace/resolve-resource resource texture-file-name)]
      (concat
        (g/set-property self :texture texture-resource)
        (when disk-sha256
          (let [workspace (resource/workspace resource)]
            (workspace/set-disk-sha256 workspace self disk-sha256)))))))

(g/defnode TrueTypeFontSourceNode
  (inherits resource-node/ResourceNode)
  (input project-settings g/Any)
  (output runtime-generation-build-target g/Any :cached
          (g/fnk [_node-id project-settings resource]
            (when (get project-settings ["font" "runtime_generation"])
              (resource-io/with-error-translation resource _node-id :runtime-generation-build-target
                (pipeline/make-source-bytes-build-target _node-id resource))))))

(defn load-true-type-font-source [project self _resource]
  (g/connect project :settings self :project-settings))

(g/defnode OpenTypeFontSourceNode
  (inherits resource-node/ResourceNode))

(g/defnk produce-font-type [font output-format]
  (font-type font output-format))

(defn- char->string [c]
  (String. (Character/toChars c)))

(g/defnk produce-preview-text [font-map]
  (let [char->glyph (into {} (map (juxt :character identity)) (:glyphs font-map))
        chars (->> char->glyph keys sort (filter #(pos? (:width (char->glyph %)))))
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

  (property font resource/Resource ; Required protobuf field.
            (value (gu/passthrough font-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :font-resource]
                                            [:font-resource-map :font-resource-map]
                                            [:runtime-generation-build-target :runtime-generation-build-target])))
            (dynamic error (g/fnk [_node-id font-resource]
                             (or (validation/prop-error :fatal _node-id :font validation/prop-nil? font-resource "Font")
                                 (validation/prop-error :fatal _node-id :font validation/prop-resource-not-exists? font-resource "Font"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource
                                  :ext font-file-extensions}))
            (dynamic label (properties/label-dynamic :font :font))
            (dynamic tooltip (properties/tooltip-dynamic :font :font)))

  (property material resource/Resource ; Required protobuf field.
    (value (gu/passthrough material-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter evaluation-context self old-value new-value
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

  (property output-format g/Keyword (default (protobuf/default Font$FontDesc :output-format))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Font$FontTextureFormat)))
            (dynamic label (properties/label-dynamic :font :output-format))
            (dynamic tooltip (properties/tooltip-dynamic :font :output-format)))
  (property render-mode g/Keyword (default (protobuf/default Font$FontDesc :render-mode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Font$FontRenderMode)))
            (dynamic label (properties/label-dynamic :font :render-mode))
            (dynamic tooltip (properties/tooltip-dynamic :font :render-mode)))
  (property size g/Int (default (protobuf/required-default Font$FontDesc :size))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? size))
            (dynamic label (properties/label-dynamic :font :size))
            (dynamic tooltip (properties/tooltip-dynamic :font :size)))
  (property antialias g/Bool (default (protobuf/int->boolean (protobuf/default Font$FontDesc :antialias)))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic label (properties/label-dynamic :font :antialias))
            (dynamic tooltip (properties/tooltip-dynamic :font :antialias)))
  (property alpha g/Num (default (protobuf/default Font$FontDesc :alpha))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type))
            (dynamic label (properties/label-dynamic :font :alpha))
            (dynamic tooltip (properties/tooltip-dynamic :font :alpha)))
  (property outline-alpha g/Num (default (protobuf/default Font$FontDesc :outline-alpha))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? outline-alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type))
            (dynamic label (properties/label-dynamic :font :outline-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :font :outline-alpha)))
  (property outline-width g/Num (default (protobuf/default Font$FontDesc :outline-width))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? outline-width))
            (dynamic edit-type (g/constantly shadows-outline-slider-edit-type))
            (dynamic label (properties/label-dynamic :font :outline-width))
            (dynamic tooltip (properties/tooltip-dynamic :font :outline-width)))
  (property shadow-alpha g/Num (default (protobuf/default Font$FontDesc :shadow-alpha))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? shadow-alpha))
            (dynamic edit-type (g/constantly alpha-slider-edit-type))
            (dynamic label (properties/label-dynamic :font :shadow-alpha))
            (dynamic tooltip (properties/tooltip-dynamic :font :shadow-alpha)))
  (property shadow-blur g/Num (default (protobuf/default Font$FontDesc :shadow-blur))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? shadow-blur))
            (dynamic edit-type (g/constantly shadows-outline-slider-edit-type))
            (dynamic label (properties/label-dynamic :font :shadow-blur))
            (dynamic tooltip (properties/tooltip-dynamic :font :shadow-blur)))
  (property shadow-x g/Num (default (protobuf/default Font$FontDesc :shadow-x))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic label (properties/label-dynamic :font :shadow-x))
            (dynamic tooltip (properties/tooltip-dynamic :font :shadow-x)))
  (property shadow-y g/Num (default (protobuf/default Font$FontDesc :shadow-y))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic label (properties/label-dynamic :font :shadow-y))
            (dynamic tooltip (properties/tooltip-dynamic :font :shadow-y)))
  (property cache-width g/Int (default (protobuf/default Font$FontDesc :cache-width))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? cache-width))
            (dynamic label (properties/label-dynamic :font :cache-width))
            (dynamic tooltip (properties/tooltip-dynamic :font :cache-width)))
  (property cache-height g/Int (default (protobuf/default Font$FontDesc :cache-height))
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-negative? cache-height))
            (dynamic label (properties/label-dynamic :font :cache-height))
            (dynamic tooltip (properties/tooltip-dynamic :font :cache-height)))
  (property characters g/Str (default (protobuf/default Font$FontDesc :characters))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic read-only? (gu/passthrough all-chars))
            (set (fn [evaluation-context self _old-value new-value]
                   ;; It is illegal to have no characters in the font, but it is
                   ;; also useful to be able to reset the field to the default
                   ;; set of characters. If the user clears the field, we reset
                   ;; to the printable ASCII characters.
                   (when (and (= "" new-value)
                              (properties/user-edit? self :characters evaluation-context))
                     (g/set-property self :characters fontc/default-characters-string))))
            (dynamic label (properties/label-dynamic :font :characters))
            (dynamic tooltip (properties/tooltip-dynamic :font :characters)))
  (property all-chars g/Bool (default (protobuf/default Font$FontDesc :all-chars))
            (dynamic visible output-format-defold-or-distance-field?)
            (dynamic label (properties/label-dynamic :font :all-chars))
            (dynamic tooltip (properties/tooltip-dynamic :font :all-chars)))

  (input runtime-generation-build-target g/Any)
  (input dep-build-targets g/Any :array)
  (input font-resource resource/Resource)
  (input material-resource resource/Resource)
  (input material-samplers [g/KeywordMap])
  (input material-shader ShaderLifecycle)
  (input font-resource-map g/Any)

  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output font-map g/Any :cached produce-font-map)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB (g/fnk [font-map preview-text]
                           (if font-map
                             (let [[w h] (measure font-map preview-text true (:cache-width font-map) 0 1)
                                   h-offset (:max-ascent font-map)]
                               (geom/make-aabb (Point3d. 0 h-offset 0)
                                               (Point3d. w (- h-offset h) 0)))
                             geom/null-aabb)))
  (output gpu-texture g/Any :cached (g/fnk [_node-id font-map material-samplers]
                                           (when font-map
                                             (let [w (:cache-width font-map)
                                                   h (:cache-height font-map)
                                                   channels (:glyph-channels font-map)
                                                   data-format (glyph-channels->data-format channels)]
                                               (texture/empty-texture _node-id data-format w h
                                                                      (material/sampler->tex-params (first material-samplers)) 0)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output type g/Keyword produce-font-type)
  (output font-data FontData :cached (g/fnk [type gpu-texture font-map]
                                            {:type type
                                             :texture gpu-texture
                                             :font-map font-map}))
  (output preview-text g/Str :cached produce-preview-text))

(defn load-font [_project self resource font-desc]
  {:pre [(map? font-desc)]} ; Font$FontDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (gu/set-properties-from-pb-map self Font$FontDesc font-desc
      font (resolve-resource :font)
      material (resolve-resource :material)
      size :size
      antialias (protobuf/int->boolean :antialias)
      alpha :alpha
      outline-alpha :outline-alpha
      outline-width :outline-width
      shadow-alpha :shadow-alpha
      shadow-blur :shadow-blur
      shadow-x :shadow-x
      shadow-y :shadow-y
      characters :characters
      output-format :output-format
      all-chars :all-chars
      cache-width :cache-width
      cache-height :cache-height
      render-mode :render-mode)))

(defn sanitize-font [{:keys [characters extra-characters] :as font-desc}]
  {:pre [(map? font-desc)]} ; Font$FontDesc in map format.
  ;; In a previous file format, we would always include all printable ASCII
  ;; characters in the font, and the user could specify :extra-characters to
  ;; include in addition to the ASCII characters.
  ;; Now, we instead explicitly list the :characters to include, but default to
  ;; the printable ASCII characters.
  (let [explicit-characters
        (if (pos? (count characters))
          characters
          fontc/default-characters-string)

        distinct-extra-characters
        (when (pos? (count extra-characters))
          ;; We have extra-characters. Remove duplicates so we can add them to
          ;; the characters field in the new file format.
          (let [distinct-extra-characters
                (into []
                      (comp (remove (set explicit-characters))
                            (distinct))
                      extra-characters)]
            (when (pos? (count distinct-extra-characters))
              (String. (char-array distinct-extra-characters)))))

        merged-characters
        (cond-> explicit-characters
                distinct-extra-characters (str distinct-extra-characters))]

    (-> font-desc
        (dissoc :extra-characters)
        (assoc :characters merged-characters))))

(defn register-resource-types [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :textual? true
      :ext "font"
      :label font-label
      :node-type FontNode
      :ddf-type Font$FontDesc
      :load-fn load-font
      :sanitize-fn sanitize-font
      :icon font-icon
      :icon-class :design
      :category (localization/message "resource.category.resources")
      :view-types [:scene :text])
    (workspace/register-resource-type workspace
      :ext "glyph_bank")
    (workspace/register-resource-type workspace
      :ext "fnt"
      :label font-label
      :node-type BitmapFontSourceNode
      :load-fn load-bitmap-font-source
      :icon font-icon
      :view-types [:default])
    (workspace/register-resource-type workspace
      :ext "ttf"
      :build-ext "ttf"
      :label font-label
      :node-type TrueTypeFontSourceNode
      :load-fn load-true-type-font-source
      :stateless? true
      :icon font-icon
      :view-types [:default])
    (workspace/register-resource-type workspace
      :ext "otf"
      :label font-label
      :node-type OpenTypeFontSourceNode
      :icon font-icon
      :view-types [:default])))

(defn- make-glyph-cache
  [^GL2 gl params]
  (let [{:keys [font-map texture]} params
        {:keys [cache-width cache-height cache-cell-width cache-cell-height cache-cell-max-ascent]} font-map
        data-format (glyph-channels->data-format (:glyph-channels font-map))
        size (* (int (/ cache-width cache-cell-width)) (int (/ cache-height cache-cell-height)))
        cells-per-row (int (/ cache-width cache-cell-width))
        cache (atom {})]
    (fn
      ([] ; this arity allows for retrieving the underlying atom when debugging/repl'ing
       cache)
      ([glyph]
       (get (swap! cache (fn [m]
                           (if (m glyph)
                             m
                             (let [cell (count m)]
                               (if-not (< cell size)
                                 (assoc m glyph ::not-available)
                                 (let [x (* (mod cell cells-per-row) cache-cell-width)
                                       y (+ (* (int (/ cell cells-per-row)) cache-cell-height) (- cache-cell-max-ascent (:ascent glyph)))
                                       {w :width h :height} (:glyph-cell-wh glyph)
                                       ^ByteBuffer src-data (-> ^ByteBuffer (.asReadOnlyByteBuffer ^ByteString (:glyph-data font-map))
                                                                ^ByteBuffer (.position (int (:glyph-data-offset glyph)))
                                                                (.slice)
                                                                (.limit (int (:glyph-data-size glyph))))
                                       tgt-data (doto (ByteBuffer/allocateDirect (:glyph-data-size glyph))
                                                  (.put src-data)
                                                  (.flip))]
                                   (when (> (:glyph-data-size glyph) 0)
                                     (texture/update-sub-image! texture gl 0 tgt-data data-format x y w h))
                                   (assoc m glyph {:x x :y y})))))))
            glyph)))))

(defn- update-glyph-cache [^GL2 gl glyph-cache params]
  (make-glyph-cache gl params))

(defn- destroy-glyph-caches [^GL2 gl font-caches _])

(scene-cache/register-object-cache! ::glyph-caches make-glyph-cache update-glyph-cache destroy-glyph-caches)

(defn- update-vb
  [^GL2 gl ^VertexBuffer vb {:keys [font-data text-entries glyph-cache]}]
  (vtx/clear! vb)
  (vtx/flip! (fill-vertex-buffer gl vb font-data text-entries glyph-cache)))

(defn- make-vb
  [^GL2 gl {:keys [font-data text-entries glyph-cache] :as data}]
  (let [vb (make-vbuf (:type font-data) text-entries (:layer-mask (:font-map font-data)))]
    (vtx/flip! (fill-vertex-buffer gl vb font-data text-entries glyph-cache))))

(defn- destroy-vbs
  [^GL2 gl vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb destroy-vbs)
