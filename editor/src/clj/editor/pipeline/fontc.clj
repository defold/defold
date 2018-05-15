(ns editor.pipeline.fontc
  (:require [editor.resource :as resource]
            [clojure.java.io :as io]
            [clojure.string :as string])
  (:import [com.defold.editor.pipeline BMFont BMFont$BMFontFormatException BMFont$Char DistanceFieldGenerator]
           [com.google.protobuf ByteString]
           [javax.imageio ImageIO]
           [java.util Arrays]
           [java.awt Canvas BasicStroke Font FontMetrics Graphics2D Color RenderingHints Composite CompositeContext Shape]
           [java.awt.font FontRenderContext GlyphVector]
           [java.awt.geom AffineTransform PathIterator FlatteningPathIterator]
           [java.awt.image BufferedImage Kernel ConvolveOp Raster WritableRaster]
           [java.io InputStream ByteArrayOutputStream FileNotFoundException IOException]
           [java.nio.file Paths]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- next-pow2 [n]
  (assert (>= ^int n 0))
  (int (Math/pow 2 (Math/ceil (/ (Math/log (max ^int n 1.0)) (Math/log 2))))))

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
    {:width cache-width
     :height cache-height}))

(defn- glyph-wh [{^int glyph-width :width
                  ^int glyph-ascent :ascent
                  ^int glyph-descent :descent}]
  {:width glyph-width
   :height (+ glyph-ascent glyph-descent)})

(defn- pad-wh [padding {^int width :width ^int height :height}]
  {:width (+ width (* ^int padding 2))
   :height (+ height (* ^int padding 2))})

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

(defn- make-ddf-glyphs [semi-glyphs glyph-extents padding]
  (map
    (fn [glyph glyph-extents]
      (let [wh (:glyph-wh glyph-extents)]
        {:character (:character glyph)
         :width (if (positive-wh? wh)
                  (:width (:image-wh glyph-extents))
                  (:width wh))
         :advance (:advance glyph)
         :left-bearing (- ^double (:left-bearing glyph) ^int padding)
         :ascent (+ ^int (:ascent glyph) ^int padding)
         :descent (+ ^int (:descent glyph) ^int padding)
         :glyph-data-offset (:glyph-data-offset glyph-extents)
         :glyph-data-size (:glyph-data-size glyph-extents)}))
    semi-glyphs glyph-extents))

(defn- max-glyph-cell-wh [glyph-extents]
  (let [wh (transduce (filter (comp positive-wh? :glyph-wh))
                      (completing
                        (fn [max-wh {:keys [glyph-cell-wh]}]
                          (-> max-wh
                              (update :width max (:width glyph-cell-wh))
                              (update :height max (:height glyph-cell-wh)))))
                      {:width 0 :height 0}
                      glyph-extents)]
    (if (or (= (:width wh) 0) (= (:height wh) 0))
      (throw (ex-info "No glyph with sane width/height"))
      wh)))

(defn- draw-bm-font-glyph ^BufferedImage [glyph ^BufferedImage bm-image]
  (.getSubimage bm-image
                ^int (:x glyph)
                ^int (:y glyph)
                ^int (:width glyph)
                (+ ^int (:ascent glyph) ^int (:descent glyph))))

(defn- int->boolean [n]
  (assert (some? n))
  (not= n 0))

(defn- pair [a b]
  (clojure.lang.MapEntry. a b))

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
                         }))]
    (when-not (seq semi-glyphs)
      (throw (ex-info "No character glyphs were included! Maybe turn on 'all_chars'?" {})))
    semi-glyphs))

(defn- make-glyph-data-bank ^bytes [glyph-extents]
  (byte-array (transduce (map :glyph-data-size) + 0 glyph-extents)))

(def ^:private positive-glyph-extent-pairs-xf (comp (map pair) (filter (comp positive-wh? :glyph-wh second))))

