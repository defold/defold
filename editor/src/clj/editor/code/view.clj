(ns editor.code.view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :refer [pair split-lines]]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices-popup :as popup]
            [editor.util :as eutil]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [schema.core :as s])
  (:import (clojure.lang IPersistentVector)
           (com.defold.control ListView)
           (com.sun.javafx.font FontResource FontStrike PGFont)
           (com.sun.javafx.geom.transform BaseTransform)
           (com.sun.javafx.perf PerformanceTracker)
           (com.sun.javafx.tk Toolkit)
           (com.sun.javafx.util Utils)
           (editor.code.data Cursor CursorRange GestureInfo LayoutInfo Rect)
           (java.util Collection)
           (java.util.regex Pattern)
           (javafx.beans.binding Bindings StringBinding)
           (javafx.beans.property Property SimpleBooleanProperty SimpleDoubleProperty SimpleStringProperty)
           (javafx.beans.value ChangeListener)
           (javafx.geometry HPos Point2D VPos)
           (javafx.scene Node Parent Scene)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.control Button CheckBox PopupControl Tab TextField)
           (javafx.scene.input Clipboard DataFormat KeyCode KeyEvent MouseButton MouseDragEvent MouseEvent ScrollEvent)
           (javafx.scene.layout ColumnConstraints GridPane Pane Priority)
           (javafx.scene.paint Color LinearGradient Paint)
           (javafx.scene.shape Rectangle)
           (javafx.scene.text Font FontSmoothingType TextAlignment)
           (javafx.stage Stage)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private default-font-size 12.0)

(defprotocol GutterView
  (gutter-metrics [this lines regions glyph-metrics] "A two-element vector with a rounded double representing the width of the gutter and another representing the margin on each side within the gutter.")
  (draw-gutter! [this gc gutter-rect layout font color-scheme lines regions visible-cursors] "Draws the gutter into the specified Rect."))

(defrecord CursorRangeDrawInfo [type fill stroke cursor-range])

(defn- cursor-range-draw-info [type fill stroke cursor-range]
  (assert (case type (:range :underline :word) true false))
  (assert (or (nil? fill) (instance? Paint fill)))
  (assert (or (nil? stroke) (instance? Paint stroke)))
  (assert (instance? CursorRange cursor-range))
  (->CursorRangeDrawInfo type fill stroke cursor-range))

