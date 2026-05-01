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

(ns editor.pipeline.fontc
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [util.coll :refer [pair]])
  (:import [com.defold.util IDigestable]
           [com.dynamo.bob.font BMFont BMFont$Char DistanceFieldGenerator Fontc]
           [com.dynamo.render.proto Font$FontDesc]
           [com.google.protobuf ByteString]
           [java.awt BasicStroke Canvas Color Composite CompositeContext Font FontMetrics Graphics Graphics2D RenderingHints Shape Transparency]
           [java.awt.color ColorSpace]
           [java.awt.font FontRenderContext GlyphVector]
           [java.awt.geom AffineTransform FlatteningPathIterator PathIterator Rectangle2D Area]
           [java.awt.image BufferedImage ComponentColorModel ConvolveOp DataBuffer DataBufferByte Kernel Raster WritableRaster]
           [java.io FileNotFoundException IOException InputStream]
           [java.nio.file Paths]
           [java.util Arrays]
           [javax.imageio ImageIO]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^String default-characters-string (String. Fontc/ASCII_7BIT))

(defn- next-pow2 [n]
  (assert (>= ^int n 0))
  (int (Math/pow 2 (Math/ceil (/ (Math/log (max ^int n 1.0)) (Math/log 2))))))

(defrecord WH [width height])

(defn- cache-wh [font-desc {^int cache-cell-width :width ^int cache-cell-height :height} glyph-count]
  (assert (and (> cache-cell-width 0) (> cache-cell-height 0)))
  (let [^int cache-width (if (> ^int (:cache-width font-desc) 0)
                           (:cache-width font-desc)
                           1024)
        ^int cache-height (if (> ^int (:cache-height font-desc) 0)
                            (:cache-height font-desc)
                            (let [^int cache-columns (/ cache-width ^int cache-cell-width)
                                  total-rows (int (Math/ceil (/ (double glyph-count) (double cache-columns))))
                                  total-height (* total-rows ^int cache-cell-height)]
                              (min ^int (next-pow2 total-height) (int 2048))))]
    (->WH cache-width cache-height)))

(defn- glyph-wh [{^int glyph-width :width
                  ^int glyph-ascent :ascent
                  ^int glyph-descent :descent}]
  (->WH glyph-width (+ glyph-ascent glyph-descent)))

(defn- pad-wh [padding {^int width :width ^int height :height}]
  (->WH (+ width (* ^int padding 2)) (+ height (* ^int padding 2))))

(defn- wh-size [{^int width :width ^int height :height}]
  (* width height))

(defn- positive-wh? [{:keys [^int width ^int height]}]
  (not (or (<= width 0) (<= height 0))))

(defn- make-glyph-extents [channel-count padding cell-padding semi-glyphs]
  (let [glyph-extents (map (fn [semi-glyph]
                             (let [wh (glyph-wh semi-glyph)
                                   image-wh (pad-wh padding wh)
                                   glyph-cell-wh (pad-wh cell-padding image-wh)
                                   glyph-data-size (if (positive-wh? wh) (* (int (wh-size glyph-cell-wh)) ^int channel-count) 0)]
                               {:glyph-wh wh
                                :image-wh image-wh
                                :glyph-cell-wh glyph-cell-wh
                                :glyph-data-size glyph-data-size}))
                           semi-glyphs)
        glyph-extents (map #(assoc %1 :glyph-data-offset %2)
                           glyph-extents
                           (reductions + 0 (map :glyph-data-size glyph-extents)))]
    glyph-extents))

(defrecord Glyph [character width image-width advance left-bearing ascent descent glyph-cell-wh glyph-data-offset glyph-data-size]
  IDigestable
  (digest [_ w]
    (doto w
      (.write "#dg/glyph[")
      (.write (str character))
      (.write " ")
      (.write (str width))
      (.write " ")
      (.write (str image-width))
      (.write " ")
      (.write (str advance))
      (.write " ")
      (.write (str left-bearing))
      (.write " ")
      (.write (str ascent))
      (.write " ")
      (.write (str descent))
      (.write " ")
      (.write (str (:width glyph-cell-wh)))
      (.write " ")
      (.write (str (:height glyph-cell-wh)))
      (.write " ")
      (.write (str glyph-data-offset))
      (.write " ")
      (.write (str glyph-data-size))
      (.write "]"))))