(defn- compile-fnt-bitmap [font-desc font-resource resolver]
  (let [^BMFont bm-font (with-open [font-stream (io/input-stream font-resource)]
                          (doto (BMFont.)
                            (.parse font-stream)))
        semi-glyphs (fnt-semi-glyphs bm-font)
        ^int shadow-blur (:shadow-blur font-desc)
        padding 0
        glyph-cell-padding 1
        bm-image (let [path (.. (Paths/get (FilenameUtils/normalize (.. bm-font page (get 0))) (into-array String [])) getFileName toString)]
                   (try
                     (with-open [image-stream ^InputStream (resolver path)]
                       (ImageIO/read image-stream))
                     (catch FileNotFoundException e
                       (throw (ex-info (str "Could not find BMFont image resource: " path) {:path path} e)))
                     (catch IOException e
                       (throw (ex-info (str "Error while reading BMFont image resource" {:path path} e))))))
        channel-count 4
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        cache-cell-wh (max-glyph-cell-wh glyph-extents)
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)]

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
    
    {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
     :material (str (:material font-desc) "c")
     :max-ascent (float (reduce max 0 (map :ascent semi-glyphs)))
     :max-descent (float (reduce max 0 (map :descent semi-glyphs)))
     :image-format (:output-format font-desc)
     :cache-width (:width cache-wh)
     :cache-height (:height cache-wh)
     :glyph-padding glyph-cell-padding
     :cache-cell-width (:width cache-cell-wh)
     :cache-cell-height (:height cache-cell-wh)
     :glyph-channels channel-count
     :glyph-data (ByteString/copyFrom glyph-data-bank)}))

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
        antialias (int->boolean (:antialias font-desc))
        image (BufferedImage. width height BufferedImage/TYPE_3BYTE_BGR)
        g (doto (.createGraphics image)
            (set-high-quality antialias)
            (.setBackground Color/BLACK)
            (.clearRect 0 0 (.getWidth image) (.getHeight image))
            (.translate dx dy))]
    (if antialias
      (let [^Shape outline (.getOutline glyph-vector 0 0)]
        (when (> font-desc-shadow-alpha 0.0)
          (when (> font-desc-alpha 0.0)
            (.setPaint g (Color. 0.0 0.0 (* font-desc-shadow-alpha font-desc-alpha)))
            (.fill g outline))
          (when (and (> font-desc-outline-width 0.0)
                     (> font-desc-outline-alpha 0.0))
            (let [outline-stroke (BasicStroke. font-desc-outline-width)]
              (.setPaint g (Color. 0.0 0.0 (* font-desc-shadow-alpha font-desc-outline-alpha)))
              (.setStroke g outline-stroke)
              (.draw g outline)))
          (dotimes [n (:shadow-blur font-desc)]
            (let [tmp (.getSubimage image 0 0 width height)]
              (.filter shadow-convolve tmp image))))
        (.setComposite g blend-composite)
        (when (and (> font-desc-outline-width 0.0)
                   (> font-desc-outline-alpha 0.0))
          (let [outline-stroke (BasicStroke. font-desc-outline-width)]
            (.setPaint g outline-color)
            (.setStroke g outline-stroke)
            (.draw g outline)))
        (when (> font-desc-alpha 0.0)
          (.setPaint g face-color)
          (.fill g outline)))
      (do
        (.setPaint g face-color)
        (.setFont g font)
        (.drawString g (String. (Character/toChars glyph-character)) 0 0)))
    image))


(def ^:private ^AffineTransform identity-transform (AffineTransform.))
(def ^:private plain-font-render-context (FontRenderContext. identity-transform (boolean false) (boolean false)))
(def ^:private antialiased-font-render-context (FontRenderContext. identity-transform (boolean true) (boolean true)))

(defn- font-render-context ^FontRenderContext [antialias]
  (if antialias
    antialiased-font-render-context
    plain-font-render-context))

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

(defn- create-ttf-font ^Font [font-desc font-resource]
  (with-open [font-stream (io/input-stream font-resource)]
    (-> (Font/createFont Font/TRUETYPE_FONT font-stream)
        (.deriveFont Font/PLAIN (float (:size font-desc))))))