(def ^:private *performance-tracker (atom nil))
(def ^:private undo-groupings #{:navigation :newline :selection :typing})
(g/deftype ColorScheme [[(s/one s/Str "pattern") (s/one Paint "paint")]])
(g/deftype CursorRangeDrawInfos [CursorRangeDrawInfo])
(g/deftype FocusState (s/->EnumSchema #{:not-focused :semi-focused :input-focused}))
(g/deftype GutterMetrics [(s/one s/Num "gutter-width") (s/one s/Num "gutter-margin")])
(g/deftype HoveredElement {:type s/Keyword s/Keyword s/Any})
(g/deftype MatchingBraces [[(s/one r/TCursorRange "brace") (s/one r/TCursorRange "counterpart")]])
(g/deftype UndoGroupingInfo [(s/one (s/->EnumSchema undo-groupings) "undo-grouping") (s/one s/Symbol "opseq")])
(g/deftype ResizeReference (s/enum :bottom :top))
(g/deftype SyntaxInfo IPersistentVector)
(g/deftype SideEffect (s/eq nil))

(defn- enable-performance-tracker! [scene]
  (reset! *performance-tracker (PerformanceTracker/getSceneTracker scene)))

(defn- mime-type->DataFormat
  ^DataFormat [^String mime-type]
  (or (DataFormat/lookupMimeType mime-type)
      (DataFormat. (into-array String [mime-type]))))

(extend-type Clipboard
  data/Clipboard
  (has-content? [this mime-type] (.hasContent this (mime-type->DataFormat mime-type)))
  (get-content [this mime-type] (.getContent this (mime-type->DataFormat mime-type)))
  (set-content! [this representation-by-mime-type]
    (.setContent this (into {}
                            (map (fn [[mime-type representation]]
                                   [(mime-type->DataFormat mime-type) representation]))
                            representation-by-mime-type))))

(defrecord GlyphMetrics [^FontStrike font-strike ^double line-height ^double ascent]
  data/GlyphMetrics
  (ascent [_this] ascent)
  (line-height [_this] line-height)
  (char-width [_this character] (Math/floor (.getCharAdvance font-strike character))))

(defn make-glyph-metrics
  ^GlyphMetrics [^Font font ^double line-height-factor]
  (let [font-loader (.getFontLoader (Toolkit/getToolkit))
        font-metrics (.getFontMetrics font-loader font)
        font-strike (.getStrike ^PGFont (.impl_getNativeFont font) BaseTransform/IDENTITY_TRANSFORM FontResource/AA_GREYSCALE)
        line-height (Math/ceil (* (inc (.getLineHeight font-metrics)) line-height-factor))
        ascent (Math/ceil (* (.getAscent font-metrics) line-height-factor))]
    (->GlyphMetrics font-strike line-height ascent)))

(def ^:private default-editor-color-scheme
  (let [^Color foreground-color (Color/valueOf "#DDDDDD")
        ^Color background-color (Color/valueOf "#27292D")
        ^Color selection-background-color (Color/valueOf "#4E4A46")
        ^Color execution-marker-color (Color/valueOf "#FBCE2F")
        ^Color execution-marker-frame-color (.deriveColor execution-marker-color 0.0 1.0 1.0 0.5)]
    [["editor.foreground" foreground-color]
     ["editor.background" background-color]
     ["editor.cursor" Color/WHITE]
     ["editor.selection.background" selection-background-color]
     ["editor.selection.background.inactive" (.deriveColor selection-background-color 0.0 0.0 0.75 1.0)]
     ["editor.selection.occurrence.outline" (Color/valueOf "#A2B0BE")]
     ["editor.tab.trigger.word.outline" (Color/valueOf "#A2B0BE")]
     ["editor.find.term.occurrence" (Color/valueOf "#60C1FF")]
     ["editor.execution-marker.current" execution-marker-color]
     ["editor.execution-marker.frame" execution-marker-frame-color]
     ["editor.gutter.foreground" (Color/valueOf "#A2B0BE")]
     ["editor.gutter.background" background-color]
     ["editor.gutter.cursor.line.background" (Color/valueOf "#393C41")]
     ["editor.gutter.breakpoint" (Color/valueOf "#AD4051")]
     ["editor.gutter.execution-marker.current" execution-marker-color]
     ["editor.gutter.execution-marker.frame" execution-marker-frame-color]
     ["editor.gutter.shadow" (LinearGradient/valueOf "to right, rgba(0, 0, 0, 0.3) 0%, transparent 100%")]
     ["editor.matching.brace" (Color/valueOf "#A2B0BE")]
     ["editor.minimap.shadow" (LinearGradient/valueOf "to left, rgba(0, 0, 0, 0.2) 0%, transparent 100%")]
     ["editor.minimap.viewed.range" (Color/valueOf "#393C41")]
     ["editor.scroll.tab" (.deriveColor foreground-color 0.0 1.0 1.0 0.15)]
     ["editor.scroll.tab.hovered" (.deriveColor foreground-color 0.0 1.0 1.0 0.5)]
     ["editor.whitespace.space" (.deriveColor foreground-color 0.0 1.0 1.0 0.2)]
     ["editor.whitespace.tab" (.deriveColor foreground-color 0.0 1.0 1.0 0.1)]
     ["editor.whitespace.rogue" (Color/valueOf "#FBCE2F")]
     ["editor.indentation.guide" (.deriveColor foreground-color 0.0 1.0 1.0 0.1)]]))

(defn make-color-scheme [ordered-paints-by-pattern]
  (into []
        (util/distinct-by first)
        (concat ordered-paints-by-pattern
                default-editor-color-scheme)))

(def ^:private code-color-scheme
  (make-color-scheme
    [["comment" (Color/valueOf "#B0B0B0")]
     ["string" (Color/valueOf "#FBCE2F")]
     ["numeric" (Color/valueOf "#AAAAFF")]
     ["preprocessor" (Color/valueOf "#E3A869")]
     ["punctuation" (Color/valueOf "#FD6623")]
     ["keyword" (Color/valueOf "#FD6623")]
     ["storage" (Color/valueOf "#FD6623")]
     ["constant" (Color/valueOf "#FFBBFF")]
     ["support.function" (Color/valueOf "#33CCCC")]
     ["support.variable" (Color/valueOf "#FFBBFF")]
     ["name.function" (Color/valueOf "#33CC33")]
     ["parameter.function" (Color/valueOf "#E3A869")]
     ["variable.language" (Color/valueOf "#E066FF")]]))

(defn color-lookup
  ^Paint [color-scheme key]
  (or (some (fn [[pattern paint]]
              (when (= key pattern)
                paint))
            color-scheme)
      (throw (ex-info (str "Missing color scheme key " key)
                      {:key key
                       :keys (mapv first color-scheme)}))))

(defn color-match
  ^Paint [color-scheme scope]
  (or (some (fn [[pattern paint]]
              (when (string/includes? scope pattern)
                paint))
            (take-while (fn [[pattern]]
                          (not (string/starts-with? pattern "editor.")))
                        color-scheme))
      (color-lookup color-scheme "editor.foreground")))

(defn- rect-outline [^Rect r]
  [[(.x r) (+ (.x r) (.w r)) (+ (.x r) (.w r)) (.x r) (.x r)]
   [(.y r) (.y r) (+ (.y r) (.h r)) (+ (.y r) (.h r)) (.y r)]])

(defn- cursor-range-outline [rects]
  (let [^Rect a (first rects)
        ^Rect b (second rects)
        ^Rect y (peek (pop rects))
        ^Rect z (peek rects)]
    (cond
      (nil? b)
      [(rect-outline a)]

      (and (identical? b z) (< (+ (.x b) (.w b)) (.x a)))
      [(rect-outline a)
       (rect-outline b)]

      :else
      [[[(.x b) (.x a) (.x a) (+ (.x a) (.w a)) (+ (.x a) (.w a)) (+ (.x z) (.w z)) (+ (.x z) (.w z)) (.x z) (.x b)]
        [(.y b) (.y b) (.y a) (.y a) (+ (.y y) (.h y)) (+ (.y y) (.h y)) (+ (.y z) (.h z)) (+ (.y z) (.h z)) (.y b)]]])))

(defn- fill-cursor-range! [^GraphicsContext gc type ^Paint fill ^Paint _stroke rects]
  (when (some? fill)
    (.setFill gc fill)
    (case type
      :word (let [^Rect r (data/expand-rect (first rects) 1.0 0.0)]
              (assert (= 1 (count rects)))
              (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0))
      :range (doseq [^Rect r rects]
               (.fillRect gc (.x r) (.y r) (.w r) (.h r)))
      :underline nil)))

(defn- stroke-opaque-polyline! [^GraphicsContext gc xs ys]
  ;; The strokePolyLine method slows down considerably when the shape covers a large
  ;; area of the screen. Drawing individual lines is a lot quicker, but since pixels
  ;; at line ends will be covered twice the stroke must be opaque.
  (loop [^double sx (first xs)
         ^double sy (first ys)
         xs (next xs)
         ys (next ys)]
    (when (and (seq xs) (seq ys))
      (let [^double ex (first xs)
            ^double ey (first ys)]
        (.strokeLine gc sx sy ex ey)
        (recur ex ey (next xs) (next ys))))))

(defn- stroke-cursor-range! [^GraphicsContext gc type ^Paint _fill ^Paint stroke rects]
  (when (some? stroke)
    (.setStroke gc stroke)
    (.setLineWidth gc 1.0)
    (case type
      :word (let [^Rect r (data/expand-rect (first rects) 1.5 0.5)]
              (assert (= 1 (count rects)))
              (.strokeRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0))
      :range (doseq [polyline (cursor-range-outline rects)]
               (let [[xs ys] polyline]
                 (stroke-opaque-polyline! gc (double-array xs) (double-array ys))))
      :underline (doseq [^Rect r rects]
                   (let [sx (.x r)
                         ex (+ sx (.w r))
                         y (+ (.y r) (.h r))]
                     (.strokeLine gc sx y ex y))))))

(defn- draw-cursor-ranges! [^GraphicsContext gc layout lines cursor-range-draw-infos]
  (let [draw-calls (mapv (fn [^CursorRangeDrawInfo draw-info]
                           [gc
                            (:type draw-info)
                            (:fill draw-info)
                            (:stroke draw-info)
                            (data/cursor-range-rects layout lines (:cursor-range draw-info))])
                         cursor-range-draw-infos)]
    (doseq [args draw-calls]
      (apply fill-cursor-range! args))
    (doseq [args draw-calls]
      (apply stroke-cursor-range! args))))

(defn- leading-whitespace-length
  ^long [^String line]
  (count (take-while #(Character/isWhitespace ^char %) line)))

(defn- find-prior-unindented-row
  ^long [lines ^long row]
  (if (or (zero? row)
          (>= row (count lines)))
    0
    (let [line (lines row)
          leading-whitespace-length (leading-whitespace-length line)
          line-has-text? (some? (get line leading-whitespace-length))]
      (if (and line-has-text? (zero? leading-whitespace-length))
        row
        (recur lines (dec row))))))

(defn- find-prior-indentation-guide-positions [^LayoutInfo layout lines]
  (let [dropped-line-count (.dropped-line-count layout)]
    (loop [row (find-prior-unindented-row lines dropped-line-count)
           guide-positions []]
      (let [line (get lines row)]
        (if (or (nil? line) (<= dropped-line-count row))
          guide-positions
          (let [leading-whitespace-length (leading-whitespace-length line)
                line-has-text? (some? (get line leading-whitespace-length))]
            (if-not line-has-text?
              (recur (inc row) guide-positions)
              (let [guide-x (data/col->x layout leading-whitespace-length line)
                    guide-positions' (conj (into [] (take-while #(< ^double % guide-x)) guide-positions) guide-x)]
                (recur (inc row) guide-positions')))))))))

(defn- fill-text!
  "Draws text onto the canvas. In order to support tab stops, we remap the supplied x
  coordinate into document space, then remap back to canvas coordinates when drawing.
  Returns the canvas x coordinate where the drawn string ends, or nil if drawing
  stopped because we reached the end of the visible canvas region."
  [^GraphicsContext gc ^LayoutInfo layout ^String text start-index end-index x y]
  (let [^Rect canvas-rect (.canvas layout)
        visible-start-x (.x canvas-rect)
        visible-end-x (+ visible-start-x (.w canvas-rect))
        offset-x (+ visible-start-x (.scroll-x layout))]
    (loop [^long i start-index
           x (- ^double x offset-x)]
      (if (= ^long end-index i)
        (+ x offset-x)
        (let [glyph (.charAt text i)
              next-i (inc i)
              next-x (double (data/advance-text layout text i next-i x))
              draw-start-x (+ x offset-x)
              draw-end-x (+ next-x offset-x)
              inside-visible-start? (< visible-start-x draw-end-x)
              inside-visible-end? (< draw-start-x visible-end-x)]
          ;; Currently using FontSmoothingType/GRAY results in poor kerning when
          ;; drawing subsequent characters in a string given to fillText. Here
          ;; glyphs are drawn individually at whole pixels as a workaround.
          (when (and inside-visible-start?
                     inside-visible-end?
                     (not (Character/isWhitespace glyph)))
            (.fillText gc (String/valueOf glyph) draw-start-x y))
          (when inside-visible-end?
            (recur next-i next-x)))))))

(defn- draw-code! [^GraphicsContext gc ^Font font ^LayoutInfo layout color-scheme lines syntax-info indent-type visible-whitespace?]
  (let [^Rect canvas-rect (.canvas layout)
        source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double ascent (data/ascent (.glyph layout))
        ^double line-height (data/line-height (.glyph layout))
        foreground-color (color-lookup color-scheme "editor.foreground")]
    (.setFont gc font)
    (.setTextAlign gc TextAlignment/LEFT)
    (loop [drawn-line-index 0
           source-line-index dropped-line-count]
      (when (and (< drawn-line-index drawn-line-count)
                 (< source-line-index source-line-count))
        (let [^String line (lines source-line-index)
              line-x (+ (.x canvas-rect)
                        (.scroll-x layout))
              line-y (+ ascent
                        (.scroll-y-remainder layout)
                        (* drawn-line-index line-height))]
          (if-some [runs (second (get syntax-info source-line-index))]
            ;; Draw syntax-highlighted runs.
            (loop [run-index 0
                   glyph-offset line-x]
              (when-some [[start scope] (get runs run-index)]
                (.setFill gc (color-match color-scheme scope))
                (let [end (or (first (get runs (inc run-index)))
                              (count line))
                      glyph-offset (fill-text! gc layout line start end glyph-offset line-y)]
                  (when (some? glyph-offset)
                    (recur (inc run-index) (double glyph-offset))))))

            ;; Just draw line as plain text.
            (when-not (string/blank? line)
              (.setFill gc foreground-color)
              (fill-text! gc layout line 0 (count line) line-x line-y)))

          (let [line-length (count line)
                baseline-offset (Math/ceil (/ line-height 4.0))
                visible-start-x (.x canvas-rect)
                visible-end-x (+ visible-start-x (.w canvas-rect))
                rogue-indent-color (color-lookup color-scheme "editor.whitespace.rogue")
                space-color (color-lookup color-scheme "editor.whitespace.space")
                tab-color (color-lookup color-scheme "editor.whitespace.tab")]
            (loop [inside-leading-whitespace? true
                   i 0
                   x 0.0]
              (when (< i line-length)
                (let [character (.charAt line i)
                      next-i (inc i)
                      next-x (double (data/advance-text layout line i next-i x))
                      draw-start-x (+ x line-x)
                      draw-end-x (+ next-x line-x)
                      inside-visible-start? (< visible-start-x draw-end-x)
                      inside-visible-end? (< draw-start-x visible-end-x)]
                  (when (and inside-visible-start? inside-visible-end?)
                    (case character
                      \space (let [sx (+ line-x (Math/floor (* (+ x next-x) 0.5)))
                                   sy (- line-y baseline-offset)]
                               (cond
                                 (and inside-leading-whitespace? (= :tabs indent-type))
                                 (do (.setFill gc rogue-indent-color)
                                     (.fillRect gc sx sy 1.0 1.0))

                                 visible-whitespace?
                                 (do (.setFill gc space-color)
                                     (.fillRect gc sx sy 1.0 1.0))))

                      \tab (let [sx (+ line-x x 2.0)
                                 sy (- line-y baseline-offset)]
                             (cond
                               (and inside-leading-whitespace? (not= :tabs indent-type))
                               (do (.setFill gc rogue-indent-color)
                                   (.fillRect gc sx sy (- next-x x 4.0) 1.0))

                               visible-whitespace?
                               (do (.setFill gc tab-color)
                                   (.fillRect gc sx sy (- next-x x 4.0) 1.0))))
                      nil))
                  (when inside-visible-end?
                    (recur (and inside-leading-whitespace? (Character/isWhitespace character)) next-i next-x))))))
          (recur (inc drawn-line-index)
                 (inc source-line-index)))))))

(defn- fill-minimap-run!
  [^GraphicsContext gc ^Color color ^LayoutInfo minimap-layout ^String text start-index end-index x y]
  (let [^Rect minimap-rect (.canvas minimap-layout)
        visible-start-x (.x minimap-rect)
        visible-end-x (+ visible-start-x (.w minimap-rect))]
    (.setFill gc (.deriveColor color 0.0 1.0 1.0 0.5))
    (loop [^long i start-index
           x (- ^double x visible-start-x)]
      (if (= ^long end-index i)
        (+ x visible-start-x)
        (let [glyph (.charAt text i)
              next-i (inc i)
              next-x (double (data/advance-text minimap-layout text i next-i x))
              draw-start-x (+ x visible-start-x)
              draw-end-x (+ next-x visible-start-x)
              inside-visible-start? (< visible-start-x draw-end-x)
              inside-visible-end? (< draw-start-x visible-end-x)]
          (when (and inside-visible-start?
                     inside-visible-end?
                     (not= \_ glyph)
                     (not (Character/isWhitespace glyph)))
            (.fillRect gc draw-start-x y (- draw-end-x draw-start-x) 1.0))
          (when inside-visible-end?
            (recur next-i next-x)))))))

(defn- draw-minimap-code! [^GraphicsContext gc ^LayoutInfo minimap-layout color-scheme lines syntax-info]
  (let [^Rect minimap-rect (.canvas minimap-layout)
        source-line-count (count lines)
        dropped-line-count (.dropped-line-count minimap-layout)
        drawn-line-count (.drawn-line-count minimap-layout)
        ^double ascent (data/ascent (.glyph minimap-layout))
        ^double line-height (data/line-height (.glyph minimap-layout))
        foreground-color (color-lookup color-scheme "editor.foreground")]
    (loop [drawn-line-index 0
           source-line-index dropped-line-count]
      (when (and (< drawn-line-index drawn-line-count)
                 (< source-line-index source-line-count))
        (let [^String line (lines source-line-index)
              line-x (.x minimap-rect)
              line-y (+ ascent
                        (.scroll-y-remainder minimap-layout)
                        (* drawn-line-index line-height))]
          (if-some [runs (second (get syntax-info source-line-index))]
            ;; Draw syntax-highlighted runs.
            (loop [run-index 0
                   glyph-offset line-x]
              (when-some [[start scope] (get runs run-index)]
                (let [end (or (first (get runs (inc run-index)))
                              (count line))
                      glyph-offset (fill-minimap-run! gc (color-match color-scheme scope) minimap-layout line start end glyph-offset line-y)]
                  (when (some? glyph-offset)
                    (recur (inc run-index) (double glyph-offset))))))

            ;; Just draw line as plain text.
            (when-not (string/blank? line)
              (fill-minimap-run! gc foreground-color minimap-layout line 0 (count line) line-x line-y)))

          (recur (inc drawn-line-index)
                 (inc source-line-index)))))))

(defn- draw! [^GraphicsContext gc ^Font font gutter-view hovered-element ^LayoutInfo layout ^LayoutInfo minimap-layout color-scheme lines regions syntax-info cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-whitespace? visible-minimap?]
  (let [^Rect canvas-rect (.canvas layout)
        source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double line-height (data/line-height (.glyph layout))
        background-color (color-lookup color-scheme "editor.background")
        scroll-tab-color (color-lookup color-scheme "editor.scroll.tab")
        scroll-tab-hovered-color (color-lookup color-scheme "editor.scroll.tab.hovered")]
    (.setFill gc background-color)
    (.fillRect gc 0 0 (.. gc getCanvas getWidth) (.. gc getCanvas getHeight))
    (.setFontSmoothingType gc FontSmoothingType/GRAY) ; FontSmoothingType/LCD is very slow.

    ;; Draw cursor ranges.
    (draw-cursor-ranges! gc layout lines cursor-range-draw-infos)

    ;; Draw indentation guides.
    (when visible-indentation-guides?
      (.setStroke gc (color-lookup color-scheme "editor.indentation.guide"))
      (.setLineWidth gc 1.0)
      (loop [drawn-line-index 0
             source-line-index dropped-line-count
             guide-positions (find-prior-indentation-guide-positions layout lines)]
        (when (and (< drawn-line-index drawn-line-count)
                   (< source-line-index source-line-count))
          (let [line (lines source-line-index)
                leading-whitespace-length (count (take-while #(Character/isWhitespace ^char %) line))
                line-has-text? (some? (get line leading-whitespace-length))
                guide-x (data/col->x layout leading-whitespace-length line)
                line-y (data/row->y layout source-line-index)
                guide-positions (if line-has-text? (into [] (take-while #(< ^double % guide-x)) guide-positions) guide-positions)]
            (when (get syntax-info source-line-index)
              (doseq [guide-x guide-positions]
                (.strokeLine gc guide-x line-y guide-x (+ line-y (dec line-height)))))
            (recur (inc drawn-line-index)
                   (inc source-line-index)
                   (if line-has-text? (conj guide-positions guide-x) guide-positions))))))

    ;; Draw syntax-highlighted code.
    (draw-code! gc font layout color-scheme lines syntax-info indent-type visible-whitespace?)

    ;; Draw minimap.
    (when visible-minimap?
      (let [^Rect r (.canvas minimap-layout)
            ^double document-line-height (data/line-height (.glyph layout))
            ^double minimap-line-height (data/line-height (.glyph minimap-layout))
            minimap-ratio (/ minimap-line-height document-line-height)
            viewed-start-y (+ (* minimap-ratio (- (.scroll-y layout))) (.scroll-y minimap-layout))
            viewed-height (Math/ceil (* minimap-ratio (.h canvas-rect)))]
        (.setFill gc background-color)
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))
        (.setFill gc (color-lookup color-scheme "editor.minimap.viewed.range"))
        (.fillRect gc (.x r) viewed-start-y (.w r) viewed-height)
        (draw-cursor-ranges! gc minimap-layout lines minimap-cursor-range-draw-infos)
        (draw-minimap-code! gc minimap-layout color-scheme lines syntax-info)

        ;; Draw minimap shadow if it covers part of the document.
        (when-some [^Rect scroll-tab-rect (some-> (.scroll-tab-x layout))]
          (when (not= (+ (.x canvas-rect) (.w canvas-rect)) (+ (.x scroll-tab-rect) (.w scroll-tab-rect)))
            (.setFill gc (color-lookup color-scheme "editor.minimap.shadow"))
            (.fillRect gc (- (.x r) 8.0) (.y r) 8.0 (.h r))))))

    ;; Draw horizontal scroll bar.
    (when-some [^Rect r (some-> (.scroll-tab-x layout) (data/expand-rect -3.0 -3.0))]
      (.setFill gc (if (= :scroll-tab-x (:ui-element hovered-element)) scroll-tab-hovered-color scroll-tab-color))
      (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) (.h r) (.h r)))

    ;; Draw vertical scroll bar.
    (when-some [^Rect r (some-> (.scroll-tab-y layout) (data/expand-rect -3.0 -3.0))]
      (let [hovered-ui-element (:ui-element hovered-element)
            color (case hovered-ui-element
                    (:scroll-bar-y-down :scroll-bar-y-up :scroll-tab-y) scroll-tab-hovered-color
                    scroll-tab-color)]
        (.setFill gc color)
        (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) (.w r) (.w r))

        ;; Draw dots around the scroll tab when hovering over the continuous scroll areas of the vertical scroll bar.
        (let [size 4.0
              dist 7.0
              offset 9.0
              half-size (* 0.5 size)
              half-tab-width (* 0.5 (.w r))]
          (case hovered-ui-element
            :scroll-bar-y-down
            (let [cx (- (+ (.x r) half-tab-width) half-size)
                  cy (- (+ (.y r) (.h r) offset) half-tab-width half-size)]
              (doseq [i (range 3)]
                (.fillOval gc cx (+ cy (* ^double i dist)) size size)))

            :scroll-bar-y-up
            (let [cx (- (+ (.x r) half-tab-width) half-size)
                  cy (- (+ (.y r) half-tab-width) half-size offset)]
              (doseq [i (range 3)]
                (.fillOval gc cx (- cy (* ^double i dist)) size size)))

            nil))))

    ;; Draw gutter.
    (let [^Rect gutter-rect (data/->Rect 0.0 (.y canvas-rect) (.x canvas-rect) (.h canvas-rect))]
      (when (< 0.0 (.w gutter-rect))
        (draw-gutter! gutter-view gc gutter-rect layout font color-scheme lines regions visible-cursors)))))

;; -----------------------------------------------------------------------------

(g/defnk produce-indent-type [indent-type]
  ;; TODO: Produce default from preferences.
  (or indent-type :tabs))

(g/defnk produce-indent-string [indent-type]
  (data/indent-type->indent-string indent-type))

(g/defnk produce-tab-spaces [indent-type]
  (data/indent-type->tab-spaces indent-type))

(g/defnk produce-indent-level-pattern [tab-spaces]
  (data/indent-level-pattern tab-spaces))

(g/defnk produce-font [font-name font-size]
  (Font. font-name font-size))

(g/defnk produce-glyph-metrics [font line-height-factor]
  (make-glyph-metrics font line-height-factor))

(g/defnk produce-gutter-metrics [gutter-view lines regions glyph-metrics]
  (gutter-metrics gutter-view lines regions glyph-metrics))

(g/defnk produce-layout [canvas-width canvas-height document-width scroll-x scroll-y lines gutter-metrics glyph-metrics tab-spaces visible-minimap?]
  (let [[gutter-width gutter-margin] gutter-metrics]
    (data/layout-info canvas-width canvas-height document-width scroll-x scroll-y lines gutter-width gutter-margin glyph-metrics tab-spaces visible-minimap?)))

(defn- invalidated-row [seen-invalidated-rows invalidated-rows]
  (let [seen-invalidated-rows-count (count seen-invalidated-rows)
        invalidated-rows-count (count invalidated-rows)]
    (cond
      (or (zero? seen-invalidated-rows-count)
          (zero? invalidated-rows-count))
      0

      ;; Redo scenario or regular use.
      (< seen-invalidated-rows-count invalidated-rows-count)
      (reduce min (subvec invalidated-rows seen-invalidated-rows-count))

      ;; Undo scenario.
      (> seen-invalidated-rows-count invalidated-rows-count)
      (reduce min (subvec seen-invalidated-rows (dec invalidated-rows-count))))))

(g/defnk produce-invalidated-syntax-info [canvas invalidated-rows lines]
  (let [seen-invalidated-rows (or (ui/user-data canvas ::invalidated-rows) [])]
    (ui/user-data! canvas ::invalidated-rows invalidated-rows)
    (let [syntax-info (or (ui/user-data canvas ::syntax-info) [])]
      (when-some [invalidated-row (invalidated-row seen-invalidated-rows invalidated-rows)]
        (let [invalidated-syntax-info (data/invalidate-syntax-info syntax-info invalidated-row (count lines))]
          (ui/user-data! canvas ::syntax-info invalidated-syntax-info)
          nil)))))

(g/defnk produce-syntax-info [canvas grammar invalidated-syntax-info layout lines]
  (assert (nil? invalidated-syntax-info))
  (if (some? grammar)
    (let [invalidated-syntax-info (or (ui/user-data canvas ::syntax-info) [])
          syntax-info (data/highlight-visible-syntax lines invalidated-syntax-info layout grammar)]
      (when-not (identical? invalidated-syntax-info syntax-info)
        (ui/user-data! canvas ::syntax-info syntax-info))
      syntax-info)
    []))

(g/defnk produce-matching-braces [lines cursor-ranges focus-state]
  (when (= :input-focused focus-state)
    (into []
          (comp (filter data/cursor-range-empty?)
                (map data/CursorRange->Cursor)
                (map (partial data/adjust-cursor lines))
                (keep (partial data/find-matching-braces lines)))
          cursor-ranges)))

(g/defnk produce-tab-trigger-scope-regions [regions]
  (filterv #(= :tab-trigger-scope (:type %)) regions))

(g/defnk produce-tab-trigger-word-regions-by-scope-id [regions]
  (group-by :scope-id (filter #(= :tab-trigger-word (:type %)) regions)))

(g/defnk produce-visible-cursors [visible-cursor-ranges focus-state]
  (when (= :input-focused focus-state)
    (mapv data/CursorRange->Cursor visible-cursor-ranges)))

(g/defnk produce-visible-cursor-ranges [lines cursor-ranges layout]
  (data/visible-cursor-ranges lines layout cursor-ranges))

(g/defnk produce-visible-regions-by-type [lines regions layout]
  (group-by :type (data/visible-cursor-ranges lines layout regions)))

(g/defnk produce-visible-matching-braces [lines matching-braces layout]
  (data/visible-cursor-ranges lines layout (into [] (mapcat identity) matching-braces)))

(defn- make-execution-marker-draw-info
  [current-color frame-color {:keys [location-type] :as execution-marker}]
  (case location-type
    :current-line
    (cursor-range-draw-info :word nil current-color execution-marker)

    :current-frame
    (cursor-range-draw-info :word nil frame-color execution-marker)))

(g/defnk produce-cursor-range-draw-infos [color-scheme lines cursor-ranges focus-state layout visible-cursors visible-cursor-ranges visible-regions-by-type visible-matching-braces highlighted-find-term find-case-sensitive? find-whole-word? execution-markers]
  (let [background-color (color-lookup color-scheme "editor.background")
        ^Color selection-background-color (color-lookup color-scheme "editor.selection.background")
        selection-background-color (if (not= :not-focused focus-state) selection-background-color (color-lookup color-scheme "editor.selection.background.inactive"))
        selection-occurrence-outline-color (color-lookup color-scheme "editor.selection.occurrence.outline")
        tab-trigger-word-outline-color (color-lookup color-scheme "editor.tab.trigger.word.outline")
        find-term-occurrence-color (color-lookup color-scheme "editor.find.term.occurrence")
        execution-marker-current-row-color (color-lookup color-scheme "editor.execution-marker.current")
        execution-marker-frame-row-color (color-lookup color-scheme "editor.execution-marker.frame")
        matching-brace-color (color-lookup color-scheme "editor.matching.brace")
        active-tab-trigger-scope-ids (into #{}
                                           (keep (fn [tab-trigger-scope-region]
                                                   (when (some #(data/cursor-range-contains? tab-trigger-scope-region (data/CursorRange->Cursor %))
                                                               cursor-ranges)
                                                     (:id tab-trigger-scope-region))))
                                           (visible-regions-by-type :tab-trigger-scope))]
    (vec
      (concat
        (map (partial cursor-range-draw-info :range selection-background-color background-color)
             visible-cursor-ranges)
        (map (partial cursor-range-draw-info :underline nil matching-brace-color)
             visible-matching-braces)
        (map (partial make-execution-marker-draw-info execution-marker-current-row-color execution-marker-frame-row-color)
             execution-markers)
        (cond
          (seq active-tab-trigger-scope-ids)
          (keep (fn [tab-trigger-word-region]
                  (when (and (contains? active-tab-trigger-scope-ids (:scope-id tab-trigger-word-region))
                             (not-any? #(data/cursor-range-contains? tab-trigger-word-region %)
                                         visible-cursors))
                    (cursor-range-draw-info :range nil tab-trigger-word-outline-color tab-trigger-word-region)))
                (visible-regions-by-type :tab-trigger-word))

          (not (empty? highlighted-find-term))
          (map (partial cursor-range-draw-info :range nil find-term-occurrence-color)
               (data/visible-occurrences lines layout find-case-sensitive? find-whole-word? (split-lines highlighted-find-term)))

          :else
          (map (partial cursor-range-draw-info :word nil selection-occurrence-outline-color)
               (data/visible-occurrences-of-selected-word lines cursor-ranges layout visible-cursor-ranges)))))))

(g/defnk produce-minimap-glyph-metrics [font-name]
  (assoc (make-glyph-metrics (Font. font-name 2.0) 1.0) :line-height 2.0))

(g/defnk produce-minimap-layout [layout lines minimap-glyph-metrics tab-spaces]
  (data/minimap-layout-info layout (count lines) minimap-glyph-metrics tab-spaces))

(g/defnk produce-minimap-cursor-range-draw-infos [color-scheme lines cursor-ranges minimap-layout highlighted-find-term find-case-sensitive? find-whole-word? execution-markers]
  (let [execution-marker-current-row-color (color-lookup color-scheme "editor.execution-marker.current")
        execution-marker-frame-row-color (color-lookup color-scheme "editor.execution-marker.frame")]
    (vec
      (concat
        (map (partial make-execution-marker-draw-info execution-marker-current-row-color execution-marker-frame-row-color)
             execution-markers)
        (cond
          (not (empty? highlighted-find-term))
          (map (partial cursor-range-draw-info :range (color-lookup color-scheme "editor.find.term.occurrence") nil)
               (data/visible-occurrences lines minimap-layout find-case-sensitive? find-whole-word? (split-lines highlighted-find-term)))

          :else
          (map (partial cursor-range-draw-info :range (color-lookup color-scheme "editor.selection.occurrence.outline") nil)
               (data/visible-occurrences-of-selected-word lines cursor-ranges minimap-layout nil)))))))

(g/defnk produce-execution-markers [lines debugger-execution-locations node-id+resource]
  (when-some [path (some-> node-id+resource second resource/proj-path)]
    (into []
          (comp (filter #(= path (:file %)))
                (map (fn [{:keys [^long line type]}]
                       (data/execution-marker lines (dec line) type))))
          debugger-execution-locations)))

(g/defnk produce-repaint-canvas [repaint-trigger ^Canvas canvas font gutter-view hovered-element layout minimap-layout color-scheme lines regions syntax-info cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-whitespace? visible-minimap? execution-markers]
  (let [regions (into [] cat [regions execution-markers])]
    (draw! (.getGraphicsContext2D canvas) font gutter-view hovered-element layout minimap-layout color-scheme lines regions syntax-info cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-whitespace? visible-minimap?))
  nil)

(defn- make-cursor-rectangle
  ^Rectangle [^Paint fill opacity ^Rect cursor-rect]
  (doto (Rectangle. (.x cursor-rect) (.y cursor-rect) (.w cursor-rect) (.h cursor-rect))
    (.setMouseTransparent true)
    (.setFill fill)
    (.setOpacity opacity)))

(g/defnk produce-repaint-cursors [repaint-trigger ^Canvas canvas ^LayoutInfo layout color-scheme lines visible-cursors cursor-opacity]
  ;; To avoid having to redraw everything while the cursor blink animation
  ;; plays, the cursors are children of the Pane that also hosts the Canvas.
  (let [^Pane canvas-pane (.getParent canvas)
        ^Rect canvas-rect (.canvas layout)
        ^Rect minimap-rect (.minimap layout)
        gutter-end (dec (.x canvas-rect))
        canvas-end (.x minimap-rect)
        children (.getChildren canvas-pane)
        cursor-color (color-lookup color-scheme "editor.cursor")
        cursor-rectangles (into []
                                (comp (map (partial data/cursor-rect layout lines))
                                      (remove (fn [^Rect cursor-rect] (< (.x cursor-rect) gutter-end)))
                                      (remove (fn [^Rect cursor-rect] (> (.x cursor-rect) canvas-end)))
                                      (map (partial make-cursor-rectangle cursor-color cursor-opacity)))
                                visible-cursors)]
    (assert (identical? canvas (first children)))
    (.remove children 1 (count children))
    (.addAll children ^Collection cursor-rectangles)
    nil))

(g/defnode CodeEditorView
  (inherits view/WorkbenchView)

  (property repaint-trigger g/Num (default 0) (dynamic visible (g/constantly false)))
  (property undo-grouping-info UndoGroupingInfo (dynamic visible (g/constantly false)))
  (property canvas Canvas (dynamic visible (g/constantly false)))
  (property canvas-width g/Num (default 0.0) (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value _new-value]
                   (let [layout (g/node-value self :layout evaluation-context)
                         scroll-x (g/node-value self :scroll-x evaluation-context)
                         new-scroll-x (data/limit-scroll-x layout scroll-x)]
                     (when (not= scroll-x new-scroll-x)
                       (g/set-property self :scroll-x new-scroll-x))))))
  (property canvas-height g/Num (default 0.0) (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   ;; NOTE: old-value will be nil when the setter is invoked for the default.
                   ;; However, our calls to g/node-value will see default values for all
                   ;; properties even though their setter fns have not yet been called.
                   (let [^double old-value (or old-value 0.0)
                         ^double new-value new-value
                         scroll-y (g/node-value self :scroll-y evaluation-context)
                         layout (g/node-value self :layout evaluation-context)
                         line-count (count (g/node-value self :lines evaluation-context))
                         resize-reference (g/node-value self :resize-reference evaluation-context)
                         new-scroll-y (data/limit-scroll-y layout line-count (case resize-reference
                                                                               :bottom (- ^double scroll-y (- old-value new-value))
                                                                               :top scroll-y))]
                     (when (not= scroll-y new-scroll-y)
                       (g/set-property self :scroll-y new-scroll-y))))))
  (property document-width g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property color-scheme ColorScheme (dynamic visible (g/constantly false)))
  (property elapsed-time-at-last-action g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property grammar g/Any (dynamic visible (g/constantly false)))
  (property gutter-view GutterView (dynamic visible (g/constantly false)))
  (property cursor-opacity g/Num (default 1.0) (dynamic visible (g/constantly false)))
  (property resize-reference ResizeReference (default :top) (dynamic visible (g/constantly false)))
  (property scroll-x g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-y g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property suggested-completions g/Any (dynamic visible (g/constantly false)))
  (property suggestions-list-view ListView (dynamic visible (g/constantly false)))
  (property suggestions-popup PopupControl (dynamic visible (g/constantly false)))
  (property gesture-start GestureInfo (dynamic visible (g/constantly false)))
  (property highlighted-find-term g/Str (default "") (dynamic visible (g/constantly false)))
  (property hovered-element HoveredElement (dynamic visible (g/constantly false)))
  (property find-case-sensitive? g/Bool (dynamic visible (g/constantly false)))
  (property find-whole-word? g/Bool (dynamic visible (g/constantly false)))
  (property focus-state FocusState (default :not-focused) (dynamic visible (g/constantly false)))

  (property font-name g/Str (default "Dejavu Sans Mono"))
  (property font-size g/Num (default (g/constantly default-font-size)))
  (property line-height-factor g/Num (default 1.0))
  (property visible-indentation-guides? g/Bool (default false))
  (property visible-whitespace? g/Bool (default false))
  (property visible-minimap? g/Bool (default false))

  (input completions g/Any)
  (input cursor-ranges r/CursorRanges)
  (input indent-type r/IndentType)
  (input invalidated-rows r/InvalidatedRows)
  (input lines r/Lines)
  (input regions r/Regions)
  (input debugger-execution-locations g/Any)

  (output indent-type r/IndentType produce-indent-type)
  (output indent-string g/Str produce-indent-string)
  (output tab-spaces g/Num produce-tab-spaces)
  (output indent-level-pattern Pattern :cached produce-indent-level-pattern)
  (output font Font :cached produce-font)
  (output glyph-metrics data/GlyphMetrics :cached produce-glyph-metrics)
  (output gutter-metrics GutterMetrics :cached produce-gutter-metrics)
  (output layout LayoutInfo :cached produce-layout)
  (output invalidated-syntax-info SideEffect :cached produce-invalidated-syntax-info)
  (output syntax-info SyntaxInfo :cached produce-syntax-info)
  (output matching-braces MatchingBraces :cached produce-matching-braces)
  (output tab-trigger-scope-regions r/Regions :cached produce-tab-trigger-scope-regions)
  (output tab-trigger-word-regions-by-scope-id r/RegionGrouping :cached produce-tab-trigger-word-regions-by-scope-id)
  (output visible-cursors r/Cursors :cached produce-visible-cursors)
  (output visible-cursor-ranges r/CursorRanges :cached produce-visible-cursor-ranges)
  (output visible-regions-by-type r/RegionGrouping :cached produce-visible-regions-by-type)
  (output visible-matching-braces r/CursorRanges :cached produce-visible-matching-braces)
  (output cursor-range-draw-infos CursorRangeDrawInfos :cached produce-cursor-range-draw-infos)
  (output minimap-glyph-metrics data/GlyphMetrics :cached produce-minimap-glyph-metrics)
  (output minimap-layout LayoutInfo :cached produce-minimap-layout)
  (output minimap-cursor-range-draw-infos CursorRangeDrawInfos :cached produce-minimap-cursor-range-draw-infos)
  (output execution-markers r/Regions :cached produce-execution-markers)
  (output repaint-canvas SideEffect :cached produce-repaint-canvas)
  (output repaint-cursors SideEffect :cached produce-repaint-cursors))

(defn- mouse-button [^MouseEvent event]
  (condp = (.getButton event)
    MouseButton/NONE nil
    MouseButton/PRIMARY :primary
    MouseButton/SECONDARY :secondary
    MouseButton/MIDDLE :middle))

(defn- operation-sequence-tx-data [view-node undo-grouping]
  (if (nil? undo-grouping)
    []
    (let [[prev-undo-grouping prev-opseq] (g/node-value view-node :undo-grouping-info)]
      (assert (contains? undo-groupings undo-grouping))
      (cond
        (= undo-grouping prev-undo-grouping)
        [(g/operation-sequence prev-opseq)]

        (and (contains? #{:navigation :selection} undo-grouping)
             (contains? #{:navigation :selection} prev-undo-grouping))
        [(g/operation-sequence prev-opseq)
         (g/set-property view-node :undo-grouping-info [undo-grouping prev-opseq])]

        :else
        (let [opseq (gensym)]
          [(g/operation-sequence opseq)
           (g/set-property view-node :undo-grouping-info [undo-grouping opseq])])))))

;; Since the elapsed time updates at 60 fps, we keep it outside the graph to avoid transaction churn.
;; This is updated from the repaint-view! function elsewhere in this file.
(defonce ^:private *elapsed-time-by-view-node (atom {}))

(defn- prelude-tx-data [view-node undo-grouping values-by-prop-kw]
  ;; Along with undo grouping info, we also keep track of when an action was
  ;; last performed in the document. We use this to stop the cursor from
  ;; blinking while typing or navigating.
  (into (operation-sequence-tx-data view-node undo-grouping)
        (when (or (contains? values-by-prop-kw :cursor-ranges)
                  (contains? values-by-prop-kw :lines))
          (g/set-property view-node :elapsed-time-at-last-action (get @*elapsed-time-by-view-node view-node 0.0)))))

;; -----------------------------------------------------------------------------

;; The functions that perform actions in the core.data module return maps of
;; properties that were modified by the operation. Some of these properties need
;; to be stored at various locations in order to be undoable, some are transient,
;; and so on. These two functions should be used to read and write these managed
;; properties at all times. Basically, get-property needs to be able to get any
;; property that is supplied to set-properties! in values-by-prop-kw.

(defn get-property
  "Gets the value of a property that is managed by the functions in the code.data module."
  [view-node prop-kw]
  (case prop-kw
    :invalidated-row
    (invalidated-row (ui/user-data (g/node-value view-node :canvas) ::invalidated-rows)
                     (g/node-value (g/node-value view-node :resource-node) :invalidated-rows))

    (g/node-value view-node prop-kw)))

(defn set-properties!
  "Sets values of properties that are managed by the functions in the code.data module.
  Returns true if any property changed, false otherwise."
  [view-node undo-grouping values-by-prop-kw]
  (if (empty? values-by-prop-kw)
    false
    (let [resource-node (g/node-value view-node :resource-node)]
      (g/transact
        (into (prelude-tx-data view-node undo-grouping values-by-prop-kw)
              (mapcat (fn [[prop-kw value]]
                        (case prop-kw
                          (:cursor-ranges :indent-type :lines :regions)
                          (g/set-property resource-node prop-kw value)

                          ;; Several rows could have been invalidated between repaints.
                          ;; We accumulate these to compare against seen invalidations.
                          :invalidated-row
                          (g/update-property resource-node :invalidated-rows conj value)

                          (g/set-property view-node prop-kw value))))
              values-by-prop-kw))
      true)))

;; -----------------------------------------------------------------------------
;; Code completion
;; -----------------------------------------------------------------------------

(defn- suggestion-info [lines cursor-ranges]
  (when-some [query-cursor-range (data/suggestion-query-cursor-range lines cursor-ranges)]
    (let [query-text (data/cursor-range-text lines query-cursor-range)]
      (when-not (string/blank? query-text)
        (let [contexts (string/split query-text #"\." 2)]
          (if (= 1 (count contexts))
            [query-cursor-range "" (contexts 0)]
            [query-cursor-range (contexts 0) (contexts 1)]))))))

(defn- cursor-bottom
  ^Point2D [^LayoutInfo layout lines ^Cursor adjusted-cursor]
  (let [^Rect canvas (.canvas layout)
        ^Rect r (data/cursor-rect layout lines adjusted-cursor)]
    (Point2D. (.x r) (+ (.y r) (.h r)))))

(defn- pref-suggestions-popup-position
  ^Point2D [^Canvas canvas width height ^Point2D cursor-bottom]
  (Utils/pointRelativeTo canvas width height HPos/CENTER VPos/CENTER (.getX cursor-bottom) (.getY cursor-bottom) true))

(defn- find-tab-trigger-scope-region-at-cursor
  ^CursorRange [tab-trigger-scope-regions ^Cursor cursor]
  (some (fn [scope-region]
          (when (data/cursor-range-contains? scope-region cursor)
            scope-region))
        tab-trigger-scope-regions))

(defn- find-tab-trigger-word-region-at-cursor
  ^CursorRange [tab-trigger-scope-regions tab-trigger-word-regions-by-scope-id ^Cursor cursor]
  (when-some [scope-region (find-tab-trigger-scope-region-at-cursor tab-trigger-scope-regions cursor)]
    (some (fn [word-region]
            (when (data/cursor-range-contains? word-region cursor)
              word-region))
          (get tab-trigger-word-regions-by-scope-id (:id scope-region)))))

(defn- suggested-completions [view-node]
  (let [tab-trigger-scope-regions (get-property view-node :tab-trigger-scope-regions)
        tab-trigger-word-regions-by-scope-id (get-property view-node :tab-trigger-word-regions-by-scope-id)
        tab-trigger-word-types (into #{}
                                     (comp (map data/CursorRange->Cursor)
                                           (keep (partial find-tab-trigger-word-region-at-cursor
                                                          tab-trigger-scope-regions
                                                          tab-trigger-word-regions-by-scope-id))
                                           (map :word-type))
                                     (get-property view-node :cursor-ranges))]
    (when-not (or (contains? tab-trigger-word-types :name)
                  (contains? tab-trigger-word-types :arglist))
      (get-property view-node :completions))))

(defn- show-suggestions! [view-node]
  (when-some [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    ;; Snapshot completions when the popup is first opened to prevent
    ;; typed words from showing up in the completions list.
    (when (nil? (g/node-value view-node :suggested-completions))
      (g/set-property! view-node :suggested-completions (suggested-completions view-node)))

    (let [lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          [replaced-cursor-range context query] (suggestion-info lines cursor-ranges)
          query-text (if (empty? context) query (str context \. query))]
      (if-not (some #(Character/isLetter ^char %) query-text)
        (when (.isShowing suggestions-popup)
          (.hide suggestions-popup))
        (let [completions (g/node-value view-node :suggested-completions)
              context-completions (get completions context)
              filtered-completions (some->> context-completions (popup/fuzzy-option-filter-fn :name :display-string query-text))]
          (if (empty? filtered-completions)
            (when (.isShowing suggestions-popup)
              (.hide suggestions-popup))
            (let [^LayoutInfo layout (get-property view-node :layout)
                  ^Canvas canvas (g/node-value view-node :canvas)
                  ^ListView suggestions-list-view (g/node-value view-node :suggestions-list-view)
                  selected-index (when (seq filtered-completions) 0)
                  [width height] (popup/update-list-view! suggestions-list-view 200.0 filtered-completions selected-index)
                  cursor-bottom (cursor-bottom layout lines (data/CursorRange->Cursor replaced-cursor-range))
                  anchor (pref-suggestions-popup-position canvas width height cursor-bottom)]
              (if (.isShowing suggestions-popup)
                (doto suggestions-popup
                  (.setAnchorX (.getX anchor))
                  (.setAnchorY (.getY anchor)))
                (let [font-name (g/node-value view-node :font-name)
                      font-size (g/node-value view-node :font-size)]
                  (doto suggestions-list-view
                    (.setFixedCellSize (data/line-height (.glyph layout)))
                    (.setStyle (eutil/format* "-fx-font-family: \"%s\"; -fx-font-size: %gpx;" font-name font-size)))
                  (.show suggestions-popup canvas (.getX anchor) (.getY anchor))))
              nil)))))))

(defn- hide-suggestions! [view-node]
  (when-some [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    (g/set-property! view-node :suggested-completions nil)
    (when (.isShowing suggestions-popup)
      (.hide suggestions-popup))))

(defn- suggestions-shown? [view-node]
  (if-some [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    (.isShowing suggestions-popup)
    false))

(defn- selected-suggestion [view-node]
  (let [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)
        ^ListView suggestions-list-view (g/node-value view-node :suggestions-list-view)]
    (when (and (some? suggestions-popup)
               (some? suggestions-list-view)
               (.isShowing suggestions-popup))
      (first (ui/selection suggestions-list-view)))))

(defn- in-tab-trigger? [view-node]
  (let [tab-trigger-scope-regions (get-property view-node :tab-trigger-scope-regions)]
    (if (empty? tab-trigger-scope-regions)
      false
      (let [cursor-ranges (get-property view-node :cursor-ranges)]
        (some? (some #(find-tab-trigger-scope-region-at-cursor tab-trigger-scope-regions
                                                               (data/CursorRange->Cursor %))
                     cursor-ranges))))))

(defn- tab-trigger-related-region? [^CursorRange region]
  (case (:type region)
    (:tab-trigger-scope :tab-trigger-word) true
    false))

(defn- exit-tab-trigger! [view-node]
  (hide-suggestions! view-node)
  (when (in-tab-trigger? view-node)
    (set-properties! view-node :navigation
                     {:regions (into []
                                     (remove tab-trigger-related-region?)
                                     (get-property view-node :regions))})))

(defn- find-closest-tab-trigger-word-region
  ^CursorRange [search-direction tab-trigger-scope-regions tab-trigger-word-regions-by-scope-id ^CursorRange cursor-range]
  ;; NOTE: Cursor range collections are assumed to be in ascending order.
  (let [cursor-range->cursor (case search-direction :prev data/cursor-range-start :next data/cursor-range-end)
        from-cursor (cursor-range->cursor cursor-range)]
    (if-some [scope-region (find-tab-trigger-scope-region-at-cursor tab-trigger-scope-regions from-cursor)]
      ;; The cursor range is inside a tab trigger scope region.
      ;; Try to find the next word region inside the scope region.
      (let [skip-word-region? (case search-direction
                                :prev #(not (pos? (data/compare-cursor-position from-cursor (cursor-range->cursor %))))
                                :next #(not (neg? (data/compare-cursor-position from-cursor (cursor-range->cursor %)))))
            word-regions-in-scope (get tab-trigger-word-regions-by-scope-id (:id scope-region))
            ordered-word-regions (case search-direction
                                   :prev (rseq word-regions-in-scope)
                                   :next word-regions-in-scope)
            closest-word-region (first (drop-while skip-word-region? ordered-word-regions))]
        (if (and (some? closest-word-region)
                 (data/cursor-range-contains? scope-region (cursor-range->cursor closest-word-region)))
          ;; Return matching word cursor range.
          (data/sanitize-cursor-range closest-word-region)

          ;; There are no more word regions inside the scope region.
          ;; If moving forward, place the cursor at the end of the scope region.
          ;; Otherwise return unchanged.
          (case search-direction
            :prev cursor-range
            :next (data/sanitize-cursor-range (data/cursor-range-end-range scope-region)))))

      ;; The cursor range is outside of any tab trigger scope ranges. Return unchanged.
      cursor-range)))

(defn- select-closest-tab-trigger-word-region! [search-direction view-node]
  (hide-suggestions! view-node)
  (when-some [tab-trigger-scope-regions (not-empty (get-property view-node :tab-trigger-scope-regions))]
    (let [tab-trigger-word-regions-by-scope-id (get-property view-node :tab-trigger-word-regions-by-scope-id)
          find-closest-tab-trigger-word-region (partial find-closest-tab-trigger-word-region search-direction tab-trigger-scope-regions tab-trigger-word-regions-by-scope-id)
          cursor-ranges (get-property view-node :cursor-ranges)
          new-cursor-ranges (mapv find-closest-tab-trigger-word-region cursor-ranges)]
      (let [exited-tab-trigger-scope-regions (into #{}
                                                   (filter (fn [^CursorRange scope-region]
                                                             (let [at-scope-end? (partial data/cursor-range-equals? (data/cursor-range-end-range scope-region))]
                                                               (some at-scope-end? new-cursor-ranges))))
                                                   tab-trigger-scope-regions)
            removed-regions (into exited-tab-trigger-scope-regions
                                  (comp (map :id)
                                        (mapcat tab-trigger-word-regions-by-scope-id))
                                  exited-tab-trigger-scope-regions)
            regions (get-property view-node :regions)
            new-regions (into [] (remove removed-regions) regions)]
        (set-properties! view-node :selection
                         (cond-> {:cursor-ranges new-cursor-ranges}

                                 (not= (count regions) (count new-regions))
                                 (assoc :regions new-regions)))))))

(def ^:private prev-tab-trigger! (partial select-closest-tab-trigger-word-region! :prev))
(def ^:private next-tab-trigger! (partial select-closest-tab-trigger-word-region! :next))

(defn- find-tab-trigger-word-regions [lines suggestion ^CursorRange scope-region]
  (when-some [tab-trigger-words (some-> suggestion :tab-triggers :select not-empty)]
    (let [tab-trigger-word-types (some-> suggestion :tab-triggers :types)
          search-range (case (:type suggestion)
                         :function (if-some [opening-paren (data/find-next-occurrence lines ["("] (data/cursor-range-start scope-region) true false)]
                                     (data/->CursorRange (data/cursor-range-end opening-paren)
                                                         (data/cursor-range-end scope-region))
                                     scope-region)
                         scope-region)]
      (assert (or (nil? tab-trigger-word-types)
                  (= (count tab-trigger-words)
                     (count tab-trigger-word-types))))
      (into []
            (map-indexed (fn [index word-cursor-range]
                           (let [tab-trigger-word-type (get tab-trigger-word-types index)]
                             (cond-> (assoc word-cursor-range
                                       :type :tab-trigger-word
                                       :scope-id (:id scope-region))

                                     (some? tab-trigger-word-type)
                                     (assoc :word-type tab-trigger-word-type)))))
            (data/find-sequential-words-in-scope lines tab-trigger-words search-range)))))

(defn- accept-suggestion! [view-node]
  (when-some [selected-suggestion (selected-suggestion view-node)]
    (let [indent-level-pattern (get-property view-node :indent-level-pattern)
          indent-string (get-property view-node :indent-string)
          grammar (get-property view-node :grammar)
          lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          regions (get-property view-node :regions)
          layout (get-property view-node :layout)
          [replaced-cursor-range] (suggestion-info lines cursor-ranges)
          replaced-char-count (- (.col (data/cursor-range-end replaced-cursor-range))
                                 (.col (data/cursor-range-start replaced-cursor-range)))
          replacement-lines (split-lines (:insert-string selected-suggestion))
          props (data/replace-typed-chars indent-level-pattern indent-string grammar lines cursor-ranges regions layout replaced-char-count replacement-lines)]
      (when (some? props)
        (hide-suggestions! view-node)
        (let [cursor-ranges (:cursor-ranges props)
              tab-trigger-scope-regions (map #(assoc % :type :tab-trigger-scope :id (gensym "tab-scope")) cursor-ranges)
              tab-trigger-word-region-colls (map (partial find-tab-trigger-word-regions (:lines props) selected-suggestion) tab-trigger-scope-regions)
              tab-trigger-word-regions (apply concat tab-trigger-word-region-colls)
              new-cursor-ranges (if (seq tab-trigger-word-regions)
                                  (mapv (comp data/sanitize-cursor-range first) tab-trigger-word-region-colls)
                                  (mapv data/cursor-range-end-range cursor-ranges))]
          (set-properties! view-node nil
                           (cond-> (assoc props :cursor-ranges new-cursor-ranges)

                                   (seq tab-trigger-word-regions)
                                   (assoc :regions (vec (sort (concat (remove tab-trigger-related-region? regions)
                                                                      tab-trigger-scope-regions
                                                                      tab-trigger-word-regions))))

                                   true
                                   (data/frame-cursor layout))))))))

;; -----------------------------------------------------------------------------

(defn move! [view-node move-type cursor-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/move (get-property view-node :lines)
                              (get-property view-node :cursor-ranges)
                              (get-property view-node :layout)
                              move-type
                              cursor-type)))

(defn page-up! [view-node move-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-up (get-property view-node :lines)
                                 (get-property view-node :cursor-ranges)
                                 (get-property view-node :scroll-y)
                                 (get-property view-node :layout)
                                 move-type)))

(defn page-down! [view-node move-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-down (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :scroll-y)
                                   (get-property view-node :layout)
                                   move-type)))

(defn delete! [view-node delete-type]
  (let [cursor-ranges (get-property view-node :cursor-ranges)
        cursor-ranges-empty? (every? data/cursor-range-empty? cursor-ranges)
        single-character-backspace? (and cursor-ranges-empty? (= :delete-before delete-type))
        undo-grouping (when cursor-ranges-empty? :typing)
        delete-fn (case delete-type
                    :delete-before data/delete-character-before-cursor
                    :delete-after data/delete-character-after-cursor)]
    (when-not single-character-backspace?
      (hide-suggestions! view-node))
    (set-properties! view-node undo-grouping
                     (data/delete (get-property view-node :lines)
                                  cursor-ranges
                                  (get-property view-node :regions)
                                  (get-property view-node :layout)
                                  delete-fn))
    (when (and single-character-backspace?
               (suggestions-shown? view-node))
      (show-suggestions! view-node))))

(defn select-all! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/select-all (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges))))

(defn select-next-occurrence! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/select-next-occurrence (get-property view-node :lines)
                                                (get-property view-node :cursor-ranges)
                                                (get-property view-node :layout))))

(defn- indent! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/indent (get-property view-node :indent-level-pattern)
                                (get-property view-node :indent-string)
                                (get-property view-node :grammar)
                                (get-property view-node :lines)
                                (get-property view-node :cursor-ranges)
                                (get-property view-node :regions)
                                (get-property view-node :layout))))

(defn- deindent! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/deindent (get-property view-node :lines)
                                  (get-property view-node :cursor-ranges)
                                  (get-property view-node :regions)
                                  (get-property view-node :tab-spaces))))

(defn- tab! [view-node]
  (cond
    (suggestions-shown? view-node)
    (accept-suggestion! view-node)

    (in-tab-trigger? view-node)
    (next-tab-trigger! view-node)

    :else
    (indent! view-node)))

(defn- shift-tab! [view-node]
  (cond
    (in-tab-trigger? view-node)
    (prev-tab-trigger! view-node)

    :else
    (deindent! view-node)))

(handler/defhandler :tab :code-view
  (run [view-node] (tab! view-node)))

(handler/defhandler :backwards-tab-trigger :code-view
  (run [view-node] (shift-tab! view-node)))

(handler/defhandler :proposals :code-view
  (run [view-node] (show-suggestions! view-node)))

;; -----------------------------------------------------------------------------

(defn handle-key-pressed! [view-node ^KeyEvent event]
  ;; Only handle bare key events that cannot be bound to handlers here.
  (when-not (or (.isAltDown event)
                (.isMetaDown event)
                (.isShiftDown event)
                (.isShortcutDown event))
    (when (not= ::unhandled
                (condp = (.getCode event)
                  KeyCode/HOME  (move! view-node :navigation :home)
                  KeyCode/END   (move! view-node :navigation :end)
                  KeyCode/LEFT  (move! view-node :navigation :left)
                  KeyCode/RIGHT (move! view-node :navigation :right)
                  KeyCode/UP    (move! view-node :navigation :up)
                  KeyCode/DOWN  (move! view-node :navigation :down)
                  ::unhandled))
      (.consume event))))

(defn- typable-key-event?
  [^KeyEvent e]
  (-> e
      keymap/key-event->map
      keymap/typable?))

(defn handle-key-typed! [view-node ^KeyEvent event]
  (.consume event)
  (when (typable-key-event? event)
    (let [typed (.getCharacter event)
          undo-grouping (if (= "\r" typed) :newline :typing)
          selected-suggestion (selected-suggestion view-node)
          grammar (get-property view-node :grammar)
          lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          [insert-typed? show-suggestions?] (cond
                                              (and (= "\r" typed) (some? selected-suggestion))
                                              (do (accept-suggestion! view-node)
                                                  [false false])

                                              (and (= "(" typed) (= :function (:type selected-suggestion)))
                                              (do (accept-suggestion! view-node)
                                                  [false false])

                                              (and (= "." typed) (= :namespace (:type selected-suggestion)))
                                              (do (accept-suggestion! view-node)
                                                  [true true])

                                              (data/typing-deindents-line? grammar lines cursor-ranges typed)
                                              [true false]

                                              :else
                                              [true true])]
      (when insert-typed?
        (when (set-properties! view-node undo-grouping
                               (data/key-typed (get-property view-node :indent-level-pattern)
                                               (get-property view-node :indent-string)
                                               grammar
                                               lines
                                               cursor-ranges
                                               (get-property view-node :regions)
                                               (get-property view-node :layout)
                                               typed))
          (if show-suggestions?
            (show-suggestions! view-node)
            (hide-suggestions! view-node)))))))

(defn- refresh-mouse-cursor! [view-node ^MouseEvent event]
  (let [hovered-element (get-property view-node :hovered-element)
        gesture-type (:type (get-property view-node :gesture-start))
        ^LayoutInfo layout (get-property view-node :layout)
        ^Node node (.getTarget event)
        x (.getX event)
        y (.getY event)
        cursor (cond
                 (or (= :ui-element (:type hovered-element))
                     (= :scroll-tab-x-drag gesture-type)
                     (= :scroll-tab-y-drag gesture-type)
                     (= :scroll-bar-y-hold-up gesture-type)
                     (= :scroll-bar-y-hold-down gesture-type))
                 javafx.scene.Cursor/DEFAULT

                 (data/rect-contains? (.canvas layout) x y)
                 ;; TODO:
                 ;; Currently using an image cursor results in a corrupted mouse pointer under macOS High Sierra.
                 ;; We should test periodically and use the white cursor on macOS as well when the bug is fixed.
                 (if (eutil/is-mac-os?)
                   javafx.scene.Cursor/TEXT
                   ui/text-cursor-white)

                 :else
                 javafx.scene.Cursor/DEFAULT)]
    ;; The cursor refresh appears buggy at the moment.
    ;; Calling setCursor with DISAPPEAR before setting the cursor forces it to refresh.
    (when (not= cursor (.getCursor node))
      (.setCursor node javafx.scene.Cursor/DISAPPEAR)
      (.setCursor node cursor))))

(defn handle-mouse-pressed! [view-node ^MouseEvent event]
  (.consume event)
  (.requestFocus ^Node (.getTarget event))
  (refresh-mouse-cursor! view-node event)
  (hide-suggestions! view-node)
  (set-properties! view-node (if (< 1 (.getClickCount event)) :selection :navigation)
                   (data/mouse-pressed (get-property view-node :lines)
                                       (get-property view-node :cursor-ranges)
                                       (get-property view-node :regions)
                                       (get-property view-node :layout)
                                       (mouse-button event)
                                       (.getClickCount event)
                                       (.getX event)
                                       (.getY event)
                                       (.isAltDown event)
                                       (.isShiftDown event)
                                       (.isShortcutDown event))))

(defn handle-mouse-moved! [view-node ^MouseDragEvent event]
  (.consume event)
  (refresh-mouse-cursor! view-node event)
  (set-properties! view-node :selection
                   (data/mouse-moved (get-property view-node :lines)
                                     (get-property view-node :cursor-ranges)
                                     (get-property view-node :layout)
                                     (get-property view-node :gesture-start)
                                     (get-property view-node :hovered-element)
                                     (.getX event)
                                     (.getY event))))

(defn handle-mouse-released! [view-node ^MouseEvent event]
  (.consume event)
  (refresh-mouse-cursor! view-node event)
  (set-properties! view-node :selection
                   (data/mouse-released (get-property view-node :layout)
                                        (get-property view-node :gesture-start)
                                        (mouse-button event)
                                        (.getX event)
                                        (.getY event))))

(defn handle-mouse-exited! [view-node ^MouseEvent event]
  (.consume event)
  (set-properties! view-node :selection
                   (data/mouse-exited (get-property view-node :gesture-start)
                                      (get-property view-node :hovered-element))))

(defn handle-scroll! [view-node ^ScrollEvent event]
  (.consume event)
  (when (set-properties! view-node :navigation
                         (data/scroll (get-property view-node :lines)
                                      (get-property view-node :scroll-x)
                                      (get-property view-node :scroll-y)
                                      (get-property view-node :layout)
                                      (.getDeltaX event)
                                      (.getDeltaY event)))
    (hide-suggestions! view-node)))

;; -----------------------------------------------------------------------------

(defn has-selection? [view-node]
  (not-every? data/cursor-range-empty? (get-property view-node :cursor-ranges)))

(defn can-paste? [view-node clipboard]
  (data/can-paste? (get-property view-node :cursor-ranges) clipboard))

(defn cut! [view-node clipboard]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/cut! (get-property view-node :lines)
                              (get-property view-node :cursor-ranges)
                              (get-property view-node :regions)
                              (get-property view-node :layout)
                              clipboard)))

(defn copy! [view-node clipboard]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/copy! (get-property view-node :lines)
                               (get-property view-node :cursor-ranges)
                               clipboard)))

(defn paste! [view-node clipboard]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/paste (get-property view-node :indent-level-pattern)
                               (get-property view-node :indent-string)
                               (get-property view-node :grammar)
                               (get-property view-node :lines)
                               (get-property view-node :cursor-ranges)
                               (get-property view-node :regions)
                               (get-property view-node :layout)
                               clipboard)))

(defn split-selection-into-lines! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/split-selection-into-lines (get-property view-node :lines)
                                                    (get-property view-node :cursor-ranges))))

(handler/defhandler :select-up :code-view
  (run [view-node] (move! view-node :selection :up)))

(handler/defhandler :select-down :code-view
  (run [view-node] (move! view-node :selection :down)))

(handler/defhandler :select-left :code-view
  (run [view-node] (move! view-node :selection :left)))

(handler/defhandler :select-right :code-view
  (run [view-node] (move! view-node :selection :right)))

(handler/defhandler :prev-word :code-view
  (run [view-node] (move! view-node :navigation :prev-word)))

(handler/defhandler :select-prev-word :code-view
  (run [view-node] (move! view-node :selection :prev-word)))

(handler/defhandler :next-word :code-view
  (run [view-node] (move! view-node :navigation :next-word)))

(handler/defhandler :select-next-word :code-view
  (run [view-node] (move! view-node :selection :next-word)))

(handler/defhandler :beginning-of-line :code-view
  (run [view-node] (move! view-node :navigation :line-start)))

(handler/defhandler :select-beginning-of-line :code-view
  (run [view-node] (move! view-node :selection :line-start)))

(handler/defhandler :beginning-of-line-text :code-view
  (run [view-node] (move! view-node :navigation :home)))

(handler/defhandler :select-beginning-of-line-text :code-view
  (run [view-node] (move! view-node :selection :home)))

(handler/defhandler :end-of-line :code-view
  (run [view-node] (move! view-node :navigation :end)))

(handler/defhandler :select-end-of-line :code-view
  (run [view-node] (move! view-node :selection :end)))

(handler/defhandler :page-up :code-view
  (run [view-node] (page-up! view-node :navigation)))

(handler/defhandler :select-page-up :code-view
  (run [view-node] (page-up! view-node :selection)))

(handler/defhandler :page-down :code-view
  (run [view-node] (page-down! view-node :navigation)))

(handler/defhandler :select-page-down :code-view
  (run [view-node] (page-down! view-node :selection)))

(handler/defhandler :beginning-of-file :code-view
  (run [view-node] (move! view-node :navigation :file-start)))

(handler/defhandler :select-beginning-of-file :code-view
  (run [view-node] (move! view-node :selection :file-start)))

(handler/defhandler :end-of-file :code-view
  (run [view-node] (move! view-node :navigation :file-end)))

(handler/defhandler :select-end-of-file :code-view
  (run [view-node] (move! view-node :selection :file-end)))

(handler/defhandler :cut :code-view
  (enabled? [view-node] (has-selection? view-node))
  (run [view-node clipboard] (cut! view-node clipboard)))

(handler/defhandler :copy :code-view
  (enabled? [view-node] (has-selection? view-node))
  (run [view-node clipboard] (copy! view-node clipboard)))

(handler/defhandler :paste :code-view
  (enabled? [view-node clipboard] (can-paste? view-node clipboard))
  (run [view-node clipboard] (paste! view-node clipboard)))

(handler/defhandler :select-all :code-view
  (run [view-node] (select-all! view-node)))

(handler/defhandler :delete :code-view
  (run [view-node] (delete! view-node :delete-after)))

(handler/defhandler :delete-backward :code-view
  (run [view-node] (delete! view-node :delete-before)))

(handler/defhandler :select-next-occurrence :code-view
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :select-next-occurrence :code-view-tools
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :split-selection-into-lines :code-view
  (run [view-node] (split-selection-into-lines! view-node)))

(handler/defhandler :toggle-breakpoint :code-view
  (run [view-node]
       (let [lines (get-property view-node :lines)
             cursor-ranges (get-property view-node :cursor-ranges)
             regions (get-property view-node :regions)
             breakpoint-rows (data/cursor-ranges->rows lines cursor-ranges)]
         (set-properties! view-node nil
                          (data/toggle-breakpoint lines
                                                  regions
                                                  breakpoint-rows)))))

(handler/defhandler :reindent :code-view
  (enabled? [view-node] (not-every? data/cursor-range-empty? (get-property view-node :cursor-ranges)))
  (run [view-node]
       (set-properties! view-node nil
                        (data/reindent (get-property view-node :indent-level-pattern)
                                       (get-property view-node :indent-string)
                                       (get-property view-node :grammar)
                                       (get-property view-node :lines)
                                       (get-property view-node :cursor-ranges)
                                       (get-property view-node :regions)
                                       (get-property view-node :layout)))))

(handler/defhandler :convert-indentation :code-view
  (run [view-node user-data]
       (set-properties! view-node nil
                        (data/convert-indentation (get-property view-node :indent-type)
                                                  user-data
                                                  (get-property view-node :lines)
                                                  (get-property view-node :cursor-ranges)
                                                  (get-property view-node :regions)))))

;; -----------------------------------------------------------------------------
;; Sort Lines
;; -----------------------------------------------------------------------------

(defn can-sort-lines? [view-node]
  (some data/cursor-range-multi-line? (get-property view-node :cursor-ranges)))

(defn sort-lines! [view-node sort-key-fn]
  (set-properties! view-node nil
                   (data/sort-lines (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges)
                                    (get-property view-node :regions)
                                    sort-key-fn)))

(handler/defhandler :sort-lines :code-view
  (enabled? [view-node] (can-sort-lines? view-node))
  (run [view-node] (sort-lines! view-node string/lower-case)))

(handler/defhandler :sort-lines-case-sensitive :code-view
  (enabled? [view-node] (can-sort-lines? view-node))
  (run [view-node] (sort-lines! view-node identity)))

;; -----------------------------------------------------------------------------
;; Properties shared among views
;; -----------------------------------------------------------------------------

;; WARNING:
;; Observing or binding to an observable that lives longer than the observer will
;; cause a memory leak. You must manually unhook them or use weak listeners.
;; Source: https://community.oracle.com/message/10360893#10360893

(defonce ^SimpleStringProperty bar-ui-type-property (doto (SimpleStringProperty.) (.setValue (name :hidden))))
(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleStringProperty find-replacement-property (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleBooleanProperty find-whole-word-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-case-sensitive-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-wrap-property (doto (SimpleBooleanProperty.) (.setValue true)))
(defonce ^SimpleDoubleProperty font-size-property (doto (SimpleDoubleProperty.) (.setValue default-font-size)))
(defonce ^SimpleBooleanProperty visible-indentation-guides-property (doto (SimpleBooleanProperty.) (.setValue true)))
(defonce ^SimpleBooleanProperty visible-minimap-property (doto (SimpleBooleanProperty.) (.setValue true)))
(defonce ^SimpleBooleanProperty visible-whitespace-property (doto (SimpleBooleanProperty.) (.setValue true)))

(defonce ^StringBinding highlighted-find-term-property
  (-> (Bindings/when (Bindings/or (Bindings/equal (name :find) bar-ui-type-property)
                                  (Bindings/equal (name :replace) bar-ui-type-property)))
      (.then find-term-property)
      (.otherwise "")))

(defn- init-property-and-bind-preference! [^Property property prefs preference default]
  (.setValue property (prefs/get-prefs prefs preference default))
  (ui/observe property (fn [_ _ new] (prefs/set-prefs prefs preference new))))

(defn initialize! [prefs]
  (init-property-and-bind-preference! find-term-property prefs "code-editor-find-term" "")
  (init-property-and-bind-preference! find-replacement-property prefs "code-editor-find-replacement" "")
  (init-property-and-bind-preference! find-whole-word-property prefs "code-editor-find-whole-word" false)
  (init-property-and-bind-preference! find-case-sensitive-property prefs "code-editor-find-case-sensitive" false)
  (init-property-and-bind-preference! find-wrap-property prefs "code-editor-find-wrap" true)
  (init-property-and-bind-preference! font-size-property prefs "code-editor-font-size" default-font-size)
  (init-property-and-bind-preference! visible-indentation-guides-property prefs "code-editor-visible-indentation-guides" true)
  (init-property-and-bind-preference! visible-minimap-property prefs "code-editor-visible-minimap" true)
  (init-property-and-bind-preference! visible-whitespace-property prefs "code-editor-visible-whitespace" true))

;; -----------------------------------------------------------------------------
;; View Settings
;; -----------------------------------------------------------------------------

(handler/defhandler :zoom-out :code-view-tools
  (enabled? [] (<= 4.0 ^double (.getValue font-size-property)))
  (run [] (when (<= 4.0 ^double (.getValue font-size-property))
            (.setValue font-size-property (dec ^double (.getValue font-size-property))))))

(handler/defhandler :zoom-in :code-view-tools
  (enabled? [] (>= 32.0 ^double (.getValue font-size-property)))
  (run [] (when (>= 32.0 ^double (.getValue font-size-property))
            (.setValue font-size-property (inc ^double (.getValue font-size-property))))))

(handler/defhandler :reset-zoom :code-view-tools
  (run [] (.setValue font-size-property default-font-size)))

(handler/defhandler :toggle-indentation-guides :code-view-tools
  (run [] (.setValue visible-indentation-guides-property (not (.getValue visible-indentation-guides-property))))
  (state [] (.getValue visible-indentation-guides-property)))

(handler/defhandler :toggle-minimap :code-view-tools
  (run [] (.setValue visible-minimap-property (not (.getValue visible-minimap-property))))
  (state [] (.getValue visible-minimap-property)))

(handler/defhandler :toggle-visible-whitespace :code-view-tools
  (run [] (.setValue visible-whitespace-property (not (.getValue visible-whitespace-property))))
  (state [] (.getValue visible-whitespace-property)))

;; -----------------------------------------------------------------------------
;; Go to Line
;; -----------------------------------------------------------------------------

(defn- bar-ui-visible? []
  (not= (name :hidden) (.getValue bar-ui-type-property)))

(defn- set-bar-ui-type! [ui-type]
  (case ui-type (:hidden :goto-line :find :replace) (.setValue bar-ui-type-property (name ui-type))))

(defn- focus-code-editor! [view-node]
  (let [^Canvas canvas (g/node-value view-node :canvas)]
    (.requestFocus canvas)))

(defn- try-parse-row [^long document-row-count ^String value]
  ;; Returns nil for an empty string.
  ;; Returns a zero-based row index for valid line numbers, starting at one.
  ;; Returns a string describing the problem for invalid input.
  (when-not (empty? value)
    (try
      (let [line-number (Long/parseLong value)]
        (dec (Math/max 1 (Math/min line-number document-row-count))))
      (catch NumberFormatException _
        "Input must be a number"))))

(defn- try-parse-goto-line-text [view-node ^String text]
  (let [lines (get-property view-node :lines)]
    (try-parse-row (count lines) text)))

(defn- try-parse-goto-line-bar-row [view-node ^GridPane goto-line-bar]
  (ui/with-controls goto-line-bar [^TextField line-field]
    (let [maybe-row (try-parse-goto-line-text view-node (.getText line-field))]
      (when (number? maybe-row)
        maybe-row))))

(defn- setup-goto-line-bar! [^GridPane goto-line-bar view-node]
  (ui/bind-presence! goto-line-bar (Bindings/equal (name :goto-line) bar-ui-type-property))
  (ui/with-controls goto-line-bar [^TextField line-field ^Button go-button]
    (ui/bind-keys! goto-line-bar {KeyCode/ENTER :goto-entered-line})
    (ui/bind-action! go-button :goto-entered-line)
    (ui/observe (.textProperty line-field)
                (fn [_ _ line-field-text]
                  (ui/refresh-bound-action-enabled! go-button)
                  (let [maybe-row (try-parse-goto-line-text view-node line-field-text)
                        error-message (when-not (number? maybe-row) maybe-row)]
                    (assert (or (nil? error-message) (string? error-message)))
                    (ui/tooltip! line-field error-message)
                    (if (some? error-message)
                      (ui/add-style! line-field "field-error")
                      (ui/remove-style! line-field "field-error"))))))
  (doto goto-line-bar
    (ui/context! :goto-line-bar {:goto-line-bar goto-line-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1)))

(defn- dispose-goto-line-bar! [^GridPane goto-line-bar]
  (.unbind (.visibleProperty goto-line-bar)))

(handler/defhandler :goto-line :code-view-tools
  (run [goto-line-bar]
       (set-bar-ui-type! :goto-line)
       (ui/with-controls goto-line-bar [^TextField line-field]
         (.requestFocus line-field)
         (.selectAll line-field))))

(handler/defhandler :goto-entered-line :goto-line-bar
  (enabled? [goto-line-bar view-node] (some? (try-parse-goto-line-bar-row view-node goto-line-bar)))
  (run [goto-line-bar view-node]
       (when-some [line-number (try-parse-goto-line-bar-row view-node goto-line-bar)]
         (let [cursor-range (data/Cursor->CursorRange (data/->Cursor line-number 0))]
           (set-properties! view-node :navigation
                            (data/select-and-frame (get-property view-node :lines)
                                                   (get-property view-node :layout)
                                                   cursor-range)))
         (set-bar-ui-type! :hidden)
         ;; Close bar on next tick so the code view will not insert a newline
         ;; if the bar was dismissed by pressing the Enter key.
         (ui/run-later
           (focus-code-editor! view-node)))))

;; -----------------------------------------------------------------------------
;; Find & Replace
;; -----------------------------------------------------------------------------

(defn- setup-find-bar! [^GridPane find-bar view-node]
  (doto find-bar
    (ui/bind-presence! (Bindings/equal (name :find) bar-ui-type-property))
    (ui/context! :code-view-find-bar {:find-bar find-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^Button next ^Button prev]
    (.bindBidirectional (.textProperty term) find-term-property)
    (.bindBidirectional (.selectedProperty whole-word) find-whole-word-property)
    (.bindBidirectional (.selectedProperty case-sensitive) find-case-sensitive-property)
    (.bindBidirectional (.selectedProperty wrap) find-wrap-property)
    (ui/bind-key-commands! find-bar {"Enter" :find-next
                                     "Shift+Enter" :find-prev})
    (ui/bind-action! next :find-next)
    (ui/bind-action! prev :find-prev))
  find-bar)

(defn- dispose-find-bar! [^GridPane find-bar]
  (.unbind (.visibleProperty find-bar))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term]
    (.unbindBidirectional (.textProperty term) find-term-property)
    (.unbindBidirectional (.selectedProperty whole-word) find-whole-word-property)
    (.unbindBidirectional (.selectedProperty case-sensitive) find-case-sensitive-property)
    (.unbindBidirectional (.selectedProperty wrap) find-wrap-property)))

(defn- setup-replace-bar! [^GridPane replace-bar view-node]
  (doto replace-bar
    (ui/bind-presence! (Bindings/equal (name :replace) bar-ui-type-property))
    (ui/context! :code-view-replace-bar {:replace-bar replace-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1))
  (ui/with-controls replace-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^TextField replacement ^Button next ^Button replace ^Button replace-all]
    (.bindBidirectional (.textProperty term) find-term-property)
    (.bindBidirectional (.textProperty replacement) find-replacement-property)
    (.bindBidirectional (.selectedProperty whole-word) find-whole-word-property)
    (.bindBidirectional (.selectedProperty case-sensitive) find-case-sensitive-property)
    (.bindBidirectional (.selectedProperty wrap) find-wrap-property)
    (ui/bind-action! next :find-next)
    (ui/bind-action! replace :replace-next)
    (ui/bind-keys! replace-bar {KeyCode/ENTER :replace-next})
    (ui/bind-action! replace-all :replace-all))
  replace-bar)

(defn- dispose-replace-bar! [^GridPane replace-bar]
  (.unbind (.visibleProperty replace-bar))
  (ui/with-controls replace-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^TextField replacement]
    (.unbindBidirectional (.textProperty term) find-term-property)
    (.unbindBidirectional (.textProperty replacement) find-replacement-property)
    (.unbindBidirectional (.selectedProperty whole-word) find-whole-word-property)
    (.unbindBidirectional (.selectedProperty case-sensitive) find-case-sensitive-property)
    (.unbindBidirectional (.selectedProperty wrap) find-wrap-property)))

(defn- focus-term-field! [^Parent bar]
  (ui/with-controls bar [^TextField term]
    (.requestFocus term)
    (.selectAll term)))

(defn- set-find-term! [^String term-text]
  (.setValue find-term-property (or term-text "")))

(defn non-empty-single-selection-text [view-node]
  (when-some [single-cursor-range (util/only (get-property view-node :cursor-ranges))]
    (when-not (data/cursor-range-empty? single-cursor-range)
      (data/cursor-range-text (get-property view-node :lines) single-cursor-range))))

(defn- find-next! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/find-next (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :layout)
                                   (split-lines (.getValue find-term-property))
                                   (.getValue find-case-sensitive-property)
                                   (.getValue find-whole-word-property)
                                   (.getValue find-wrap-property))))

(defn- find-prev! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/find-prev (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :layout)
                                   (split-lines (.getValue find-term-property))
                                   (.getValue find-case-sensitive-property)
                                   (.getValue find-whole-word-property)
                                   (.getValue find-wrap-property))))

(defn- replace-next! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/replace-next (get-property view-node :lines)
                                      (get-property view-node :cursor-ranges)
                                      (get-property view-node :regions)
                                      (get-property view-node :layout)
                                      (split-lines (.getValue find-term-property))
                                      (split-lines (.getValue find-replacement-property))
                                      (.getValue find-case-sensitive-property)
                                      (.getValue find-whole-word-property)
                                      (.getValue find-wrap-property))))

(defn- replace-all! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/replace-all (get-property view-node :lines)
                                     (get-property view-node :regions)
                                     (get-property view-node :layout)
                                     (split-lines (.getValue find-term-property))
                                     (split-lines (.getValue find-replacement-property))
                                     (.getValue find-case-sensitive-property)
                                     (.getValue find-whole-word-property))))

(handler/defhandler :find-text :code-view
  (run [find-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-bar-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :replace-text :code-view
  (run [replace-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-bar-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :find-text :code-view-tools ;; In practice, from find / replace and go to line bars.
  (run [find-bar]
       (set-bar-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :replace-text :code-view-tools
  (run [replace-bar]
       (set-bar-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :escape :code-view-tools
  (run [find-bar replace-bar view-node]
       (cond
         (in-tab-trigger? view-node)
         (exit-tab-trigger! view-node)

         (suggestions-shown? view-node)
         (hide-suggestions! view-node)

         (bar-ui-visible?)
         (do (set-bar-ui-type! :hidden)
             (focus-code-editor! view-node))

         :else
         (set-properties! view-node :selection
                          (data/escape (get-property view-node :cursor-ranges))))))

(handler/defhandler :find-next :code-view-find-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :code-view-replace-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :code-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-prev :code-view-find-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :code-view-replace-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :code-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :replace-next :code-view-replace-bar
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :replace-next :code-view
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :replace-all :code-view-replace-bar
  (run [view-node] (replace-all! view-node)))

;; -----------------------------------------------------------------------------

(ui/extend-menu ::menubar :editor.app-view/edit-end
                [{:command :find-text                  :label "Find..."}
                 {:command :find-next                  :label "Find Next"}
                 {:command :find-prev                  :label "Find Previous"}
                 {:label :separator}
                 {:command :replace-text               :label "Replace..."}
                 {:command :replace-next               :label "Replace Next"}
                 {:label :separator}
                 {:command :reindent                   :label "Reindent Lines"}

                 {:label "Convert Indentation"
                  :children [{:label "To Tabs"
                              :command :convert-indentation
                              :user-data :tabs}
                             {:label "To Two Spaces"
                              :command :convert-indentation
                              :user-data :two-spaces}
                             {:label "To Four Spaces"
                              :command :convert-indentation
                              :user-data :four-spaces}]}

                 {:command :toggle-comment             :label "Toggle Comment"}
                 {:label :separator}
                 {:command :sort-lines                 :label "Sort Lines"}
                 {:command :sort-lines-case-sensitive  :label "Sort Lines (Case Sensitive)"}
                 {:label :separator}
                 {:command :select-next-occurrence     :label "Select Next Occurrence"}
                 {:command :split-selection-into-lines :label "Split Selection Into Lines"}
                 {:label :separator}
                 {:command :toggle-breakpoint          :label "Toggle Breakpoint"}])

(ui/extend-menu ::menubar :editor.app-view/view-end
                [{:command :toggle-minimap             :label "Minimap" :check true}
                 {:command :toggle-indentation-guides  :label "Indentation Guides" :check true}
                 {:command :toggle-visible-whitespace  :label "Visible Whitespace" :check true}
                 {:label :separator}
                 {:command :zoom-in                    :label "Increase Font Size"}
                 {:command :zoom-out                   :label "Decrease Font Size"}
                 {:command :reset-zoom                 :label "Reset Font Size"}
                 {:label :separator}
                 {:command :goto-line                  :label "Go to Line..."}])

;; -----------------------------------------------------------------------------

(defn- setup-view! [resource-node view-node app-view]
  (let [glyph-metrics (g/node-value view-node :glyph-metrics)
        tab-spaces (g/node-value view-node :tab-spaces)
        tab-stops (data/tab-stops glyph-metrics tab-spaces)
        lines (g/node-value resource-node :lines)
        document-width (data/max-line-width glyph-metrics tab-stops lines)]
    (g/transact
      (concat
        (g/set-property view-node :document-width document-width)
        (g/connect resource-node :completions view-node :completions)
        (g/connect resource-node :cursor-ranges view-node :cursor-ranges)
        (g/connect resource-node :indent-type view-node :indent-type)
        (g/connect resource-node :invalidated-rows view-node :invalidated-rows)
        (g/connect resource-node :lines view-node :lines)
        (g/connect resource-node :regions view-node :regions)
        (g/connect app-view :debugger-execution-locations view-node :debugger-execution-locations)))
    view-node))

(defn- cursor-opacity
  ^double [^double elapsed-time-at-last-action ^double elapsed-time]
  (if (< ^double (mod (- elapsed-time elapsed-time-at-last-action) 1.0) 0.5) 1.0 0.0))

(defn- draw-fps-counters! [^GraphicsContext gc ^double fps]
  (let [canvas (.getCanvas gc)
        margin 14.0
        width 83.0
        height 24.0
        top margin
        right (- (.getWidth canvas) margin)
        left (- right width)]
    (.setFill gc Color/DARKSLATEGRAY)
    (.fillRect gc left top width height)
    (.setFill gc Color/WHITE)
    (.fillText gc (format "%.3f fps" fps) (- right 5.0) (+ top 16.0))))

(defn repaint-view! [view-node elapsed-time]
  (swap! *elapsed-time-by-view-node assoc view-node elapsed-time)
  (let [evaluation-context (g/make-evaluation-context)
        elapsed-time-at-last-action (g/node-value view-node :elapsed-time-at-last-action evaluation-context)
        old-cursor-opacity (g/node-value view-node :cursor-opacity evaluation-context)
        new-cursor-opacity (cursor-opacity elapsed-time-at-last-action elapsed-time)]
    (set-properties! view-node nil
                     (cond-> (data/tick (g/node-value view-node :lines evaluation-context)
                                        (g/node-value view-node :layout evaluation-context)
                                        (g/node-value view-node :gesture-start evaluation-context))
                             (not= old-cursor-opacity new-cursor-opacity) (assoc :cursor-opacity new-cursor-opacity)))
    (g/update-cache-from-evaluation-context! evaluation-context))
  (let [evaluation-context (g/make-evaluation-context)]
    (g/node-value view-node :repaint-canvas evaluation-context)
    (g/node-value view-node :repaint-cursors evaluation-context)
    (when-some [^PerformanceTracker performance-tracker @*performance-tracker]
      (let [^long repaint-trigger (g/node-value view-node :repaint-trigger evaluation-context)
            ^Canvas canvas (g/node-value view-node :canvas evaluation-context)]
        (g/set-property! view-node :repaint-trigger (unchecked-inc repaint-trigger))
        (draw-fps-counters! (.getGraphicsContext2D canvas) (.getInstantFPS performance-tracker))
        (when (= 0 (mod repaint-trigger 10))
          (.resetAverageFPS performance-tracker))))
    (g/update-cache-from-evaluation-context! evaluation-context)))

(defn- make-suggestions-list-view
  ^ListView [^Canvas canvas]
  (doto (popup/make-choices-list-view 17.0 (partial popup/make-choices-list-view-cell :display-string))
    (.addEventFilter KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                       (let [^KeyEvent event event
                             ^KeyCode code (.getCode event)]
                         (when-not (or (= KeyCode/UP code)
                                       (= KeyCode/DOWN code)
                                       (= KeyCode/PAGE_UP code)
                                       (= KeyCode/PAGE_DOWN code))
                           (ui/send-event! canvas event)
                           (.consume event)))))))

(defn- make-execution-marker-arrow
  ([x y w h]
   (make-execution-marker-arrow x y w h 0.5 0.4))
  ([x y w h w-prop h-prop]
   (let [x ^long x
         y ^long y
         w ^long w
         h ^long h
         w-body (* ^double w-prop w)
         h-body (* ^double h-prop h)
         ;; x coords used, left to right
         x0 x
         x1 (+ x0 w-body)
         x2 (+ x0 w)
         ;; y coords used, top to bottom
         y0 y
         y1 (+ y (* 0.5 (- h h-body)))
         y2 (+ y (* 0.5 h))
         y3 (+ y (- h (* 0.5 (- h h-body))))
         y4 (+ y h)]
     {:xs (double-array [x0 x1 x1 x2 x1 x1 x0])
      :ys (double-array [y1 y1 y0 y2 y4 y3 y3])
      :n  7})))

(deftype CodeEditorGutterView []
  GutterView

  (gutter-metrics [this lines regions glyph-metrics]
    (let [gutter-margin (data/line-height glyph-metrics)]
      (data/gutter-metrics glyph-metrics gutter-margin (count lines))))

  (draw-gutter! [this gc gutter-rect layout font color-scheme lines regions visible-cursors]
    (let [^GraphicsContext gc gc
          ^Rect gutter-rect gutter-rect
          ^LayoutInfo layout layout
          glyph-metrics (.glyph layout)
          ^double line-height (data/line-height glyph-metrics)
          gutter-foreground-color (color-lookup color-scheme "editor.gutter.foreground")
          gutter-background-color (color-lookup color-scheme "editor.gutter.background")
          gutter-shadow-color (color-lookup color-scheme "editor.gutter.shadow")
          gutter-breakpoint-color (color-lookup color-scheme "editor.gutter.breakpoint")
          gutter-cursor-line-background-color (color-lookup color-scheme "editor.gutter.cursor.line.background")
          gutter-execution-marker-current-color (color-lookup color-scheme "editor.gutter.execution-marker.current")
          gutter-execution-marker-frame-color (color-lookup color-scheme "editor.gutter.execution-marker.frame")]

      ;; Draw gutter background and shadow when scrolled horizontally.
      (when (neg? (.scroll-x layout))
        (.setFill gc gutter-background-color)
        (.fillRect gc (.x gutter-rect) (.y gutter-rect) (.w gutter-rect) (.h gutter-rect))
        (.setFill gc gutter-shadow-color)
        (.fillRect gc (+ (.x gutter-rect) (.w gutter-rect)) 0.0 8.0 (.h gutter-rect)))

      ;; Highlight lines with cursors in gutter.
      (.setFill gc gutter-cursor-line-background-color)
      (let [highlight-width (- (+ (.x gutter-rect) (.w gutter-rect)) (/ line-height 2.0))
            highlight-height (dec line-height)]
        (doseq [^Cursor cursor visible-cursors]
          (let [y (+ (data/row->y layout (.row cursor)) 0.5)]
            (.fillRect gc 0 y highlight-width highlight-height))))

      ;; Draw line numbers and markers in gutter.
      (.setFont gc font)
      (.setTextAlign gc TextAlignment/RIGHT)
      (let [^Rect line-numbers-rect (.line-numbers layout)
            ^double ascent (data/ascent glyph-metrics)
            drawn-line-count (.drawn-line-count layout)
            dropped-line-count (.dropped-line-count layout)
            source-line-count (count lines)
            indicator-diameter (- line-height 6.0)
            breakpoint-rows (data/cursor-ranges->rows lines (filter data/breakpoint-region? regions))
            execution-markers-by-type (group-by :location-type (filter data/execution-marker? regions))
            execution-marker-current-rows (data/cursor-ranges->rows lines (:current-line execution-markers-by-type))
            execution-marker-frame-rows (data/cursor-ranges->rows lines (:current-frame execution-markers-by-type))]
        (loop [drawn-line-index 0
               source-line-index dropped-line-count]
          (when (and (< drawn-line-index drawn-line-count)
                     (< source-line-index source-line-count))
            (let [y (data/row->y layout source-line-index)]
              (when (contains? breakpoint-rows source-line-index)
                (.setFill gc gutter-breakpoint-color)
                (.fillOval gc
                           (+ (.x line-numbers-rect) (.w line-numbers-rect) 3.0)
                           (+ y 3.0) indicator-diameter indicator-diameter))
              (when (contains? execution-marker-current-rows source-line-index)
                (let [x (+ (.x line-numbers-rect) (.w line-numbers-rect) 4.0)
                      y (+ y 4.0)
                      w (- line-height 8.0)
                      h (- line-height 8.0)
                      {:keys [xs ys n]} (make-execution-marker-arrow x y w h)]
                  (.setFill gc gutter-execution-marker-current-color)
                  (.fillPolygon gc xs ys n)))
              (when (contains? execution-marker-frame-rows source-line-index)
                (let [x (+ (.x line-numbers-rect) (.w line-numbers-rect) 4.0)
                      y (+ y 4.0)
                      w (- line-height 8.0)
                      h (- line-height 8.0)
                      {:keys [xs ys n]} (make-execution-marker-arrow x y w h)]
                  (.setFill gc gutter-execution-marker-frame-color)
                  (.fillPolygon gc xs ys n)))
              (.setFill gc gutter-foreground-color)
              (.fillText gc (str (inc source-line-index))
                         (+ (.x line-numbers-rect) (.w line-numbers-rect))
                         (+ ascent y))
              (recur (inc drawn-line-index)
                     (inc source-line-index)))))))))

(defn make-property-change-setter
  ^ChangeListener [node-id prop-kw]
  (assert (integer? node-id))
  (assert (keyword? prop-kw))
  (reify ChangeListener
    (changed [_this _observable _old new]
      (g/set-property! node-id prop-kw new))))

(defn make-focus-change-listener
  ^ChangeListener [view-node parent canvas]
  (assert (integer? view-node))
  (assert (instance? Parent parent))
  (assert (instance? Canvas canvas))
  (reify ChangeListener
    (changed [_ _ _ focus-owner]
      (g/set-property! view-node :focus-state
                       (cond
                         (= canvas focus-owner)
                         :input-focused

                         (some? (ui/closest-node-where (partial = parent) focus-owner))
                         :semi-focused

                         :else
                         :not-focused)))))

(defn- make-view! [graph parent resource-node opts]
  (let [^Tab tab (:tab opts)
        grid (GridPane.)
        canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        suggestions-list-view (make-suggestions-list-view canvas)
        suggestions-popup (popup/make-choices-popup canvas-pane suggestions-list-view)
        undo-grouping-info (pair :navigation (gensym))
        app-view (:app-view opts)
        view-node (setup-view! resource-node
                               (g/make-node! graph CodeEditorView
                                             :canvas canvas
                                             :color-scheme code-color-scheme
                                             :font-size (.getValue font-size-property)
                                             :grammar (:grammar opts)
                                             :gutter-view (CodeEditorGutterView.)
                                             :highlighted-find-term (.getValue highlighted-find-term-property)
                                             :line-height-factor 1.2
                                             :suggestions-list-view suggestions-list-view
                                             :suggestions-popup suggestions-popup
                                             :undo-grouping-info undo-grouping-info
                                             :visible-indentation-guides? (.getValue visible-indentation-guides-property)
                                             :visible-minimap? (.getValue visible-minimap-property)
                                             :visible-whitespace? (.getValue visible-whitespace-property))
                               app-view)
        goto-line-bar (setup-goto-line-bar! (ui/load-fxml "goto-line.fxml") view-node)
        find-bar (setup-find-bar! (ui/load-fxml "find.fxml") view-node)
        replace-bar (setup-replace-bar! (ui/load-fxml "replace.fxml") view-node)
        repainter (ui/->timer "repaint-code-editor-view" (fn [_ elapsed-time]
                                                           (when (.isSelected tab)
                                                             (repaint-view! view-node elapsed-time))))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :goto-line-bar goto-line-bar
                     :find-bar find-bar
                     :replace-bar replace-bar
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.setCursor javafx.scene.Cursor/TEXT)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (handle-key-pressed! view-node event)))
      (.addEventHandler KeyEvent/KEY_TYPED (ui/event-handler event (handle-key-typed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (handle-mouse-released! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_EXITED (ui/event-handler event (handle-mouse-exited! view-node event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (handle-scroll! view-node event))))

    (ui/context! grid :code-view-tools context-env nil)

    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints canvas-pane 0 0)
    (GridPane/setVgrow canvas-pane Priority/ALWAYS)

    (ui/children! grid [canvas-pane goto-line-bar find-bar replace-bar])
    (ui/children! parent [grid])
    (ui/fill-control grid)
    (ui/context! canvas :code-view context-env nil)

    ;; Steal input focus when our tab becomes selected.
    (ui/observe (.selectedProperty tab)
                (fn [_ _ became-selected?]
                  (when became-selected?
                    (ui/run-later (.requestFocus canvas)))))

    ;; Clicking an autocomplete-entry in the suggestions popup accepts it.
    (.setOnMouseClicked suggestions-list-view
                        (ui/event-handler event
                          (when (some? (ui/cell-item-under-mouse event))
                            (accept-suggestion! view-node))))

    ;; Highlight occurrences of search term while find bar is open.
    (let [find-case-sensitive-setter (make-property-change-setter view-node :find-case-sensitive?)
          find-whole-word-setter (make-property-change-setter view-node :find-whole-word?)
          font-size-setter (make-property-change-setter view-node :font-size)
          highlighted-find-term-setter (make-property-change-setter view-node :highlighted-find-term)
          visible-indentation-guides-setter (make-property-change-setter view-node :visible-indentation-guides?)
          visible-minimap-setter (make-property-change-setter view-node :visible-minimap?)
          visible-whitespace-setter (make-property-change-setter view-node :visible-whitespace?)]
      (.addListener find-case-sensitive-property find-case-sensitive-setter)
      (.addListener find-whole-word-property find-whole-word-setter)
      (.addListener font-size-property font-size-setter)
      (.addListener highlighted-find-term-property highlighted-find-term-setter)
      (.addListener visible-indentation-guides-property visible-indentation-guides-setter)
      (.addListener visible-minimap-property visible-minimap-setter)
      (.addListener visible-whitespace-property visible-whitespace-setter)

      ;; Ensure the focus-state property reflects the current input focus state.
      (let [^Stage stage (g/node-value app-view :stage)
            ^Scene scene (.getScene stage)
            focus-owner-property (.focusOwnerProperty scene)
            focus-change-listener (make-focus-change-listener view-node grid canvas)]
        (.addListener focus-owner-property focus-change-listener)

        ;; Remove callbacks when our tab is closed.
        (ui/on-closed! tab (fn [_]
                             (ui/timer-stop! repainter)
                             (dispose-goto-line-bar! goto-line-bar)
                             (dispose-find-bar! find-bar)
                             (dispose-replace-bar! replace-bar)
                             (swap! *elapsed-time-by-view-node dissoc view-node)
                             (g/set-property! view-node :elapsed-time-at-last-action 0.0)
                             (.removeListener find-case-sensitive-property find-case-sensitive-setter)
                             (.removeListener find-whole-word-property find-whole-word-setter)
                             (.removeListener font-size-property font-size-setter)
                             (.removeListener highlighted-find-term-property highlighted-find-term-setter)
                             (.removeListener visible-indentation-guides-property visible-indentation-guides-setter)
                             (.removeListener visible-minimap-property visible-minimap-setter)
                             (.removeListener visible-whitespace-property visible-whitespace-setter)
                             (.removeListener focus-owner-property focus-change-listener)))))

    ;; Start repaint timer.
    (ui/timer-start! repainter)
    view-node))

(defn- focus-view! [view-node {:keys [line]}]
  (.requestFocus ^Node (g/node-value view-node :canvas))
  (when-some [row (some-> ^double line dec)]
    (set-properties! view-node :navigation
                     (data/select-and-frame (get-property view-node :lines)
                                            (get-property view-node :layout)
                                            (data/Cursor->CursorRange (data/->Cursor row 0))))))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :label "Code"
                                :make-view-fn (fn [graph parent resource-node opts] (make-view! graph parent resource-node opts))
                                :focus-fn (fn [view-node opts] (focus-view! view-node opts))
                                :text-selection-fn non-empty-single-selection-text))