(defn- make-ddf-glyphs [semi-glyphs glyph-extents padding]
  (mapv
    (fn [glyph glyph-extents]
      (let [wh (:glyph-wh glyph-extents)
            width (if (positive-wh? wh)
                    (:width (:image-wh glyph-extents))
                    (:width wh))]
        (->Glyph
          #_character (:character glyph)
          #_width width
          #_image-width width
          #_advance (:advance glyph)
          #_left-bearing (:left-bearing glyph)
          #_ascent (+ ^int (:ascent glyph) ^int padding)
          #_descent (+ ^int (:descent glyph) ^int padding)
          #_glyph-cell-wh (:glyph-cell-wh glyph-extents)
          #_glyph-data-offset (:glyph-data-offset glyph-extents)
          #_glyph-data-size (:glyph-data-size glyph-extents))))
    semi-glyphs glyph-extents))

(defn- max-glyph-cell-wh [glyph-extents ^long line-height ^long padding ^long glyph-cell-padding]
  (let [glyph-cell-widths (into (vector-of :long)
                                (comp (filter (comp positive-wh? :glyph-wh))
                                      (map (comp :width :glyph-cell-wh)))
                                glyph-extents)
        ^long max-width (loop [max-width 0
                               index (dec (count glyph-cell-widths))]
                          (if (neg? index)
                            max-width
                            (let [^long width (glyph-cell-widths index)]
                              (recur (max width max-width)
                                     (dec index)))))
        max-height (+ line-height padding padding glyph-cell-padding glyph-cell-padding)]
    (if (or (zero? max-width) (zero? max-height))
      (throw (ex-info "No glyph size information. Incompatible font format?" {}))
      (->WH max-width max-height))))

(defn- draw-bm-font-glyph ^BufferedImage [glyph ^BufferedImage bm-image]
  (.getSubimage bm-image
                ^int (:x glyph)
                ^int (:y glyph)
                ^int (:width glyph)
                (+ ^int (:ascent glyph) ^int (:descent glyph))))

(defn- fnt-semi-glyphs [^BMFont bm-font]
  (let [semi-glyphs (for [index (range (.. bm-font charArray size))]
                      (let [^BMFont$Char c (.. bm-font charArray (get index))
                            ascent (- (.base bm-font) (.yoffset c))
                            descent (- (.height c) ascent)]
                        {:ascent ascent
                         :descent descent
                         :x (.x c)
                         :y (.y c)
                         :character (.id c)
                         :advance (double (.xadvance c))
                         :left-bearing (double (.xoffset c))
                         :width (.width c)
                         :image-width (.width c)}))]
    (when-not (seq semi-glyphs)
      (throw (ex-info "No character glyphs were included! Maybe turn on 'all_chars'?" {})))
    semi-glyphs))

(defn- make-glyph-data-bank ^bytes [glyph-extents]
  (byte-array (transduce (map :glyph-data-size) + 0 glyph-extents)))

(defn check-monospaced [semi-glyphs]
  ; We can't know if it's only one glyph. And chances are it's a dynamic font
  (if (<= (count semi-glyphs) 1)
    false
    (let [base-advance (:advance (first semi-glyphs))]
      (every? #(= base-advance (:advance %)) semi-glyphs))))

(def ^:private positive-glyph-extent-pairs-xf (comp (map pair) (filter (comp positive-wh? :glyph-wh second))))

(def ^:private ^Canvas metrics-canvas (Canvas.))

(defn- font-metrics ^FontMetrics [^Font font]
  ;; Fontc.java does .getFontMetrics on a Graphics2D created from a
  ;; BufferedImage and initialized with various RenderingHints depending
  ;; on whether the font is antialiased. Can't see any difference from
  ;; doing it like this instead.
  ;; This bug: https://bugs.openjdk.java.net/browse/JDK-8172139 seems to
  ;; indicate the font metrics could depend on the rendering
  ;; attributes though.
  (.getFontMetrics metrics-canvas font))

(defn- font-glyph-bounds ^Rectangle2D [^FontMetrics metrics]
  ;; Fontc.java does .getFontMetrics on a Graphics2D created from a
  ;; BufferedImage and initialized with various RenderingHints depending
  ;; on whether the font is antialiased. Can't see any difference from
  ;; doing it like this instead.
  ;; This bug: https://bugs.openjdk.java.net/browse/JDK-8172139 seems to
  ;; indicate the font metrics could depend on the rendering
  ;; attributes though.
  (let [image (BufferedImage. 1 1 BufferedImage/TYPE_3BYTE_BGR)
        g (.createGraphics image)]
    (.getMaxCharBounds metrics ^Graphics g)))

(defn- get-font-map-props [font-desc]
  {:material (str (:material font-desc) "c")
   :size (:size font-desc)
   :all-chars (:all-chars font-desc)
   :characters (:characters font-desc)
   :antialias (:antialias font-desc)
   :shadow-x (:shadow-x font-desc)
   :shadow-y (:shadow-y font-desc)
   :shadow-blur (:shadow-blur font-desc)
   :shadow-alpha (:shadow-alpha font-desc)
   :outline-alpha (:outline-alpha font-desc)
   :outline-width (:outline-width font-desc)
   :render-mode (:render-mode font-desc)
   :output-format (:output-format font-desc)
   :alpha (:alpha font-desc)})

(defn- get-font-metrics [font]
  (let [font-metrics (font-metrics font)
        ^Rectangle2D max-glyph-rect (font-glyph-bounds font-metrics)]
    {:max-ascent (.getMaxAscent font-metrics)
     :max-descent (.getMaxDescent font-metrics)
     :max-advance (.getMaxAdvance font-metrics)
     :max-width (.getWidth max-glyph-rect)
     :max-height (.getHeight max-glyph-rect)}))

(defn- compile-fnt-bitmap [font-desc font-resource resolver]
  (let [^BMFont bm-font (with-open [font-stream (io/input-stream font-resource)]
                          (doto (BMFont.)
                            (.parse font-stream)))
        semi-glyphs (fnt-semi-glyphs bm-font)
        padding 0
        glyph-cell-padding 1
        bm-image (let [path (.. (Paths/get (FilenameUtils/normalize (.. bm-font page (get 0))) (into-array String [])) getFileName toString)]
                   (try
                     (with-open [image-stream ^InputStream (resolver path)]
                       (ImageIO/read image-stream))
                     (catch FileNotFoundException e
                       (throw (ex-info (str "Could not find BMFont image resource: " path) {:path path} e)))
                     (catch IOException e
                       (throw (ex-info (str "Error while reading BMFont image resource:" path) {:path path} e)))))
        channel-count 4
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        ;; Note: see comment in compile-ttf-bitmap regarding the cache ascent/descent
        ^long cache-cell-max-ascent (reduce max 0 (map :ascent semi-glyphs))
        ^long cache-cell-max-descent (reduce max 0 (map :descent semi-glyphs))
        line-height (+ cache-cell-max-ascent cache-cell-max-descent)
        cache-cell-wh (max-glyph-cell-wh glyph-extents line-height padding glyph-cell-padding)
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)
        is-monospaced (check-monospaced semi-glyphs)]

    (doall
      (pmap (fn [[semi-glyph glyph-extents]]
              (let [glyph-image (draw-bm-font-glyph semi-glyph bm-image)
                    {:keys [image-wh glyph-cell-wh ^int glyph-data-size ^int glyph-data-offset]} glyph-extents
                    ^int image-width (:width image-wh)
                    ^int image-height (:height image-wh)
                    row-size (* (int (:width glyph-cell-wh)) channel-count)]
                (Arrays/fill glyph-data-bank glyph-data-offset (+ glyph-data-offset (* glyph-cell-padding row-size)) (unchecked-byte 0))
                (doseq [^int y (range image-height)]
                  (let [y-offset (+ glyph-data-offset
                                    (* glyph-cell-padding row-size)
                                    (* y row-size))]
                    (Arrays/fill glyph-data-bank y-offset (+ y-offset (* glyph-cell-padding channel-count)) (unchecked-byte 0))
                    (doseq [^int x (range image-width)]
                      (let [x-offset (+ y-offset
                                        (* glyph-cell-padding channel-count)
                                        (* x channel-count))
                            color (.getRGB glyph-image x y)
                            alpha (-> color (bit-shift-right 24) (bit-and 0xff))
                            blue (-> color (bit-and 0xff) (* alpha) (/ 255))
                            green (-> color (bit-shift-right 8) (bit-and 0xff) (* alpha) (/ 255))
                            red (-> color (bit-shift-right 16) (bit-and 0xff) (* alpha) (/ 255))]
                        (aset glyph-data-bank x-offset (unchecked-byte red))
                        (aset glyph-data-bank (+ x-offset 1) (unchecked-byte green))
                        (aset glyph-data-bank (+ x-offset 2) (unchecked-byte blue))
                        (aset glyph-data-bank (+ x-offset 3) (unchecked-byte alpha))))
                    (Arrays/fill glyph-data-bank
                                 (+ y-offset (* glyph-cell-padding channel-count) (* image-width channel-count))
                                 (+ y-offset (* glyph-cell-padding channel-count) (* image-width channel-count) (* glyph-cell-padding channel-count))
                                 (unchecked-byte 0))))
                (let [last-y-offset (- (+ glyph-data-offset glyph-data-size) row-size)]
                  (Arrays/fill glyph-data-bank last-y-offset (+ last-y-offset row-size) (unchecked-byte 0)))))
            (sequence positive-glyph-extent-pairs-xf
                      semi-glyphs
                      glyph-extents)))

    (let [max-ascent (+ (float (reduce max 0 (map :ascent semi-glyphs))) padding)]
      (merge
        (get-font-map-props font-desc)
        {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
         :size (.size bm-font)
         :max-ascent max-ascent
         :max-descent (+ (float (reduce max 0 (map :descent semi-glyphs))) padding)
         :image-format (:output-format font-desc)
         :layer-mask 0x1                                    ; Face layer only - we don't generate shadow or outline.
         :cache-width (:width cache-wh)
         :cache-height (:height cache-wh)
         :glyph-padding glyph-cell-padding
         :cache-cell-width (:width cache-cell-wh)
         :cache-cell-height (:height cache-cell-wh)
         :cache-cell-max-ascent max-ascent
         :glyph-channels channel-count
         :glyph-data (ByteString/copyFrom glyph-data-bank)
         :is-monospaced is-monospaced
         :padding padding}))))

(defn- do-blend-rasters [^Raster src ^Raster dst-in ^WritableRaster dst-out]
  (let [width (min (.getWidth src) (.getWidth dst-in) (.getWidth dst-out))
        height (min (.getHeight src) (.getHeight dst-in) (.getHeight dst-out))
        int0 (int 0)
        int1 (int 1)]
    (doseq [^int i (range width)
            ^int j (range height)]
      (let [sr (.getSampleFloat src i j 0)
            sg (.getSampleFloat src i j 1)]
        (if (> sr 0.0)
          (.setSample dst-out i j int0 sr)
          (.setSample dst-out i j 0 (.getSample dst-in i j 0)))
        (if (> sg 0.0)
          (.setSample dst-out i j int1 sg)
          (.setSample dst-out i j 1 (.getSample dst-in i j 1)))
        (.setSample dst-out i j 2 (.getSample dst-in i j 2))))))

(def ^:private blend-context (reify CompositeContext
                               (^void compose [this ^Raster src ^Raster dst-in ^WritableRaster dst-out]
                                 (do-blend-rasters src dst-in dst-out))
                               (^void dispose [this])))

(def ^:private blend-composite (reify Composite
                                 (createContext [this arg0 arg1 arg2]
                                   blend-context)))

(def ^:private ^ConvolveOp shadow-convolve
  (let [kernel-data (float-array [0.0625 0.1250 0.0625
                                  0.1250 0.2500 0.1250
                                  0.0625 0.1250 0.0625])
        kernel (Kernel. 3 3 kernel-data)
        hints (doto (RenderingHints. nil)
                (.put RenderingHints/KEY_COLOR_RENDERING RenderingHints/VALUE_COLOR_RENDER_QUALITY)
                (.put RenderingHints/KEY_DITHERING RenderingHints/VALUE_DITHER_DISABLE))]
    (ConvolveOp. kernel ConvolveOp/EDGE_NO_OP hints)))

(defn- set-high-quality [^Graphics2D g antialias]
  (doseq [[hint value] [[RenderingHints/KEY_ALPHA_INTERPOLATION RenderingHints/VALUE_ALPHA_INTERPOLATION_QUALITY]
                        [RenderingHints/KEY_COLOR_RENDERING RenderingHints/VALUE_COLOR_RENDER_QUALITY]
                        [RenderingHints/KEY_DITHERING RenderingHints/VALUE_DITHER_DISABLE]
                        [RenderingHints/KEY_INTERPOLATION RenderingHints/VALUE_INTERPOLATION_BICUBIC]
                        [RenderingHints/KEY_RENDERING RenderingHints/VALUE_RENDER_QUALITY]]]
    (.setRenderingHint g hint value))
  (doseq [[hint value] (if antialias
                         [[RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON]
                          [RenderingHints/KEY_STROKE_CONTROL RenderingHints/VALUE_STROKE_PURE]
                          [RenderingHints/KEY_FRACTIONALMETRICS RenderingHints/VALUE_FRACTIONALMETRICS_ON]
                          [RenderingHints/KEY_TEXT_ANTIALIASING RenderingHints/VALUE_TEXT_ANTIALIAS_ON]]
                         [[RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_OFF]
                          [RenderingHints/KEY_STROKE_CONTROL RenderingHints/VALUE_STROKE_NORMALIZE]
                          [RenderingHints/KEY_FRACTIONALMETRICS RenderingHints/VALUE_FRACTIONALMETRICS_OFF]
                          [RenderingHints/KEY_TEXT_ANTIALIASING RenderingHints/VALUE_TEXT_ANTIALIAS_OFF]])]
    (.setRenderingHint g hint value)))

(defn- draw-ttf-bitmap-glyph ^BufferedImage [{^int glyph-width :width
                                              ^int glyph-ascent :ascent
                                              ^int glyph-descent :descent
                                              ^double glyph-left-bearing :left-bearing
                                              ^GlyphVector glyph-vector :vector
                                              ^int glyph-character :character
                                              :as glyph}
                                             {^double font-desc-shadow-alpha :shadow-alpha
                                              ^double font-desc-outline-width :outline-width
                                              ^double font-desc-outline-alpha :outline-alpha
                                              ^double font-desc-alpha :alpha
                                              :as font-desc}
                                             padding
                                             font
                                             face-color
                                             outline-color]
  (let [^int padding padding
        width (+ glyph-width (* padding 2))
        height (+ glyph-ascent glyph-descent (* padding 2))
        dx (+ (- (int glyph-left-bearing)) padding)
        dy (+ glyph-ascent padding)
        antialias (protobuf/int->boolean (:antialias font-desc))
        image (BufferedImage. width height BufferedImage/TYPE_3BYTE_BGR)
        g (doto (.createGraphics image)
            (set-high-quality antialias)
            (.setBackground Color/BLACK)
            (.clearRect 0 0 (.getWidth image) (.getHeight image))
            (.translate dx dy))]
    (let [^Shape outline (.getOutline glyph-vector 0 0)
          stroke-width (* 2.0 font-desc-outline-width)]
      (when (> font-desc-shadow-alpha 0.0)
        (when (> font-desc-alpha 0.0)
          (.setPaint g (Color. 0.0 0.0 (* font-desc-shadow-alpha font-desc-alpha)))
          (.fill g outline))
        (when (and (> font-desc-outline-width 0.0)
                   (> font-desc-outline-alpha 0.0))
          (let [outline-stroke (BasicStroke. stroke-width)]
            (.setPaint g (Color. 0.0 0.0 (* font-desc-shadow-alpha font-desc-outline-alpha)))
            (.setStroke g outline-stroke)
            (.draw g outline)))
        (dotimes [n (:shadow-blur font-desc)]
          (let [tmp (.getSubimage image 0 0 width height)]
            (.filter shadow-convolve tmp image))))
      (.setComposite g blend-composite)
      (when (and (> font-desc-outline-width 0.0)
                 (> font-desc-outline-alpha 0.0))
        (let [outline-stroke (BasicStroke. stroke-width)]
          (.setPaint g outline-color)
          (.setStroke g outline-stroke)
          (.draw g outline)))
      (when (> font-desc-alpha 0.0)
        (.setPaint g face-color)
        (.fill g outline)))
    (do
      (.setPaint g face-color)
      (.setFont g font)
      (.drawString g (String. (Character/toChars glyph-character)) 0 0))
    image))


(def ^:private ^AffineTransform identity-transform (AffineTransform.))
(def ^:private plain-font-render-context (FontRenderContext. identity-transform (boolean false) (boolean false)))
(def ^:private antialiased-font-render-context (FontRenderContext. identity-transform (boolean true) (boolean true)))

(defn- font-render-context ^FontRenderContext [antialias]
  (if antialias
    antialiased-font-render-context
    plain-font-render-context))

(defn- create-ttf-font ^Font [font-desc font-resource]
  (with-open [font-stream (io/input-stream font-resource)]
    (-> (Font/createFont Font/TRUETYPE_FONT font-stream)
        (.deriveFont Font/PLAIN (float (:size font-desc))))))

(defn- ttf-semi-glyph [^Font font antialias codepoint]
  (let [glyph-vector (.createGlyphVector font (font-render-context antialias) (Character/toChars codepoint))
        visual-bounds (.. glyph-vector getOutline getBounds)
        metrics (.getGlyphMetrics glyph-vector 0)
        left-bearing (double (.getLSB metrics))
        width (+ (.getWidth visual-bounds) (if (not= left-bearing 0.0) 1 0))]
    {:ascent (int (Math/ceil (- (.getMinY visual-bounds))))
     :descent (int (Math/ceil (.getMaxY visual-bounds)))
     :character codepoint
     :advance (double (Math/round (.getAdvance metrics)))
     :left-bearing (Math/floor left-bearing)
     :width width
     :image-width width
     :vector glyph-vector}))

(defn- ttf-semi-glyphs [font-desc ^Font font antialias]
  (let [prospect-codepoints (if (:all-chars font-desc)
                              (range 0x10ffff)
                              (sort
                                (eduction
                                  (map int)
                                  (distinct)
                                  (:characters font-desc))))
        displayable-codepoint? (fn [codepoint] (.canDisplay font ^int codepoint))
        semi-glyphs (into []
                          (comp
                            (filter displayable-codepoint?)
                            (map (partial ttf-semi-glyph font antialias))
                            (remove (fn [semi-glyph]
                                      (and (zero? (int (:width semi-glyph)))
                                           (zero? (double (:advance semi-glyph)))
                                           (<= 65000 (long (:character semi-glyph)))))))
                          prospect-codepoints)]
    (when-not (seq semi-glyphs)
      (throw (ex-info "No character glyphs were included! Maybe turn on 'all_chars'?" {})))
    semi-glyphs))

(def ^:private default-alpha (protobuf/default Font$FontDesc :alpha))
(def ^:private default-shadow-alpha (protobuf/default Font$FontDesc :shadow-alpha))
(def ^:private default-outline-alpha (protobuf/default Font$FontDesc :outline-alpha))
(def ^:private default-outline-width (protobuf/default Font$FontDesc :outline-width))
(def ^:private default-render-mode (protobuf/default Font$FontDesc :render-mode))

(defn font-desc->layer-mask [font-desc]
  (let [^double alpha (:alpha font-desc default-alpha)
        ^double shadow-alpha (:shadow-alpha font-desc default-shadow-alpha)
        ^double outline-alpha (:outline-alpha font-desc default-outline-alpha)
        ^double outline-width (:outline-width font-desc default-outline-width)
        render-mode (:render-mode font-desc default-render-mode)
        face-layer 0x1
        outline-layer (if (and (> outline-width 0.0)
                               (> outline-alpha 0.0)
                               (= render-mode :mode-multi-layer))
                        0x2 0x0)
        shadow-layer (if (and (> shadow-alpha 0.0)
                              (> alpha 0.0)
                              (= render-mode :mode-multi-layer))
                       0x4 0x0)]
    (bit-or face-layer outline-layer shadow-layer)))

(defn- compile-ttf-bitmap [font-desc font-resource]
  (let [font (create-ttf-font font-desc font-resource)
        antialias (protobuf/int->boolean (:antialias font-desc))
        semi-glyphs (ttf-semi-glyphs font-desc font antialias)
        padding (+ (min (int 4) ^int (:shadow-blur font-desc))
                   (int (:outline-width font-desc)))
        glyph-cell-padding 1
        channel-count (if (or (> ^double (:shadow-alpha font-desc) 0.0)
                              (and (> ^double (:outline-width font-desc) 0.0)
                                   (> ^double (:outline-alpha font-desc) 0.0)))
                        3 1)
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        ;; Note: We need to diverge between placing glyphs within the cache and placing them
        ;;       when rendering. For the cache placement, we need to know the _actual_
        ;;       max ascent/descent of a glyph set and not just rely on the values we get from
        ;;       the font-metrics as they yield wrong values sometimes. For backwards compat, we
        ;;       place the glyph quads in the same way as before.
        ^long cache-cell-max-ascent (reduce max 0 (map :ascent semi-glyphs))
        ^long cache-cell-max-descent (reduce max 0 (map :descent semi-glyphs))
        line-height (+ cache-cell-max-ascent cache-cell-max-descent)
        ;; BOB: "Some hardware don't like doing subimage updates on non-aligned cell positions."
        cache-cell-wh (cond-> (max-glyph-cell-wh glyph-extents line-height padding glyph-cell-padding)
                              (= channel-count 3)
                              (update :width #(* (int (Math/ceil (/ ^double % 4.0))) 4)))
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)
        layer-mask (font-desc->layer-mask font-desc)
        is-monospaced (check-monospaced semi-glyphs)]
    (doall
      (pmap (fn [[semi-glyph glyph-extents]]
              (let [^BufferedImage glyph-image (let [face-color (Color. ^double (:alpha font-desc) 0.0 0.0)
                                                     outline-color (Color. 0.0 ^double (:outline-alpha font-desc) 0.0)]
                                                 (draw-ttf-bitmap-glyph semi-glyph font-desc padding font face-color outline-color))
                    {:keys [image-wh glyph-cell-wh ^int glyph-data-size ^int glyph-data-offset]} glyph-extents
                    ^int image-width (:width image-wh)
                    ^int image-height (:height image-wh)
                    row-size (* (int (:width glyph-cell-wh)) channel-count)]
                (Arrays/fill glyph-data-bank glyph-data-offset (+ glyph-data-offset (* glyph-cell-padding row-size)) (unchecked-byte 0))
                (doseq [^int y (range image-height)]
                  (let [y-offset (+ glyph-data-offset
                                    (* glyph-cell-padding row-size)
                                    (* y row-size))]
                    (Arrays/fill glyph-data-bank y-offset (+ y-offset (* glyph-cell-padding channel-count)) (unchecked-byte 0))
                    (doseq [^int x (range image-width)]
                      (let [x-offset (+ y-offset
                                        (* glyph-cell-padding channel-count)
                                        (* x channel-count))
                            color (.getRGB glyph-image x y)
                            alpha (-> color (bit-shift-right 24) (bit-and 0xff))
                            blue (-> color (bit-and 0xff) (* alpha) (/ 255))
                            green (-> color (bit-shift-right 8) (bit-and 0xff) (* alpha) (/ 255))
                            red (-> color (bit-shift-right 16) (bit-and 0xff) (* alpha) (/ 255))]
                        (aset glyph-data-bank x-offset (unchecked-byte red))
                        (when (= channel-count 3)
                          (aset glyph-data-bank (+ x-offset 1) (unchecked-byte green))
                          (aset glyph-data-bank (+ x-offset 2) (unchecked-byte blue)))))
                    (Arrays/fill glyph-data-bank
                                 (+ y-offset (* glyph-cell-padding channel-count) (* image-width channel-count))
                                 (+ y-offset (* glyph-cell-padding channel-count) (* image-width channel-count) (* glyph-cell-padding channel-count))
                                 (unchecked-byte 0))
                    (let [last-y-offset (- (+ glyph-data-offset glyph-data-size) row-size)]
                      (Arrays/fill glyph-data-bank last-y-offset (+ last-y-offset row-size) (unchecked-byte 0)))))))
            (sequence positive-glyph-extent-pairs-xf
                      semi-glyphs
                      glyph-extents)))
    (merge
      (get-font-map-props font-desc)
      (get-font-metrics font)
      {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
       :image-format (:output-format font-desc)
       :layer-mask layer-mask
       :cache-width (:width cache-wh)
       :cache-height (:height cache-wh)
       :glyph-padding glyph-cell-padding
       :cache-cell-width (:width cache-cell-wh)
       :cache-cell-height (:height cache-cell-wh)
       :cache-cell-max-ascent (+ cache-cell-max-ascent padding)
       :glyph-channels channel-count
       :glyph-data (ByteString/copyFrom glyph-data-bank)
       :is-monospaced is-monospaced
       :padding padding})))

(defn- calculate-ttf-distance-field-edge-limit [^double width ^double spread ^double edge]
  (let [sdf-limit-value (- (/ width spread))]
    (+ (* sdf-limit-value (- 1.0 edge)) edge)))

(defn- draw-ttf-distance-field [{^int glyph-ascent :ascent
                                 glyph-left-bearing :left-bearing
                                 ^GlyphVector glyph-vector :vector
                                 :as glyph}
                                ^DistanceFieldGenerator distance-field-generator
                                padding
                                channel-count
                                outline-width
                                sdf-outline
                                sdf-spread
                                sdf-shadow-spread
                                edge
                                shadow-blur
                                shadow-alpha]
  (let [glyph-left-bearing (double glyph-left-bearing)
        ^int padding padding
        outline-width (double outline-width)
        sdf-spread (double sdf-spread)
        sdf-shadow-spread (double sdf-shadow-spread)
        edge (double edge)
        shadow-alpha (double shadow-alpha)
        sdf-outline (double sdf-outline)
        ^int channel-count channel-count
        ^int shadow-blur shadow-blur
        {^int width :width ^int height :height} (pad-wh padding (glyph-wh glyph))
        ;; Normalize the outline by boolean-unioning overlapping contours to avoid
        ;; internal edges contributing to the distance field (fixes artifacts for
        ;; glyphs composed of multiple vector shapes; see issue #6577)
        ^Shape glyph-outline (let [area (Area. (.getGlyphOutline glyph-vector 0))] area)
        ^PathIterator outline-iterator (FlatteningPathIterator. (.getPathIterator glyph-outline identity-transform) 0.1)
        segment-points (double-array 6 0.0)]

    (loop [x 0.0
           y 0.0
           lastmx 0.0
           lastmy 0.0]
      (when-not (.isDone outline-iterator)
        (let [segment-type (.currentSegment outline-iterator segment-points)]
          (.next outline-iterator)
          (condp = segment-type
            PathIterator/SEG_MOVETO
            (do (recur (aget segment-points 0)
                       (aget segment-points 1)
                       (aget segment-points 0)
                       (aget segment-points 1)))

            PathIterator/SEG_LINETO
            (do (.addLine distance-field-generator x y (aget segment-points 0) (aget segment-points 1))
                (recur (aget segment-points 0)
                       (aget segment-points 1)
                       lastmx
                       lastmy))

            PathIterator/SEG_CLOSE
            (do (.addLine distance-field-generator x y lastmx lastmy)
                (recur lastmx
                       lastmy
                       lastmx
                       lastmy))

            (do (assert false "Unexpected segment type"))))))
    (let [u0 (double (- glyph-left-bearing padding))
          v0 (double (- (+ glyph-ascent padding)))
          res (double-array (* width height))
          image (byte-array (* width height channel-count))]
      (.render distance-field-generator res u0 v0 (+ u0 width) (+ v0 height) width height)
      (doseq [^int v (range height)
              ^int u (range width)]
        (let [gx (+ u0 u)
              gy (+ v0 v)
              df-offset (+ (* v width) u)
              out-face-offset (* df-offset channel-count)
              out-outline-offset (+ out-face-offset 1)
              out-shadow-offset (+ out-face-offset 2)
              df-face' (aget res df-offset)
              df-outline' (- outline-width df-face')
              df-face (if-not (.contains glyph-outline gx gy) (- df-face') df-face')
              df-face-norm (+ (* (/ df-face sdf-spread) (- 1.0 edge)) edge)
              df-outline (if (> df-face-norm sdf-outline)
                           edge df-outline')
              df-face-channel (max 0 (min (int (* 255.0 df-face-norm)) 255))
              df-outline-norm (+ (* (/ df-outline sdf-shadow-spread) (- 1.0 edge)) edge)
              df-outline-channel (max 0 (min (int (* 255.0 df-outline-norm)) 255))]
          (aset image out-face-offset (unchecked-byte df-face-channel))
          (when (> channel-count 1)
            (aset image out-outline-offset (unchecked-byte 0))
            (aset image out-shadow-offset (unchecked-byte df-outline-channel)))))
      (when (and (> shadow-alpha 0.0) (> shadow-blur 0) (> channel-count 1))
        (let [image-byte-length (* width height channel-count)
              shadow-image (byte-array image-byte-length)
              _ (System/arraycopy image 0 shadow-image 0 image-byte-length)
              shadow-data-buffer (DataBufferByte. shadow-image image-byte-length)
              shadow-band-offsets (int-array [0 1 2])
              shadow-n-bits (int-array [8 8 8])
              shadow-raster (Raster/createInterleavedRaster shadow-data-buffer width height (* width channel-count) channel-count shadow-band-offsets nil)
              shadow-cs (ColorSpace/getInstance ColorSpace/CS_sRGB)
              shadow-cm (ComponentColorModel. shadow-cs shadow-n-bits false false Transparency/TRANSLUCENT DataBuffer/TYPE_BYTE)
              shadow-image (BufferedImage. shadow-cm shadow-raster false nil)
              ;; When the blur kernel is != 0, make sure to always blur the DF data set
              ;; at least once so we can avoid the jaggies around the face edges. This is mostly
              ;; prominent when the blur size is small and the offset is large.
              ^BufferedImage tmp (.getSubimage shadow-image 0 0 width height)]
          (.filter shadow-convolve tmp shadow-image)
          (doseq [^int v (range height)
                  ^int u (range width)]
            (let [df-offset (+ (* v width) u)
                  shadow-offset (+ (* df-offset channel-count) 2)
                  shadow-pixel (.getRGB shadow-image u v)]
              (aset image shadow-offset (unchecked-byte shadow-pixel))))))
      {:field image
       :width width
       :height height})))

(defn- compile-ttf-distance-field [font-desc font-resource]
  (let [font (create-ttf-font font-desc font-resource)
        antialias (protobuf/int->boolean (:antialias font-desc))
        semi-glyphs (ttf-semi-glyphs font-desc font antialias)
        ^double outline-width (:outline-width font-desc)
        ^double shadow-blur (:shadow-blur font-desc)
        ^double face-alpha (:alpha font-desc)
        ^double shadow-alpha (:shadow-alpha font-desc)
        padding (+ (int shadow-blur)
                   (int outline-width)
                   1)
        channel-count (if (and (> shadow-alpha 0.0)
                               (> face-alpha 0.0))
                        3 1)
        glyph-cell-padding 1
        edge 0.75
        sdf-spread (+ (Math/sqrt 2.0) outline-width)
        sdf-shadow-spread (+ (Math/sqrt 2.0) shadow-blur)
        sdf-outline (calculate-ttf-distance-field-edge-limit outline-width sdf-spread edge)
        sdf-shadow (if (= (int shadow-blur) 0)
                     1.0
                     (calculate-ttf-distance-field-edge-limit shadow-blur sdf-shadow-spread edge))
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        ;; Note: see comment in compile-ttf-bitmap regarding the cache ascent/descent
        ^long cache-cell-max-ascent (reduce max 0 (map :ascent semi-glyphs))
        ^long cache-cell-max-descent (reduce max 0 (map :descent semi-glyphs))
        line-height (+ cache-cell-max-ascent cache-cell-max-descent)
        cache-cell-wh (max-glyph-cell-wh glyph-extents line-height padding glyph-cell-padding)
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)
        layer-mask (font-desc->layer-mask font-desc)
        is-monospaced (check-monospaced semi-glyphs)]
    (dorun
      (pmap
        (fn [batch]
          (let [distance-field-generator (DistanceFieldGenerator.)]
            (run! (fn [[semi-glyph glyph-extents]]
                    (set! (.-lineSegmentsEnd distance-field-generator) 0)
                    (let [{:keys [^bytes field]} (draw-ttf-distance-field semi-glyph distance-field-generator padding channel-count outline-width sdf-outline sdf-spread sdf-shadow-spread edge shadow-blur shadow-alpha)
                          {:keys [image-wh glyph-cell-wh ^int glyph-data-size ^int glyph-data-offset]} glyph-extents
                          ^int image-width (:width image-wh)
                          ^int image-height (:height image-wh)
                          row-size (* (int (:width glyph-cell-wh)) channel-count)]
                      (Arrays/fill glyph-data-bank glyph-data-offset (+ glyph-data-offset row-size) (unchecked-byte 0))
                      (doseq [^int y (range image-height)]
                        (let [y-offset (+ glyph-data-offset
                                          row-size
                                          (* y row-size))]
                          (aset glyph-data-bank y-offset (unchecked-byte 0))
                          (System/arraycopy field (+ (* y image-width channel-count) 0) glyph-data-bank (+ y-offset channel-count) (* image-width channel-count))
                          (aset glyph-data-bank (+ (+ y-offset channel-count) (* image-width channel-count)) (unchecked-byte 0))))
                      (let [last-y-offset (- (+ glyph-data-offset glyph-data-size) row-size)]
                        (Arrays/fill glyph-data-bank last-y-offset (+ last-y-offset row-size) (unchecked-byte 0)))))
                  batch)))
        (sequence (comp
                    positive-glyph-extent-pairs-xf
                    (partition-all 100))
                  semi-glyphs
                  glyph-extents)))

    (merge
      (get-font-map-props font-desc)
      (get-font-metrics font)
      {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
       :image-format (:output-format font-desc)
       :layer-mask layer-mask
       :sdf-spread sdf-spread
       :sdf-shadow-spread sdf-shadow-spread
       :sdf-outline sdf-outline
       :sdf-shadow sdf-shadow
       :cache-width (:width cache-wh)
       :cache-height (:height cache-wh)
       :glyph-padding glyph-cell-padding
       :cache-cell-width (:width cache-cell-wh)
       :cache-cell-height (:height cache-cell-wh)
       :cache-cell-max-ascent (+ cache-cell-max-ascent padding)
       :glyph-channels channel-count
       :glyph-data (ByteString/copyFrom glyph-data-bank)
       :is-monospaced is-monospaced
       :padding padding})))

(defn compile-font [font-desc font-resource resolver]
  (let [font-ext (resource/type-ext font-resource)]
    (cond
      (and (= font-ext "fnt") (= (:output-format font-desc) :type-bitmap))
      (compile-fnt-bitmap font-desc font-resource resolver)

      (or (= font-ext "ttf") (= font-ext "otf"))
      (if (= (:output-format font-desc) :type-distance-field)
        (compile-ttf-distance-field font-desc font-resource)
        (compile-ttf-bitmap font-desc font-resource))

      :else
      (throw (ex-info "Unsupported font format" {:font (resource/proj-path font-resource)})))))