(defn- ttf-semi-glyph [^Font font antialias codepoint]
  (let [glyph-vector (.createGlyphVector font (font-render-context antialias) (Character/toChars codepoint))
        visual-bounds (.. glyph-vector getOutline getBounds)
        metrics (.getGlyphMetrics glyph-vector 0)
        left-bearing (double (.getLSB metrics))]
    {:ascent (int (Math/ceil (- (.getMinY visual-bounds))))
     :descent (int (Math/ceil (.getMaxY visual-bounds)))
     :character codepoint
     :advance (double (Math/round (.getAdvance metrics)))
     :left-bearing (Math/floor left-bearing)
     :width (+ (.getWidth visual-bounds) (if (not= left-bearing 0.0) 1 0))
     :vector glyph-vector}))

(defn- ttf-semi-glyphs [font-desc ^Font font antialias]
  (let [prospect-codepoints (if (:all-chars font-desc)
                              (range 0x10ffff)
                              (concat (range 32 127) (map int (:extra-characters font-desc))))
        displayable-codepoint? (fn [codepoint] (.canDisplay font ^int codepoint))
        semi-glyphs (into []
                          (comp
                            (filter displayable-codepoint?)
                            (map (partial ttf-semi-glyph font antialias)))
                          prospect-codepoints)]
    (when-not (seq semi-glyphs)
      (throw (ex-info "No character glyphs were included! Maybe turn on 'all_chars'?" {})))
    semi-glyphs))

(defn- compile-ttf-bitmap [font-desc font-resource]
  (let [font (create-ttf-font font-desc font-resource)
        antialias (int->boolean (:antialias font-desc))
        semi-glyphs (ttf-semi-glyphs font-desc font antialias)
        font-metrics (font-metrics font)
        padding (if antialias
                  (+ (min (int 4) ^int (:shadow-blur font-desc))
                     (int (Math/ceil (* ^double (:outline-width font-desc) 0.5))))
                  0)
        glyph-cell-padding 1
        channel-count (if (and (> ^double (:outline-width font-desc) 0.0)
                               (> ^double (:outline-alpha font-desc) 0.0))
                        3 1)
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        ;; BOB: "Some hardware don't like doing subimage updates on non-aligned cell positions."
        cache-cell-wh (cond-> (max-glyph-cell-wh glyph-extents)
                        (= channel-count 3)
                        (update :width #(* (int (Math/ceil (/ ^double % 4.0))) 4)))
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)]
    
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

    {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
     :material (str (:material font-desc) "c")
     :shadow-x (:shadow-x font-desc)
     :shadow-y (:shadow-y font-desc)
     :max-ascent (float (.getMaxAscent font-metrics))
     :max-descent (float (.getMaxDescent font-metrics))
     :image-format (:output-format font-desc)
     :cache-width (:width cache-wh)
     :cache-height (:height cache-wh)
     :glyph-padding glyph-cell-padding
     :cache-cell-width (:width cache-cell-wh)
     :cache-cell-height (:height cache-cell-wh)
     :glyph-channels channel-count
     :glyph-data (ByteString/copyFrom glyph-data-bank)}))

(defn- draw-ttf-distance-field [{^int glyph-ascent :ascent
                                 ^double glyph-left-bearing :left-bearing
                                 ^GlyphVector glyph-vector :vector
                                 :as glyph}
                                padding
                                sdf-spread
                                edge]
  (let [^int padding padding
        ^double sdf-spread sdf-spread
        ^double edge edge
        {^int width :width ^int height :height} (pad-wh padding (glyph-wh glyph))
        ^Shape glyph-outline (.getGlyphOutline glyph-vector 0)
        ^PathIterator outline-iterator (FlatteningPathIterator. (.getPathIterator glyph-outline identity-transform) 0.1)
        ^DistanceFieldGenerator distance-field-generator (DistanceFieldGenerator.)
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
          image (byte-array (* width height))]
      (.render distance-field-generator res u0 v0 (+ u0 width) (+ v0 height) width height)
      (doseq [^int v (range height)
              ^int u (range width)]
        (let [gx (+ u0 u)
              gy (+ v0 v)
              ofs (+ (* v width) u)
              value' (aget res ofs)
              value (if-not (.contains glyph-outline gx gy) (- value') value')
              df-norm (+ (* (/ value sdf-spread) (- 1.0 edge)) edge)
              oval' (int (* 255.0 df-norm))
              oval (max 0 (min oval' 255))]
          (aset image ofs (unchecked-byte oval))))
      {:field image
       :width width
       :height height})))

(defn- compile-ttf-distance-field [font-desc font-resource]
  (let [font (create-ttf-font font-desc font-resource)
        antialias (int->boolean (:antialias font-desc))
        semi-glyphs (ttf-semi-glyphs font-desc font antialias)
        font-metrics (font-metrics font)
        padding (+ (int (if antialias
                          (Math/ceil (* ^double (:outline-width font-desc) 0.5))
                          0))
                   1)
        channel-count 1
        glyph-cell-padding 1
        sdf-spread (+ (Math/sqrt 2.0) ^double (:outline-width font-desc))
        edge 0.75
        glyph-extents (make-glyph-extents channel-count padding glyph-cell-padding semi-glyphs)
        cache-cell-wh (max-glyph-cell-wh glyph-extents)
        cache-wh (cache-wh font-desc cache-cell-wh (count semi-glyphs))
        glyph-data-bank (make-glyph-data-bank glyph-extents)]

    (doall
      (pmap (fn [[semi-glyph glyph-extents]]
              (let [{:keys [^bytes field]} (draw-ttf-distance-field semi-glyph padding sdf-spread edge)
                    {:keys [image-wh glyph-cell-wh ^int glyph-data-size ^int glyph-data-offset]} glyph-extents
                    ^int image-width (:width image-wh)
                    ^int image-height (:height image-wh)
                    ^int row-size (:width glyph-cell-wh)]
                (Arrays/fill glyph-data-bank glyph-data-offset (+ glyph-data-offset row-size) (unchecked-byte 0))
                (doseq [^int y (range image-height)]
                  (let [y-offset (+ glyph-data-offset
                                    row-size
                                    (* y row-size))]
                    (aset glyph-data-bank y-offset (unchecked-byte 0))
                    (System/arraycopy field (+ (* y image-width) 0) glyph-data-bank (inc y-offset) image-width)
                    (aset glyph-data-bank (+ (inc y-offset) image-width) (unchecked-byte 0))))
                (let [last-y-offset (- (+ glyph-data-offset glyph-data-size) row-size)]
                  (Arrays/fill glyph-data-bank last-y-offset (+ last-y-offset row-size) (unchecked-byte 0)))))
            (sequence positive-glyph-extent-pairs-xf
                      semi-glyphs
                      glyph-extents)))

    {:glyphs (make-ddf-glyphs semi-glyphs glyph-extents padding)
     :material (str (:material font-desc) "c")
     :shadow-x (:shadow-x font-desc)
     :shadow-y (:shadow-y font-desc)
     :max-ascent (float (.getMaxAscent font-metrics))
     :max-descent (float (.getMaxDescent font-metrics))
     :image-format (:output-format font-desc)
     :sdf-spread sdf-spread
     :sdf-outline (let [outline-edge (- (/ ^double (:outline-width font-desc) sdf-spread))]
                    (+ (* outline-edge (- 1.0 edge)) edge))
     :cache-width (:width cache-wh)
     :cache-height (:height cache-wh)
     :glyph-padding glyph-cell-padding
     :cache-cell-width (:width cache-cell-wh)
     :cache-cell-height (:height cache-cell-wh)
     :glyph-channels channel-count
     :glyph-data (ByteString/copyFrom glyph-data-bank)
     :alpha (:alpha font-desc)
     :outline-alpha (:outline-alpha font-desc)
     :shadow-alpha (:shadow-alpha font-desc)}))

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
