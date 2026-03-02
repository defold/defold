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

(ns editor.code.view
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.popup :as fx.popup]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as mutator]
            [cljfx.prop :as prop]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code-completion :as code-completion]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :refer [split-lines]]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.lsp :as lsp]
            [editor.markdown :as markdown]
            [editor.menu-items :as menu-items]
            [editor.notifications :as notifications]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.ui.fuzzy-choices-popup :as popup]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [schema.core :as s]
            [service.smoke-log :as slog]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [com.defold.control ListView]
           [com.sun.javafx.font FontResource FontStrike PGFont]
           [com.sun.javafx.geom.transform BaseTransform]
           [com.sun.javafx.perf PerformanceTracker]
           [com.sun.javafx.scene.text FontHelper]
           [com.sun.javafx.tk Toolkit]
           [com.sun.javafx.util Utils]
           [editor.code.data Cursor CursorRange GestureInfo LayoutInfo Rect]
           [java.util Collection]
           [java.util.regex Pattern]
           [javafx.beans.binding ObjectBinding]
           [javafx.beans.property Property SimpleBooleanProperty SimpleDoubleProperty SimpleObjectProperty SimpleStringProperty]
           [javafx.beans.value ChangeListener]
           [javafx.event Event EventHandler]
           [javafx.geometry HPos Point2D Rectangle2D VPos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.control Button CheckBox Tab TextField]
           [javafx.scene.input Clipboard DataFormat InputMethodEvent InputMethodRequests KeyCode KeyEvent MouseButton MouseDragEvent MouseEvent ScrollEvent]
           [javafx.scene.layout ColumnConstraints GridPane Pane Priority]
           [javafx.scene.paint Color LinearGradient Paint]
           [javafx.scene.shape Rectangle]
           [javafx.scene.text Font FontSmoothingType Text TextAlignment]
           [javafx.stage PopupWindow Screen Stage]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private default-font-size 12.0)

(defonce/protocol GutterView
  (gutter-metrics [this lines regions glyph-metrics] "A two-element vector with a rounded double representing the width of the gutter and another representing the margin on each side within the gutter.")
  (draw-gutter! [this gc gutter-rect layout hovered-ui-element font color-scheme lines regions visible-cursors hovered-row] "Draws the gutter into the specified Rect."))

(defonce/record CursorRangeDrawInfo [type fill stroke cursor-range])

(defn- cursor-range-draw-info [type fill stroke cursor-range]
  {:pre [(case type (:range :squiggle :underline :word) true false)
         (or (nil? fill) (instance? Paint fill))
         (or (nil? stroke) (instance? Paint stroke))
         (instance? CursorRange cursor-range)]}
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
(g/deftype VisibleWhitespace (s/enum :all :none :rogue))

(defn- boolean->visible-whitespace [visible?]
  (if visible? :all :rogue))

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

(def ^:private ^:const min-cached-char-width
  (double (inc Byte/MIN_VALUE)))

(def ^:private ^:const max-cached-char-width
  (double Byte/MAX_VALUE))

(defn- make-char-width-cache [^FontStrike font-strike]
  (let [cache (byte-array (inc (int Character/MAX_VALUE)) Byte/MIN_VALUE)]
    (fn get-char-width [^Character character]
      (let [ch (unchecked-char character)
            i (unchecked-int ch)
            cached-width (aget cache i)]
        (if (= cached-width Byte/MIN_VALUE)
          (let [width (Math/floor (.getCharAdvance font-strike ch))]
            (when (and (<= min-cached-char-width width)
                       (<= width max-cached-char-width))
              (aset cache i (byte width)))
            width)
          cached-width)))))

(defonce/record GlyphMetrics [char-width-cache ^double line-height ^double ascent]
  data/GlyphMetrics
  (ascent [_this] ascent)
  (line-height [_this] line-height)
  (char-width [_this character] (char-width-cache character)))

(defn make-glyph-metrics
  ^GlyphMetrics [^Font font ^double line-height-factor]
  (let [font-loader (.getFontLoader (Toolkit/getToolkit))
        font-metrics (.getFontMetrics font-loader font)
        font-strike (.getStrike ^PGFont (FontHelper/getNativeFont font)
                                BaseTransform/IDENTITY_TRANSFORM
                                FontResource/AA_GREYSCALE)
        line-height (Math/ceil (* (inc (.getLineHeight font-metrics)) line-height-factor))
        ascent (Math/ceil (* (.getAscent font-metrics) line-height-factor))]
    (->GlyphMetrics (make-char-width-cache font-strike) line-height ascent)))

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
     ["editor.indentation.guide" (.deriveColor foreground-color 0.0 1.0 1.0 0.1)]
     ["editor.matching.brace" (Color/valueOf "#A2B0BE")]
     ["editor.minimap.shadow" (LinearGradient/valueOf "to left, rgba(0, 0, 0, 0.2) 0%, transparent 100%")]
     ["editor.minimap.viewed.range" (Color/valueOf "#393C41")]
     ["editor.scroll.tab" (.deriveColor foreground-color 0.0 1.0 1.0 0.15)]
     ["editor.scroll.tab.hovered" (.deriveColor foreground-color 0.0 1.0 1.0 0.5)]
     ["editor.whitespace.space" (.deriveColor foreground-color 0.0 1.0 1.0 0.2)]
     ["editor.whitespace.tab" (.deriveColor foreground-color 0.0 1.0 1.0 0.1)]
     ["editor.whitespace.rogue" (Color/valueOf "#FBCE2F")]]))

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
     ["type" (Color/valueOf "#FFBBFF")]
     ["support.function" (Color/valueOf "#33CCCC")]
     ["support.variable" (Color/valueOf "#FFBBFF")]
     ["name.function" (Color/valueOf "#33CC33")]
     ["parameter.function" (Color/valueOf "#E3A869")]
     ["variable.language" (Color/valueOf "#E066FF")]
     ["editor.error" (Color/valueOf "#FF6161")]
     ["editor.warning" (Color/valueOf "#FF9A34")]
     ["editor.info" (Color/valueOf "#CCCFD3")]
     ["editor.debug" (Color/valueOf "#3B8CF8")]]))

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
      :underline nil
      :squiggle nil)))

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
                     (.strokeLine gc sx y ex y)))
      :squiggle (doseq [^Rect r rects]
                  (let [sx (.x r)
                        ex (+ sx (.w r))
                        y (+ (.y r) (.h r))
                        old-line-dashes (.getLineDashes gc)]
                    (doto gc
                      (.setLineDashes (double-array [2.0 3.0]))
                      (.strokeLine sx y ex y)
                      (.setLineDashes old-line-dashes)))))))

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

(defn- draw-code! [^GraphicsContext gc ^Font font ^LayoutInfo layout color-scheme lines syntax-info indent-type visible-whitespace]
  (let [^Rect canvas-rect (.canvas layout)
        source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double ascent (data/ascent (.glyph layout))
        ^double line-height (data/line-height (.glyph layout))
        visible-whitespace? (= :all visible-whitespace)
        highlight-rogue-whitespace? (not= :none visible-whitespace)
        foreground-color (color-lookup color-scheme "editor.foreground")
        space-color (color-lookup color-scheme "editor.whitespace.space")
        tab-color (color-lookup color-scheme "editor.whitespace.tab")
        rogue-whitespace-color (color-lookup color-scheme "editor.whitespace.rogue")]
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
                visible-end-x (+ visible-start-x (.w canvas-rect))]
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
                                 (and highlight-rogue-whitespace?
                                      inside-leading-whitespace?
                                      (= :tabs indent-type))
                                 (do (.setFill gc rogue-whitespace-color)
                                     (.fillRect gc sx sy 1.0 1.0))

                                 visible-whitespace?
                                 (do (.setFill gc space-color)
                                     (.fillRect gc sx sy 1.0 1.0))))

                      \tab (let [sx (+ line-x x 2.0)
                                 sy (- line-y baseline-offset)]
                             (cond
                               (and highlight-rogue-whitespace?
                                    inside-leading-whitespace?
                                    (not= :tabs indent-type))
                               (do (.setFill gc rogue-whitespace-color)
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

(defn- draw! [^GraphicsContext gc ^Font font gutter-view hovered-element hovered-row ^LayoutInfo layout ^LayoutInfo minimap-layout color-scheme lines regions syntax-info cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-minimap? visible-whitespace]
  (let [^Rect canvas-rect (.canvas layout)
        source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double line-height (data/line-height (.glyph layout))
        background-color (color-lookup color-scheme "editor.background")
        scroll-tab-color (color-lookup color-scheme "editor.scroll.tab")
        scroll-tab-hovered-color (color-lookup color-scheme "editor.scroll.tab.hovered")
        hovered-ui-element (:ui-element hovered-element)]
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
    (draw-code! gc font layout color-scheme lines syntax-info indent-type visible-whitespace)

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

        ;; Draw the viewed range if the mouse hovers the minimap.
        (case hovered-ui-element
          (:minimap :minimap-viewed-range)
          (let [color (color-lookup color-scheme  "editor.minimap.viewed.range")]
            (.setFill gc color)
            (.fillRect gc (.x r) viewed-start-y (.w r) viewed-height))

          nil)

        (draw-cursor-ranges! gc minimap-layout lines minimap-cursor-range-draw-infos)
        (draw-minimap-code! gc minimap-layout color-scheme lines syntax-info)

        ;; Draw minimap shadow if it covers part of the document.
        (when-some [^Rect scroll-tab-rect (some-> (.scroll-tab-x layout))]
          (when (not= (+ (.x canvas-rect) (.w canvas-rect)) (+ (.x scroll-tab-rect) (.w scroll-tab-rect)))
            (.setFill gc (color-lookup color-scheme "editor.minimap.shadow"))
            (.fillRect gc (- (.x r) 8.0) (.y r) 8.0 (.h r))))))

    ;; Draw horizontal scroll bar.
    (when-some [^Rect r (some-> (.scroll-tab-x layout) (data/expand-rect -3.0 -3.0))]
      (.setFill gc (if (= :scroll-tab-x hovered-ui-element) scroll-tab-hovered-color scroll-tab-color))
      (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) (.h r) (.h r)))

    ;; Draw vertical scroll bar.
    (when-some [^Rect r (some-> (.scroll-tab-y layout) (data/expand-rect -3.0 -3.0))]
      (let [color (case hovered-ui-element
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
        (draw-gutter! gutter-view gc gutter-rect layout hovered-ui-element font color-scheme lines regions visible-cursors hovered-row)))))

;; -----------------------------------------------------------------------------

(g/defnk produce-indent-type [indent-type]
  ;; Defaults to :two-spaces when not connected to a resource node. This is what
  ;; the Protobuf-based file formats use.
  (or indent-type :two-spaces))

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

(defn- invalidated-row
  "Find the first invalidated row by comparing ever-growing histories of all
  invalidated rows. Seen means the history at the point the view was last drawn.
  By looking at the subset of added-since-seen or removed-since-seen, we can
  find the first invalidated row since the previous repaint."
  [seen-invalidated-rows invalidated-rows]
  (let [seen-invalidated-rows-count (count seen-invalidated-rows)
        invalidated-rows-count (count invalidated-rows)]
    (cond
      ;; Redo scenario or regular use.
      (< seen-invalidated-rows-count invalidated-rows-count)
      (reduce min (subvec invalidated-rows seen-invalidated-rows-count))

      ;; Undo scenario.
      (> seen-invalidated-rows-count invalidated-rows-count)
      (reduce min (subvec seen-invalidated-rows invalidated-rows-count)))))

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

(g/defnk produce-tab-trigger-regions-by-scope-id [regions]
  (->> regions
       (eduction
         (filter #(let [type (:type %)]
                    (or (= :tab-trigger-word type)
                        (= :tab-trigger-exit type)))))
       (group-by :scope-id)))

(g/defnk produce-visible-cursors [visible-cursor-ranges focus-state]
  (when (= :input-focused focus-state)
    (mapv data/CursorRange->Cursor visible-cursor-ranges)))

(g/defnk produce-visible-cursor-ranges [lines cursor-ranges layout]
  (data/visible-cursor-ranges lines layout cursor-ranges))

(g/defnk produce-visible-regions [lines regions layout]
  (data/visible-cursor-ranges lines layout regions))

(g/defnk produce-visible-matching-braces [lines matching-braces layout]
  (data/visible-cursor-ranges lines layout (into [] (mapcat identity) matching-braces)))

(defn- make-execution-marker-draw-info
  [current-color frame-color {:keys [location-type] :as execution-marker}]
  (case location-type
    :current-line
    (cursor-range-draw-info :word nil current-color execution-marker)

    :current-frame
    (cursor-range-draw-info :word nil frame-color execution-marker)))

(g/defnk produce-cursor-range-draw-infos [color-scheme lines cursor-ranges focus-state layout visible-cursors visible-cursor-ranges visible-regions visible-matching-braces highlighted-find-term find-case-sensitive? find-whole-word? execution-markers]
  (let [background-color (color-lookup color-scheme "editor.background")
        ^Color selection-background-color (color-lookup color-scheme "editor.selection.background")
        selection-background-color (if (not= :not-focused focus-state) selection-background-color (color-lookup color-scheme "editor.selection.background.inactive"))
        selection-occurrence-outline-color (color-lookup color-scheme "editor.selection.occurrence.outline")
        tab-trigger-word-outline-color (color-lookup color-scheme "editor.tab.trigger.word.outline")
        find-term-occurrence-color (color-lookup color-scheme "editor.find.term.occurrence")
        execution-marker-current-row-color (color-lookup color-scheme "editor.execution-marker.current")
        execution-marker-frame-row-color (color-lookup color-scheme "editor.execution-marker.frame")
        matching-brace-color (color-lookup color-scheme "editor.matching.brace")
        foreground-color (color-lookup color-scheme "editor.foreground")
        error-color (color-lookup color-scheme "editor.error")
        warning-color (color-lookup color-scheme "editor.warning")
        info-color (color-lookup color-scheme "editor.info")
        debug-color (color-lookup color-scheme "editor.debug")
        visible-regions-by-type (group-by :type visible-regions)
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
        (map (partial cursor-range-draw-info :underline nil foreground-color)
             (visible-regions-by-type :resource-reference))
        (map (fn [{:keys [severity] :as region}]
               (cursor-range-draw-info :squiggle
                                       nil
                                       (case severity
                                         :error error-color
                                         :warning warning-color
                                         :information info-color
                                         :hint debug-color)
                                       region))
             (visible-regions-by-type :diagnostic))
        (map (partial make-execution-marker-draw-info execution-marker-current-row-color execution-marker-frame-row-color)
             execution-markers)
        (cond
          (seq active-tab-trigger-scope-ids)
          (keep (fn [tab-trigger-region]
                  (when (and (contains? active-tab-trigger-scope-ids (:scope-id tab-trigger-region))
                             (not-any? #(data/cursor-range-contains? tab-trigger-region %)
                                       visible-cursors))
                    (cursor-range-draw-info :range nil tab-trigger-word-outline-color tab-trigger-region)))
                (concat (visible-regions-by-type :tab-trigger-word)
                        (visible-regions-by-type :tab-trigger-exit)))

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

(g/defnk produce-execution-markers [lines debugger-execution-locations node-id+type+resource]
  (when-some [path (some-> node-id+type+resource (get 2) resource/proj-path)]
    (into []
          (comp (filter #(= path (:file %)))
                (map (fn [{:keys [^long line type]}]
                       (data/execution-marker lines (dec line) type))))
          debugger-execution-locations)))

(g/defnk produce-canvas-repaint-info [canvas color-scheme cursor-range-draw-infos execution-markers font grammar gutter-view hovered-element hovered-row indent-type invalidated-rows layout lines minimap-cursor-range-draw-infos minimap-layout regions repaint-trigger visible-cursors visible-indentation-guides? visible-minimap? visible-whitespace :as canvas-repaint-info]
  canvas-repaint-info)

(defn- repaint-canvas! [{:keys [^Canvas canvas execution-markers font gutter-view hovered-element hovered-row layout minimap-layout color-scheme lines regions cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-minimap? visible-whitespace] :as _canvas-repaint-info} syntax-info]
  (let [regions (into [] cat [regions execution-markers])]
    (draw! (.getGraphicsContext2D canvas) font gutter-view hovered-element hovered-row layout minimap-layout color-scheme lines regions syntax-info cursor-range-draw-infos minimap-cursor-range-draw-infos indent-type visible-cursors visible-indentation-guides? visible-minimap? visible-whitespace))
  nil)

(g/defnk produce-cursor-repaint-info [canvas color-scheme cursor-opacity layout lines repaint-trigger visible-cursors :as cursor-repaint-info]
  cursor-repaint-info)

(defn- make-cursor-rectangle
  ^Rectangle [^Paint fill opacity ^Rect cursor-rect]
  (doto (Rectangle. (.x cursor-rect) (.y cursor-rect) (.w cursor-rect) (.h cursor-rect))
    (.setMouseTransparent true)
    (.setFill fill)
    (.setOpacity opacity)))

(defn- repaint-cursors! [{:keys [^Canvas canvas ^LayoutInfo layout color-scheme lines visible-cursors cursor-opacity] :as _cursor-repaint-info}]
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

(g/defnk produce-completion-context
  "Returns a map of completion-related information

  The map includes following keys (all required):
    :context           a string indicating a context in which the built-in
                       completions should be requested, e.g. for string
                       \"socket.d\" before cursor the context will be \"socket\"
    :query             a string used for filtering completions, e.g. for string
                       \"socket.d\" before cursor the query will be \"d\"
    :cursor-ranges     replacement ranges that should be used when
                       accepting the suggestion
    :request-cursor    a cursor identifying the place where the LSP completions
                       should be requested
    :insert-cursor     a cursor identifying the place where the LSP completions
                       are supposed to be inserted, used for validating if
                       complete LSP completion list should be refreshed
    :trigger           single-character trigger string before cursor (\"\\n\" on
                       the start of the line, even it's the first one)"
  [lines cursor-ranges]
  {:pre [(pos? (count cursor-ranges))]}
  (let [results (mapv (fn [^CursorRange cursor-range]
                        (let [suggestion-cursor (data/adjust-cursor lines (data/cursor-range-start cursor-range))
                              line (subs (lines (.-row suggestion-cursor)) 0 (.-col suggestion-cursor))
                              prefix (or (re-find #"[a-zA-Z_][a-zA-Z_0-9.]*$" line) "")
                              affected-cursor (if (pos? (data/compare-cursor-position
                                                          (.-from cursor-range)
                                                          (.-to cursor-range)))
                                                :to
                                                :from)
                              last-dot (string/last-index-of prefix ".")
                              context (if last-dot (subs prefix 0 last-dot) "")
                              query (if last-dot (subs prefix (inc ^long last-dot)) prefix)
                              replacement-range (update cursor-range affected-cursor update :col - (count query))]
                          [replacement-range context query]))
                      cursor-ranges)
        cursor (data/adjust-cursor lines (data/cursor-range-start (first cursor-ranges)))
        col (.-col cursor)]
    (let [[replacement-range context query] (first results)]
      {:context context
       :query query
       :cursor-ranges (mapv first results)
       :request-cursor cursor
       :insert-cursor (data/cursor-range-start replacement-range)
       :trigger (if (zero? col)
                  "\n"
                  (str (.charAt ^String (lines (.-row cursor)) (dec col))))})))

(g/defnk produce-visible-completion-ranges [lines layout completion-context]
  (data/visible-cursor-ranges lines layout (:cursor-ranges completion-context)))

(defn- find-tab-trigger-scope-region-at-cursor
  ^CursorRange [tab-trigger-scope-regions ^Cursor cursor]
  (some (fn [scope-region]
          (when (data/cursor-range-contains? scope-region cursor)
            scope-region))
        tab-trigger-scope-regions))

(defn- find-tab-trigger-region-at-cursor
  ^CursorRange [tab-trigger-scope-regions tab-trigger-regions-by-scope-id ^Cursor cursor]
  (when-some [scope-region (find-tab-trigger-scope-region-at-cursor tab-trigger-scope-regions cursor)]
    (some (fn [region]
            (when (data/cursor-range-contains? region cursor)
              region))
          (get tab-trigger-regions-by-scope-id (:id scope-region)))))

(g/defnk produce-suggested-completions [completions completion-context]
  (get completions (:context completion-context) []))

(g/defnk produce-suggested-choices
  [tab-trigger-scope-regions tab-trigger-regions-by-scope-id cursor-ranges]
  (let [choices-colls (into []
                            (comp
                              (map data/CursorRange->Cursor)
                              (keep (partial find-tab-trigger-region-at-cursor
                                             tab-trigger-scope-regions
                                             tab-trigger-regions-by-scope-id))
                              (keep :choices))
                            cursor-ranges)]
    (when (pos? (count choices-colls))
      (into []
            (comp
              cat
              (map code-completion/make))
            choices-colls))))

(g/defnk produce-completions-selected-index
  "Return a valid completions selected index or nil

  If completions are not empty, the result is guaranteed to be in the
  completion index range, otherwise the result is nil"
  [completions-selected-index completions-combined]
  (cond
    completions-selected-index
    (let [max-index (dec (count completions-combined))]
      (when-not (neg? max-index)
        (max 0 (min ^long completions-selected-index max-index))))

    (pos? (count completions-combined))
    0))

(g/defnk produce-completions-selection [completions-selected-index completions-combined]
  (when completions-selected-index
    (completions-combined completions-selected-index)))

(defn- completion-identifier [{:keys [display-string type]}]
  (pair
    (case type
      :message :message
      :property :property
      :event :event
      :snippet :snippet
      :text)
    display-string))

(defn- update-document-width [evaluation-context view-node]
  ;; This is called in response to changing layout-related properties on the
  ;; view-node. We only want to do something when we're connected to a resource
  ;; node. If we're not, the lines output will return nil.
  (when-some [lines (g/node-value view-node :lines evaluation-context)]
    (let [glyph-metrics (g/node-value view-node :glyph-metrics evaluation-context)
          tab-spaces (g/node-value view-node :tab-spaces evaluation-context)
          tab-stops (data/tab-stops glyph-metrics tab-spaces)
          document-width (data/max-line-width glyph-metrics tab-stops lines)]
      (g/set-property view-node :document-width document-width))))

(g/defnk produce-completions-combined [completions-built-in completions-lsp completion-context]
  (let [all-completions (into []
                              (comp cat (util/distinct-by completion-identifier))
                              [completions-built-in
                               (eduction
                                 (map-indexed #(vary-meta %2 assoc ::index %1))
                                 (:items completions-lsp))])
        query (:query completion-context)
        ;; We want to treat :text completions as "lower quality", only showing them
        ;; when there are no other types of completions
        qualified-completions (->> all-completions
                                   (filterv #(not (identical? :text (:type %))))
                                   (popup/fuzzy-option-filter-fn :name :display-string query))]
    (if (pos? (coll/bounded-count 1 qualified-completions))
      (vec qualified-completions)
      (->> all-completions
           (filterv #(identical? :text (:type %)))
           (popup/fuzzy-option-filter-fn :name :display-string query)
           vec))))

(g/defnk produce-completions-combined-ids [completions-combined]
  (mapv completion-identifier completions-combined))

;; region get/set properties

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

(defn- prelude-tx-data [view-node undo-grouping values-by-prop-kw]
  ;; Along with undo grouping info, we also keep track of when an action was
  ;; last performed in the document. We use this to stop the cursor from
  ;; blinking while typing or navigating.
  (into (operation-sequence-tx-data view-node undo-grouping)
        (when (or (contains? values-by-prop-kw :cursor-ranges)
                  (contains? values-by-prop-kw :lines))
          (g/set-property view-node :elapsed-time-at-last-action (or (g/user-data view-node :elapsed-time) 0.0)))))

;; -----------------------------------------------------------------------------

;; The functions that perform actions in the core.data module return maps of
;; properties that were modified by the operation. Some of these properties need
;; to be stored at various locations in order to be undoable, some are transient,
;; and so on. These two functions should be used to read and write these managed
;; properties at all times. Basically, get-property needs to be able to get any
;; property that is supplied to set-properties! in values-by-prop-kw.

(defn get-property
  "Gets the value of a property that is managed by the functions in the code.data module."
  ([view-node prop-kw]
   (g/with-auto-evaluation-context evaluation-context
     (get-property view-node prop-kw evaluation-context)))
  ([view-node prop-kw evaluation-context]
   (case prop-kw
     :invalidated-row
     (invalidated-row (:invalidated-rows (g/user-data view-node :canvas-repaint-info))
                      (g/node-value view-node :invalidated-rows evaluation-context))

     (g/node-value view-node prop-kw evaluation-context))))

(defn- region->prop-kw [region]
  (case (:type region)
    :diagnostic :diagnostics
    :hover (if (:hoverable region)
             :hover-showing-lsp-regions
             :hover-cursor-lsp-regions)
    :regions))

(defn- set-properties
  "Return transaction steps for the view changes"
  [view-node undo-grouping values-by-prop-kw]
  (let [resource-node (g/node-value view-node :resource-node)
        resource-node-type (g/node-type* resource-node)]
    (into (prelude-tx-data view-node undo-grouping values-by-prop-kw)
          (mapcat (fn [[prop-kw value]]
                    (case prop-kw
                      :cursor-ranges
                      (if (g/has-property? resource-node-type :cursor-ranges)
                        (g/set-property resource-node :cursor-ranges value)
                        (g/set-property view-node :fallback-cursor-ranges value))

                      :regions
                      (let [{:keys [diagnostics hover-showing-lsp-regions hover-cursor-lsp-regions regions]}
                            (group-by region->prop-kw value)]
                        (concat
                          (g/set-property view-node :hover-showing-lsp-regions hover-showing-lsp-regions)
                          (g/set-property view-node :hover-cursor-lsp-regions hover-cursor-lsp-regions)
                          (g/set-property view-node :diagnostics (or diagnostics []))
                          (g/set-property resource-node prop-kw (or regions []))))

                      ;; Several actions might have invalidated rows since
                      ;; we last produced syntax-info. We keep an ever-
                      ;; growing history of invalidated-rows. Then when
                      ;; producing syntax-info we find the first invalidated
                      ;; row by comparing the history of invalidated rows to
                      ;; what it was at the time of the last call. See the
                      ;; invalidated-row function for details.
                      :invalidated-row
                      (g/update-property resource-node :invalidated-rows conj value)

                      ;; The :indent-type output in the resource node is
                      ;; cached, but reads from disk unless a value exists
                      ;; for the :modified-indent-type property.
                      :indent-type
                      (g/set-property resource-node :modified-indent-type value)

                      ;; The :lines output in the resource node is uncached.
                      ;; It reads from disk unless a value exists for the
                      ;; :modified-lines property. This means only modified
                      ;; or currently open files are kept in memory.
                      :lines
                      (g/set-property resource-node :modified-lines value)

                      ;; All other properties are set on the view node.
                      (g/set-property view-node prop-kw value))))
          values-by-prop-kw)))

(defn- set-resource-properties
  "Return transaction steps eduction for the editable resource node changes"
  [resource-node values-by-prop-kw]
  (e/mapcat
    (fn [[prop-kw value]]
      (case prop-kw
        :cursor-ranges (g/set-property resource-node :cursor-ranges value)
        :regions (g/set-property resource-node :regions (filterv #(= :regions (region->prop-kw %)) value))
        :invalidated-row (g/update-property resource-node :invalidated-rows conj value)
        :lines (g/set-property resource-node :modified-lines value)
        :indent-type (g/set-property resource-node :modified-indent-type value)))
    values-by-prop-kw))

(defn set-properties!
  "Sets values of properties that are managed by the functions in the code.data module.
  Returns true if any property changed, false otherwise."
  [view-node undo-grouping values-by-prop-kw]
  (if (empty? values-by-prop-kw)
    false
    (do (g/transact
          (set-properties view-node undo-grouping values-by-prop-kw))
        true)))

;; endregion

;; region rename

(defn- handle-rename-key-pressed [view-node text rename-cursor-range swap-state ^KeyEvent e]
  (when (= KeyCode/ENTER (.getCode e))
    (.consume e)
    (g/with-auto-evaluation-context evaluation-context
      (let [resource-node (g/node-value view-node :resource-node evaluation-context)
            lsp (lsp/get-node-lsp (:basis evaluation-context) resource-node)]
        (swap-state assoc :done true)
        (lsp/rename
          lsp
          rename-cursor-range
          text
          (fn on-rename-response [resource->ascending-cursor-ranges-and-replacements]
            (ui/run-later
              (some->
                (g/with-auto-evaluation-context evaluation-context
                  (when (identical? rename-cursor-range (get-property view-node :rename-cursor-range evaluation-context))
                    (let [resource->view (->> (get-property view-node :open-views evaluation-context)
                                              (e/keep
                                                (fn [[view {:keys [resource]}]]
                                                  (when (g/node-kw-instance? (:basis evaluation-context) ::CodeEditorView view)
                                                    [resource view])))
                                              (into {}))
                          project (get-property view-node :project evaluation-context)]
                      (into (set-properties view-node nil {:rename-cursor-range nil})
                            (mapcat
                              (fn [[resource ascending-cursor-ranges-and-replacements]]
                                (when-let [resource-node (project/get-resource-node project resource evaluation-context)]
                                  (when (g/node-instance? (:basis evaluation-context) r/CodeEditorResourceNode resource-node)
                                    (if-let [view (resource->view resource)]
                                      (set-properties
                                        view nil
                                        (data/apply-edits
                                          (get-property view :lines evaluation-context)
                                          (get-property view :regions evaluation-context)
                                          (get-property view :cursor-ranges evaluation-context)
                                          ascending-cursor-ranges-and-replacements
                                          (get-property view :layout evaluation-context)))
                                      (set-resource-properties
                                        resource-node
                                        (data/apply-edits
                                          (g/node-value resource-node :lines evaluation-context)
                                          (g/node-value resource-node :regions evaluation-context)
                                          (g/node-value resource-node :cursor-ranges evaluation-context)
                                          ascending-cursor-ranges-and-replacements)))))))
                            resource->ascending-cursor-ranges-and-replacements))))
                g/transact))))))))

(fxui/defc rename-popup-view
  {:compose [{:fx/type fx/ext-state
              :initial-state {:text (data/cursor-range-text
                                      (:lines (:canvas-repaint-info props))
                                      (:rename-cursor-range props))
                              :done false}}]}
  [{:keys [canvas-repaint-info _node-id rename-cursor-range state swap-state]}]
  (let [{:keys [^Canvas canvas layout lines ^Font font]} canvas-repaint-info
        {:keys [text done]} state
        ^Rect r (->> rename-cursor-range
                     (data/adjust-cursor-range lines)
                     (data/cursor-range-rects layout lines)
                     first)
        anchor (.localToScreen canvas
                               (min (max 0.0 (.-x r)) (.getWidth canvas))
                               (min (max 0.0 (.-y r)) (.getHeight canvas)))
        padding (+
                  ;; text field padding
                  4.0
                  ;; border width
                  1.0)]
    {:fx/type fxui/with-popup-window
     :desc {:fx/type fxui/ext-value :value canvas}
     :popup {:fx/type fx.popup/lifecycle
             :on-hidden (fn [_] (set-properties! _node-id nil {:rename-cursor-range nil}))
             :showing true
             :on-window true
             :anchor-x (- (.getX anchor) padding 12.0) ;; adjust for shadow
             :anchor-y (- (.getY anchor) padding 2.0) ;; adjust for shadow
             :auto-hide true
             :consume-auto-hiding-events false
             :content [{:fx/type fx.stack-pane/lifecycle
                        :stylesheets [(str (io/resource "editor.css"))]
                        :children [{:fx/type fx.region/lifecycle
                                    :style-class "completion-popup-background"}
                                   {:fx/type fx.text-field/lifecycle
                                    :editable (not done)
                                    :style-class "rename-text-field"
                                    :font font
                                    :min-width (+ (.-w r) (* 2.0 padding))
                                    :pref-width (-> (Text. text)
                                                    (doto (.setFont font))
                                                    .getLayoutBounds
                                                    .getWidth
                                                    (+ (* 2.0 padding)))
                                    :on-key-pressed #(handle-rename-key-pressed _node-id text rename-cursor-range swap-state %)
                                    :text text
                                    :on-text-changed #(swap-state assoc :text %)}]}]}}))

;; endregion

(g/defnode CodeEditorView
  (inherits view/WorkbenchView)

  ;; For files that can be edited as code, the `CursorRanges` that keep track of
  ;; text selections are stored in a `cursor-ranges` property alongside the
  ;; `modified-lines` on the edited resource node. This is necessary to ensure
  ;; the lines and the selections stay in sync when undoing.
  ;;
  ;; However, we also use the `CodeEditorView` to open other resources as text.
  ;; In that situation, you cannot edit the text, and we don't need the text
  ;; selection to stay in sync with the file contents.
  ;;
  ;; To avoid having to pollute every resource node type with a `cursor-ranges`
  ;; property, we store the text selections in this `fallback-cursor-ranges`
  ;; property on the `CodeEditorView` itself when viewing these resources.
  (property fallback-cursor-ranges r/CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))

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
  (property completion-trigger-characters g/Any (default #{}) (dynamic visible (g/constantly false)))
  (output completion-trigger-characters g/Any :cached (g/fnk [completion-trigger-characters grammar]
                                                        (into #{}
                                                              (comp
                                                                cat
                                                                (remove (:ignored-completion-trigger-characters grammar #{})))
                                                              [completion-trigger-characters
                                                               (:completion-trigger-characters grammar)])))
  (property diagnostics r/Regions (default []) (dynamic visible (g/constantly false)))
  (property document-width g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property color-scheme ColorScheme (dynamic visible (g/constantly false)))
  (property elapsed-time-at-last-action g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property grammar g/Any (dynamic visible (g/constantly false)))
  (property gutter-view GutterView (dynamic visible (g/constantly false)))
  (property cursor-opacity g/Num (default 1.0) (dynamic visible (g/constantly false)))
  (property resize-reference ResizeReference (default :top) (dynamic visible (g/constantly false)))
  (property scroll-x g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-y g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property gesture-start GestureInfo (dynamic visible (g/constantly false)))
  (property highlighted-find-term g/Str (default "") (dynamic visible (g/constantly false)))
  (property hovered-element HoveredElement (dynamic visible (g/constantly false)))
  (property hovered-row g/Num (default 0) (dynamic visible (g/constantly false)))
  (property edited-breakpoint r/Region (dynamic visible (g/constantly false)))
  (property find-case-sensitive? g/Bool (dynamic visible (g/constantly false)))
  (property find-whole-word? g/Bool (dynamic visible (g/constantly false)))
  (property focus-state FocusState (default :not-focused) (dynamic visible (g/constantly false)))

  (property completions-showing g/Bool (default false) (dynamic visible (g/constantly false)))
  (property completions-doc g/Bool (default false) (dynamic visible (g/constantly false)))
  (property completions-insert-cursor g/Any (dynamic visible (g/constantly false)))
  (property completions-built-in g/Any (default []) (dynamic visible (g/constantly false)))
  (property completions-lsp g/Any)
  (property completions-selected-index g/Any (dynamic visible (g/constantly false)))
  (property completions-previous-combined-ids g/Any (dynamic visible (g/constantly false)))

  ;; the cursor position for which we show the hover.
  (property hover-showing-cursor g/Any (dynamic visible (g/constantly false)))
  ;; instance of `(ui/->future)`, delayed refresh check
  (property hover-request g/Any (dynamic visible (g/constantly false)))
  ;; current mouse position as a cursor, nil if not hovering a character
  (property hover-cursor g/Any (dynamic visible (g/constantly false)))
  ;; hover regions for the current cursor that we got from LSP, not shown (:hoverable set to false)
  (property hover-cursor-lsp-regions g/Any (dynamic visible (g/constantly false)))
  ;; hover regions for the showing cursor, visible
  (property hover-showing-lsp-regions g/Any (dynamic visible (g/constantly false)))
  ;; we need to track if we are hovering the popup because on Windows we receive
  ;; both popup and canvas mouse events...
  (property hover-mouse-over-popup g/Bool (default false) (dynamic visible (g/constantly false)))
  ;; all displayed hovered regions
  (output hover-showing-regions g/Any :cached (g/fnk [visible-regions hover-showing-cursor]
                                                (when hover-showing-cursor
                                                  (->> visible-regions
                                                       (e/filter :hoverable)
                                                       (e/filter #(data/cursor-range-contains-exclusive? % hover-showing-cursor))
                                                       vec
                                                       coll/not-empty))))
  ;; when showing a hover, this is a region that is considered a single hovered thing
  (output hover-showing-region g/Any :cached (g/fnk [hover-showing-regions ^Cursor hover-showing-cursor]
                                               (when hover-showing-cursor
                                                 (or (->> hover-showing-regions
                                                          (reduce
                                                            (fn [a b]
                                                              (if (nil? a)
                                                                b
                                                                (if-let [ret (data/cursor-range-intersection a b)]
                                                                  ret
                                                                  (reduced nil))))
                                                            nil))
                                                     (data/->CursorRange
                                                       hover-showing-cursor
                                                       (data/->Cursor (.-row hover-showing-cursor)
                                                                      (inc (.-col hover-showing-cursor))))))))

  ;; prepared rename range
  (property rename-cursor-range g/Any (dynamic visible (g/constantly false)))
  (output rename-view g/Any :cached (g/fnk [_node-id rename-cursor-range canvas-repaint-info :as props]
                                      (assoc props :fx/type rename-popup-view)))

  (output completions-selected-index g/Any :cached produce-completions-selected-index) ;; either in completions index range or nil
  (output completions-selection g/Any produce-completions-selection)
  (output completions-combined g/Any :cached produce-completions-combined)
  (output completions-combined-ids g/Any :cached produce-completions-combined-ids)

  (output completions-shortcut-text g/Any :cached (g/fnk [keymap]
                                                    (keymap/display-text keymap :code.show-completions nil)))

  (property font-name g/Str (default "Dejavu Sans Mono")
            (set (fn [evaluation-context self _old-value _new-value]
                   (update-document-width evaluation-context self))))
  (property font-size g/Num
            (default (g/constantly default-font-size))
            (set (fn [evaluation-context self _old-value _new-value]
                   (update-document-width evaluation-context self))))
  (property line-height-factor g/Num (default 1.0))
  (property visible-indentation-guides? g/Bool (default false))
  (property visible-minimap? g/Bool (default false))
  (property visible-whitespace VisibleWhitespace (default :none))

  ;; Inputs from CodeEditorResourceNode.
  (input completions g/Any :substitute {})
  (input cursor-ranges r/CursorRanges :substitute nil) ; Fall back on fallback-cursor-ranges.
  (input indent-type r/IndentType :substitute r/default-indent-type)
  (input invalidated-rows r/InvalidatedRows :substitute [])
  (input lines types/Lines :substitute [""])
  (input regions r/Regions :substitute [])

  ;; Inputs from elsewhere.
  (input open-views g/Any) ;; open views from app-view
  (input project g/NodeID) ;; used for completions doc popup, e.g. for opening defold:// URIs
  (input debugger-execution-locations g/Any)
  (input keymap g/Any)

  (output completion-context g/Any :cached produce-completion-context)
  (output cursor-ranges r/CursorRanges (g/fnk [cursor-ranges fallback-cursor-ranges]
                                         (or cursor-ranges fallback-cursor-ranges)))
  (output visible-completion-ranges g/Any :cached produce-visible-completion-ranges)
  (output suggested-completions g/Any produce-suggested-completions)
  (output suggested-choices g/Any :cached produce-suggested-choices)
  ;; We cache the lines in the view instead of the resource node, since the
  ;; resource node will read directly from disk unless edits have been made.
  (output lines types/Lines :cached (gu/passthrough lines))
  (output regions r/Regions :cached (g/fnk [regions diagnostics hover-cursor-lsp-regions hover-showing-lsp-regions]
                                      (vec (sort (into regions cat [diagnostics hover-cursor-lsp-regions hover-showing-lsp-regions])))))
  (output indent-type r/IndentType produce-indent-type)
  (output indent-string g/Str produce-indent-string)
  (output tab-spaces g/Num produce-tab-spaces)
  (output indent-level-pattern Pattern :cached produce-indent-level-pattern)
  (output font Font :cached produce-font)
  (output glyph-metrics data/GlyphMetrics :cached produce-glyph-metrics)
  (output gutter-metrics GutterMetrics :cached produce-gutter-metrics)
  (output layout LayoutInfo :cached produce-layout)
  (output matching-braces MatchingBraces :cached produce-matching-braces)
  (output tab-trigger-scope-regions r/Regions :cached produce-tab-trigger-scope-regions)
  (output tab-trigger-regions-by-scope-id r/RegionGrouping :cached produce-tab-trigger-regions-by-scope-id)
  (output visible-cursors r/Cursors :cached produce-visible-cursors)
  (output visible-cursor-ranges r/CursorRanges :cached produce-visible-cursor-ranges)
  (output visible-regions r/Regions :cached produce-visible-regions)
  (output visible-matching-braces r/CursorRanges :cached produce-visible-matching-braces)
  (output cursor-range-draw-infos CursorRangeDrawInfos :cached produce-cursor-range-draw-infos)
  (output minimap-glyph-metrics data/GlyphMetrics :cached produce-minimap-glyph-metrics)
  (output minimap-layout LayoutInfo :cached produce-minimap-layout)
  (output minimap-cursor-range-draw-infos CursorRangeDrawInfos :cached produce-minimap-cursor-range-draw-infos)
  (output execution-markers r/Regions :cached produce-execution-markers)
  (output canvas-repaint-info g/Any :cached produce-canvas-repaint-info)
  (output cursor-repaint-info g/Any :cached produce-cursor-repaint-info))

(defn- mouse-button [^MouseEvent event]
  (condp = (.getButton event)
    MouseButton/NONE nil
    MouseButton/PRIMARY :primary
    MouseButton/SECONDARY :secondary
    MouseButton/MIDDLE :middle
    MouseButton/BACK :back
    MouseButton/FORWARD :forward))


;; -----------------------------------------------------------------------------
;; Code completion
;; -----------------------------------------------------------------------------

(defn- cursor-bottom
  ^Point2D [^LayoutInfo layout lines ^Cursor adjusted-cursor]
  (let [^Rect r (data/cursor-rect layout lines adjusted-cursor)]
    (Point2D. (.x r) (+ (.y r) (.h r)))))

(defn- pref-suggestions-popup-position
  ^Point2D [^Canvas canvas width height ^Point2D cursor-bottom]
  (Utils/pointRelativeTo canvas width height HPos/CENTER VPos/CENTER (.getX cursor-bottom) (.getY cursor-bottom) true))

(defn- resolve-selected-completion! [view-node]
  (g/with-auto-evaluation-context evaluation-context
    (when (and (get-property view-node :completions-showing evaluation-context)
               (get-property view-node :completions-doc evaluation-context))
      (when-let [completion (get-property view-node :completions-selection evaluation-context)]
        (when-let [index (::index (meta completion))]
          (when-not (::resolved (meta completion))
            (let [resource-node (get-property view-node :resource-node evaluation-context)
                  lsp (lsp/get-node-lsp (:basis evaluation-context) resource-node)]
              (lsp/resolve-completion!
                lsp completion
                (fn [resolved-completion]
                  (ui/run-later
                    (g/with-auto-evaluation-context evaluation-context
                      (when-let [completions-lsp (get-property view-node :completions-lsp evaluation-context)]
                        (when (= completion (get (:items completions-lsp) index))
                          (set-properties!
                            view-node nil
                            {:completions-lsp
                             (update completions-lsp :items assoc index
                                     (vary-meta resolved-completion assoc ::resolved true))}))))))))))))))

(defn- refresh-completion-selected-index! [view-node]
  (g/with-auto-evaluation-context evaluation-context
    (let [old (get-property view-node :completions-previous-combined-ids evaluation-context)
          new (get-property view-node :completions-combined-ids evaluation-context)]
      (when-not (= old new)
        (set-properties! view-node nil
                         {:completions-previous-combined-ids new
                          :completions-selected-index nil})
        (resolve-selected-completion! view-node)))))

(defn- on-lsp-completions [view-node insert-cursor request-cursor completions]
  (ui/run-later
    (g/with-auto-evaluation-context evaluation-context
      (when (and (get-property view-node :completions-showing evaluation-context)
                 (= insert-cursor (get-property view-node :completions-insert-cursor evaluation-context)))
        (set-properties! view-node nil
                         {:completions-lsp (assoc completions :request-cursor request-cursor)})
        (refresh-completion-selected-index! view-node)))))

(defn- show-suggestions! [view-node & {:keys [explicit typed] :or {explicit false}}]
  ;; Some notes about LSP completion requests and completeness:
  ;; LSP completion responses can signal if the results they respond with are
  ;; complete or not. If results are not complete, further typing for this
  ;; completion context should request completions again. If the results are
  ;; complete, we don't need to request completions in this context, since no
  ;; new results can appear. This is done to make completions calculation and
  ;; transfer from the language server process to the language client process
  ;; faster.
  ;; We need to store the cursor position that corresponds to LSP completion
  ;; results to determine if we should request completions again or not:
  ;; - if we continued typing after we got a complete response (i.e. we narrow
  ;;   down the completion list), then we don't request completions again. In
  ;;   this scenario, our cursor that could request the completions is located
  ;;   after the request cursor we used to get the complete responses
  ;; - if instead of typing we start deleting characters, we need to request
  ;;   completions again as soon as we move the cursor before the cursor that
  ;;   responded with complete results, since results for less narrow completion
  ;;   query might be incomplete
  ;; This behavior is supported with :request-cursor.
  ;; Additionally, some typing might change the completion context altogether.
  ;; For example, typing "." after "math" always requires a completion request,
  ;; since now we complete need completions in the context of the `math` table.
  ;; So, in addition to :request-cursor, we also keep track of :insert-cursor,
  ;; which is a cursor that identifies the completion context as a place where
  ;; we want to insert the completions.
  (g/with-auto-evaluation-context evaluation-context
    (let [showing (get-property view-node :completions-showing evaluation-context)]
      ;; short circuit: toggle doc popup
      (if (and explicit showing)
        (do (set-properties! view-node nil {:completions-doc (not (get-property view-node :completions-doc evaluation-context))})
            (resolve-selected-completion! view-node))
        (let [{:keys [request-cursor insert-cursor]} (get-property view-node :completion-context evaluation-context)
              completions-insert-cursor (get-property view-node :completions-insert-cursor evaluation-context)
              completions-lsp (get-property view-node :completions-lsp evaluation-context)
              new-properties
              ;; perform a completion request?
              (if (or (and explicit (not showing))
                      (and (not explicit)
                           (or
                             ;; refresh both built-in and lsp:
                             (not showing)
                             (not= insert-cursor completions-insert-cursor)
                             ;; refresh lsp:
                             (not completions-lsp)
                             (not (:complete completions-lsp))
                             (neg? (compare request-cursor (:request-cursor completions-lsp))))))
                (if-let [choices (get-property view-node :suggested-choices evaluation-context)]
                  ;; if there are choices, we use them as completion items
                  {:completions-showing true
                   :completions-insert-cursor insert-cursor
                   :completions-built-in choices
                   :completions-lsp {:items [] :complete true :request-cursor request-cursor}}
                  ;; we have no choices, either refresh everything or only LSP
                  (let [resource-node (get-property view-node :resource-node evaluation-context)
                        resource (g/node-value resource-node :resource evaluation-context)
                        lsp (lsp/get-node-lsp (:basis evaluation-context) resource-node)
                        context (cond
                                  typed
                                  {:trigger-kind :trigger-character
                                   :trigger-character ({"\r" "\n"} typed typed)}

                                  (and completions-lsp
                                       (not (:complete completions-lsp)))
                                  {:trigger-kind :trigger-for-incomplete-completions}

                                  :else
                                  {:trigger-kind :invoked})]
                    (lsp/request-completions!
                      lsp resource request-cursor context
                      #(on-lsp-completions view-node insert-cursor request-cursor %))
                    (cond->
                      {:completions-showing true
                       :completions-insert-cursor insert-cursor
                       :completions-lsp {:items [] :complete false :request-cursor request-cursor}}
                      (not= insert-cursor completions-insert-cursor)
                      (assoc :completions-built-in (get-property view-node :suggested-completions evaluation-context)))))
                ;; view already open, completions present: only filter/reset index
                {})]
          (set-properties!
            view-node
            nil
            (-> new-properties
                ;; lines and cursor-ranges are needed to frame the cursor
                (assoc :lines (get-property view-node :lines evaluation-context)
                       :cursor-ranges (get-property view-node :cursor-ranges evaluation-context))
                (data/frame-cursor (get-property view-node :layout evaluation-context))
                (dissoc :lines :cursor-ranges)))
          (refresh-completion-selected-index! view-node))))))

(defn get-valid-syntax-info
  "Get syntax info valid up till target row (1-indexed)

  Returns empty vector if there is no grammar

  Args:
    resource-node          resource node
    canvas-repaint-info    canvas repaint info
    target-row             1-indexed row in the document"
  [resource-node canvas-repaint-info target-row]
  (let [{:keys [grammar lines]} canvas-repaint-info
        invalidated-rows (:invalidated-rows canvas-repaint-info)
        ;; Read-only views don't have invalidated rows at all: we detect changes
        ;; in them by comparing lines hashes. When they change, we refresh the
        ;; syntax from the start.
        read-only-view (nil? invalidated-rows)
        lines-hash (when read-only-view (hash lines))
        syntax-info
        (-> (if (nil? grammar)
              []
              (if-some [prev-syntax-info (g/user-data resource-node :syntax-info)]
                (let [invalidated-syntax-info
                      (if-some [invalidated-row (if read-only-view
                                                  (when-not (= lines-hash (:lines-hash (meta prev-syntax-info)))
                                                    0)
                                                  (invalidated-row (:invalidated-rows (meta prev-syntax-info))
                                                                   invalidated-rows))]
                        (data/invalidate-syntax-info prev-syntax-info invalidated-row (count lines))
                        prev-syntax-info)]
                  (data/ensure-syntax-info invalidated-syntax-info target-row lines grammar))
                (data/ensure-syntax-info [] target-row lines grammar)))
            (vary-meta assoc :invalidated-rows invalidated-rows)
            (cond-> read-only-view (vary-meta assoc :lines-hash lines-hash)))]
    (g/user-data! resource-node :syntax-info syntax-info)
    syntax-info))

(defn- get-current-syntax-info [resource-node]
  (or (g/user-data resource-node :syntax-info) []))

(defn- syntax-scope-before-cursor [view-node ^Cursor cursor evaluation-context]
  (if-let [syntax-info (coll/not-empty
                         (get-valid-syntax-info
                           (get-property view-node :resource-node evaluation-context)
                           (get-property view-node :canvas-repaint-info evaluation-context)
                           (inc (.-row cursor))))]
    (data/syntax-scope-before-cursor syntax-info (get-property view-node :grammar evaluation-context) cursor)
    "source"))

(defn- implies-completions?
  ([view-node]
   (g/with-auto-evaluation-context evaluation-context
     (implies-completions? view-node evaluation-context)))
  ([view-node evaluation-context]
   (let [{:keys [trigger request-cursor]} (get-property view-node :completion-context evaluation-context)
         trigger-characters (get-property view-node :completion-trigger-characters evaluation-context)
         syntax-scope (syntax-scope-before-cursor view-node request-cursor evaluation-context)]
     (boolean (and (not (contains? #{"\n" "\t" " "} trigger))
                   (or (re-matches #"[a-zA-Z_]" trigger)
                       (contains? trigger-characters trigger))
                   (not (string/starts-with? syntax-scope "punctuation.definition.string.end"))
                   (not (string/starts-with? syntax-scope "comment")))))))

(defn- hide-suggestions! [view-node]
  (set-properties! view-node nil
                   {:completions-showing false
                    :completions-selected-index nil
                    :completions-built-in []
                    :completions-insert-cursor nil
                    :completions-lsp nil
                    :completions-previous-combined-ids nil}))

(defn- hide-hover! [view-node]
  (set-properties! view-node nil {:hover-showing-cursor nil :hover-showing-lsp-regions nil :hover-mouse-over-popup false}))

(defn- suggestions-shown? [view-node]
  (g/with-auto-evaluation-context evaluation-context
    (get-property view-node :completions-showing evaluation-context)))

(defn- suggestions-visible? [view-node]
  (g/with-auto-evaluation-context evaluation-context
    (and (get-property view-node :completions-showing evaluation-context)
         (pos? (count (get-property view-node :completions-combined evaluation-context))))))

(defn- hover-visible?
  ([view-node]
   (g/with-auto-evaluation-context evaluation-context
     (hover-visible? view-node evaluation-context)))
  ([view-node evaluation-context]
   (some? (get-property view-node :hover-showing-regions evaluation-context))))

(defn- selected-suggestion [view-node]
  (g/with-auto-evaluation-context evaluation-context
    (when (get-property view-node :completions-showing evaluation-context)
      (get-property view-node :completions-selection evaluation-context))))

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
    (:tab-trigger-scope :tab-trigger-word :tab-trigger-exit) true
    false))

(defn- exit-tab-trigger! [view-node]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (when (in-tab-trigger? view-node)
    (set-properties! view-node :navigation
                     {:regions (into []
                                     (remove tab-trigger-related-region?)
                                     (get-property view-node :regions))})))

(defn- find-closest-tab-trigger-regions
  [search-direction tab-trigger-scope-regions tab-trigger-regions-by-scope-id ^CursorRange cursor-range]
  ;; NOTE: Cursor range collections are assumed to be in ascending order.
  (let [cursor-range->cursor (case search-direction :prev data/cursor-range-start :next data/cursor-range-end)
        from-cursor (cursor-range->cursor cursor-range)]
    (if-some [scope-region (find-tab-trigger-scope-region-at-cursor tab-trigger-scope-regions from-cursor)]
      ;; The cursor range is inside a tab trigger scope region.
      ;; Try to find the next word region inside the scope region.
      (let [scope-id (:id scope-region)
            selected-tab-region (find-tab-trigger-region-at-cursor
                                  tab-trigger-scope-regions
                                  tab-trigger-regions-by-scope-id
                                  from-cursor)
            selected-index (if selected-tab-region
                             (case (:type selected-tab-region)
                               :tab-trigger-word (:index selected-tab-region)
                               :tab-trigger-exit ##Inf)
                             (case search-direction
                               :next ##-Inf
                               :prev ##Inf))
            tab-trigger-regions (get tab-trigger-regions-by-scope-id scope-id)
            index->tab-triggers (->> tab-trigger-regions
                                     (eduction
                                       (filter #(= :tab-trigger-word (:type %))))
                                     (util/group-into (sorted-map) [] :index))]
        (or
          ;; find next region by index
          (when-let [[_ regions] (first (case search-direction
                                          :next (subseq index->tab-triggers > selected-index)
                                          :prev (rsubseq index->tab-triggers < selected-index)))]
            {:regions (mapv data/sanitize-cursor-range regions)})
          ;; no further regions found: exit if moving forward
          (when (= :next search-direction)
            {:regions (or
                        ;; if there are explicit exit regions, use those
                        (not-empty
                          (into []
                                (keep
                                  #(when (= :tab-trigger-exit (:type %))
                                     (data/sanitize-cursor-range %)))
                                tab-trigger-regions))
                        ;; otherwise, exit to the end of the scope
                        [(-> scope-region
                             data/cursor-range-end-range
                             data/sanitize-cursor-range)])
             :exit (into [scope-region] tab-trigger-regions)})

          ;; fallback: return the same range
          {:regions [cursor-range]}))

      ;; The cursor range is outside any tab trigger scope ranges. Return unchanged.
      {:regions [cursor-range]})))

(defn- select-closest-tab-trigger-region! [search-direction view-node]
  (when-some [tab-trigger-scope-regions (not-empty (get-property view-node :tab-trigger-scope-regions))]
    (let [tab-trigger-regions-by-scope-id (get-property view-node :tab-trigger-regions-by-scope-id)
          find-closest-tab-trigger-regions (partial find-closest-tab-trigger-regions
                                                    search-direction
                                                    tab-trigger-scope-regions
                                                    tab-trigger-regions-by-scope-id)
          cursor-ranges (get-property view-node :cursor-ranges)
          new-cursor-ranges+exits (mapv find-closest-tab-trigger-regions cursor-ranges)
          removed-regions (into #{} (mapcat :exit) new-cursor-ranges+exits)
          new-cursor-ranges (vec (sort (into #{} (mapcat :regions) new-cursor-ranges+exits)))
          regions (get-property view-node :regions)
          new-regions (into [] (remove removed-regions) regions)]
      (set-properties! view-node :selection
                       (cond-> {:cursor-ranges new-cursor-ranges}

                               (not= (count regions) (count new-regions))
                               (assoc :regions new-regions))))))

(def ^:private prev-tab-trigger! #(select-closest-tab-trigger-region! :prev %))
(def ^:private next-tab-trigger! #(select-closest-tab-trigger-region! :next %))

(defn- accept-suggestion!
  ([view-node]
   (when-let [completion (selected-suggestion view-node)]
     (accept-suggestion! view-node (code-completion/insertion completion))))
  ([view-node insertion]
   (let [indent-level-pattern (get-property view-node :indent-level-pattern)
         indent-string (get-property view-node :indent-string)
         grammar (get-property view-node :grammar)
         lines (get-property view-node :lines)
         regions (get-property view-node :regions)
         layout (get-property view-node :layout)
         explicit-cursor-range (:cursor-range insertion)
         implied-cursor-ranges (:cursor-ranges (get-property view-node :completion-context))
         replacement-cursor-ranges (if explicit-cursor-range [explicit-cursor-range] implied-cursor-ranges)
         {:keys [insert-string exit-ranges tab-triggers additional-edits]} insertion
         replacement-lines (split-lines insert-string)
         replacement-line-counts (mapv count replacement-lines)
         insert-string-index->replacement-lines-cursor
         (fn insert-string-index->replacement-lines-cursor [^long i]
           (loop [row 0
                  col i]
             (let [^long row-len (replacement-line-counts row)]
               (if (< row-len col)
                 (recur (inc row) (dec (- col row-len)))
                 (data/->Cursor row col)))))
         insert-string-index-range->cursor-range
         (fn insert-string-index-range->cursor-range [[from to]]
           (data/->CursorRange
             (insert-string-index->replacement-lines-cursor from)
             (insert-string-index->replacement-lines-cursor to)))
         ;; cursor ranges and replacements
         splices (mapv
                   (fn [replacement-range]
                     (let [scope-id (gensym "tab-scope")
                           introduced-regions
                           (-> [(assoc (insert-string-index-range->cursor-range [0 (count insert-string)])
                                  :type :tab-trigger-scope
                                  :id scope-id)]
                               (cond->
                                 tab-triggers
                                 (into
                                   (comp
                                     (map-indexed
                                       (fn [i tab-trigger]
                                         (let [tab-trigger-contents (assoc (dissoc tab-trigger :ranges)
                                                                      :type :tab-trigger-word
                                                                      :scope-id scope-id
                                                                      :index i)]
                                           (eduction
                                             (map (fn [range]
                                                    (conj (insert-string-index-range->cursor-range range)
                                                          tab-trigger-contents)))
                                             (:ranges tab-trigger)))))
                                     cat)
                                   tab-triggers)

                                 exit-ranges
                                 (into
                                   (map (fn [index-range]
                                          (assoc (insert-string-index-range->cursor-range index-range)
                                            :type :tab-trigger-exit
                                            :scope-id scope-id)))
                                   exit-ranges))
                               sort
                               vec)]
                       [replacement-range replacement-lines introduced-regions]))
                   replacement-cursor-ranges)
         tab-scope-ids (into #{}
                             (comp
                               (mapcat (fn [[_ _ regions]]
                                         regions))
                               (keep (fn [{:keys [type] :as region}]
                                       (when (= :tab-trigger-scope type)
                                         (:id region)))))
                             splices)
         introduced-region? (fn [{:keys [type] :as region}]
                              (case type
                                :tab-trigger-scope (contains? tab-scope-ids (:id region))
                                (:tab-trigger-word :tab-trigger-exit) (contains? tab-scope-ids (:scope-id region))
                                false))
         all-splices (if (or additional-edits explicit-cursor-range)
                       (let [;; Add additional text edits. Per LSP specification,
                             ;; these text edit must not overlap with neither the
                             ;; main edit nor themselves, we will trust the language
                             ;; servers for now
                             splices (if additional-edits
                                       (into splices (map (juxt :cursor-range (comp split-lines :value))) additional-edits)
                                       splices)
                             ;; if the insertion specifies an explicit cursor range,
                             ;; we want to clear the part of the original replacement
                             ;; range that does not overlap with all the other splices
                             ;; since it's considered a left-over query for the
                             ;; completion popup
                             clear-cursor-ranges (when explicit-cursor-range
                                                   (let [clear-cursor-range (first implied-cursor-ranges)]
                                                     (reduce
                                                       (fn [acc [other-cursor-range]]
                                                         (into []
                                                               (mapcat #(data/cursor-range-differences % other-cursor-range))
                                                               acc))
                                                       [clear-cursor-range]
                                                       splices)))
                             splices (if clear-cursor-ranges
                                       (into splices
                                             (map (fn [clear-cursor-range]
                                                    [clear-cursor-range [""]]))
                                             clear-cursor-ranges)
                                       splices)]
                         (vec (sort-by first splices)))
                       splices)
         props (data/replace-typed-chars indent-level-pattern indent-string grammar lines regions layout all-splices)]
     (when (some? props)
       (hide-hover! view-node)
       (hide-suggestions! view-node)
       (let [cursor-ranges (:cursor-ranges props)
             regions (:regions props)
             new-cursor-ranges (cond
                                 tab-triggers
                                 (into []
                                       (comp
                                         (filter #(and (= :tab-trigger-word (:type %))
                                                       (zero? ^long (:index %))
                                                       (tab-scope-ids (:scope-id %))))
                                         (map data/sanitize-cursor-range))
                                       regions)

                                 exit-ranges
                                 (into []
                                       (comp
                                         (filter #(and (= :tab-trigger-exit (:type %))
                                                       (tab-scope-ids (:scope-id %))))
                                         (map data/sanitize-cursor-range))
                                       regions)

                                 :else
                                 (mapv data/cursor-range-end-range cursor-ranges))]
         (set-properties! view-node nil
                          (-> props
                              (assoc :cursor-ranges new-cursor-ranges
                                     :regions (if tab-triggers
                                                ;; remove other tab-trigger-related regions
                                                (into []
                                                      (remove #(and (tab-trigger-related-region? %)
                                                                    (not (introduced-region? %))))
                                                      regions)
                                                ;; no triggers: remove introduced regions
                                                (into [] (remove introduced-region?) regions)))
                              (data/frame-cursor layout))))))))

(def ^:private ext-with-list-view-props
  (fx/make-ext-with-props
    {:items (prop/make
              (mutator/setter (fn [^ListView view [_key items]]
                                ;; force items change on key change since text
                                ;; runs are in meta (equality not affected), but
                                ;; we want to show them when they change anyway
                                (.setAll (.getItems view) ^Collection items)))
              fx.lifecycle/scalar)
     :selected-index (prop/make
                       (mutator/setter
                         (fn [^ListView view [_key index]]
                           (.clearAndSelect (.getSelectionModel view) (or index 0))
                           (when-not index (.scrollTo view 0))))
                       fx.lifecycle/scalar)}))

(def ^:private ^:const completion-type-icon-size 12.0)
(def ^:private ^:const completion-icon-text-spacing 4.0)

;; Completion icons by Microsoft (CC BY 4.0)
;; https://github.com/microsoft/vscode-icons
(defn- completion-type-icon [{:keys [type]}]
  (let [width-multiplier (case type
                           :event 10
                           :unit 12
                           :file 14
                           16)
        height-multiplier (case type
                            :message 12
                            :text 10
                            :field 15
                            :variable 11
                            (:method :function :constructor) 17
                            (:value :enum) 13
                            :enum-member 13
                            :constant 12
                            :folder 13
                            :struct 13
                            :type-parameter 10
                            16)]
    {:fx/type fx.region/lifecycle
     :style {:-fx-background-color :-df-text-dark}
     :max-width (* completion-type-icon-size (double (/ width-multiplier 16)))
     :max-height (* completion-type-icon-size (double (/ height-multiplier 16)))
     :shape {:fx/type fx.svg-path/lifecycle
             :content (case type
                        ;; defold-specific
                        :message "M1 3.5L1.5 3H14.5L15 3.5V3.769V12.5L14.5 13H1.5L1 12.5V3.769V3.5ZM2 4.53482V12H14V4.53597L8.31 8.9H7.7L2 4.53482ZM13.03 4H2.97L8 7.869L13.03 4Z"
                        ;; shared with vscode
                        :text "M7.22313 10.933C7.54888 11.1254 7.92188 11.2231 8.30013 11.215C8.63802 11.2218 8.97279 11.1492 9.27746 11.003C9.58213 10.8567 9.84817 10.6409 10.0541 10.373C10.5094 9.76519 10.7404 9.01867 10.7081 8.25998C10.7414 7.58622 10.5376 6.9221 10.1321 6.38298C9.93599 6.14161 9.68601 5.94957 9.40225 5.82228C9.11848 5.69498 8.80883 5.63597 8.49813 5.64997C8.07546 5.64699 7.66018 5.76085 7.29813 5.97898C7.18328 6.04807 7.07515 6.12775 6.97513 6.21698V3.47498H5.98413V11.1H6.97913V10.756C7.0554 10.8217 7.13702 10.8809 7.22313 10.933ZM7.85005 6.70006C8.03622 6.62105 8.23832 6.58677 8.44013 6.59998C8.61281 6.59452 8.78429 6.63054 8.94018 6.70501C9.09608 6.77948 9.23185 6.89023 9.33613 7.02798C9.59277 7.39053 9.71865 7.82951 9.69313 8.27297C9.71996 8.79748 9.57993 9.31701 9.29313 9.75698C9.18846 9.91527 9.04571 10.0447 8.87797 10.1335C8.71023 10.2223 8.52289 10.2675 8.33313 10.265C8.14958 10.2732 7.96654 10.24 7.79758 10.1678C7.62862 10.0956 7.47809 9.98628 7.35713 9.84797C7.10176 9.55957 6.96525 9.18506 6.97513 8.79998V8.19998C6.96324 7.78332 7.10287 7.3765 7.36813 7.05498C7.49883 6.90064 7.66388 6.77908 7.85005 6.70006ZM3.28926 5.67499C2.97035 5.67933 2.65412 5.734 2.35226 5.83699C2.06442 5.92293 1.79372 6.05828 1.55226 6.23699L1.45226 6.31399V7.51399L1.87526 7.15499C2.24603 6.80478 2.73158 6.60146 3.24126 6.58299C3.36617 6.57164 3.49194 6.59147 3.60731 6.64068C3.72267 6.6899 3.82402 6.76697 3.90226 6.86499C4.05245 7.0971 4.13264 7.36754 4.13326 7.64399L2.90026 7.82499C2.39459 7.87781 1.9155 8.07772 1.52226 8.39999C1.36721 8.55181 1.24363 8.73271 1.15859 8.93235C1.07355 9.13199 1.02873 9.34644 1.02668 9.56343C1.02464 9.78042 1.06542 9.99568 1.14668 10.1969C1.22795 10.3981 1.3481 10.5813 1.50026 10.736C1.66895 10.8904 1.86647 11.01 2.08149 11.0879C2.29651 11.1659 2.5248 11.2005 2.75326 11.19C3.14725 11.1931 3.53302 11.0774 3.86026 10.858C3.96178 10.7897 4.05744 10.7131 4.14626 10.629V11.073H5.08726V7.71499C5.12161 7.17422 4.95454 6.63988 4.61826 6.21499C4.45004 6.03285 4.24373 5.89003 4.01402 5.7967C3.78431 5.70336 3.53686 5.66181 3.28926 5.67499ZM4.14626 8.71599C4.16588 9.13435 4.02616 9.54459 3.75526 9.864C3.63714 10.0005 3.49023 10.1092 3.3251 10.1821C3.15998 10.2551 2.98073 10.2906 2.80026 10.286C2.69073 10.2945 2.5806 10.2812 2.47624 10.2469C2.37187 10.2125 2.27536 10.1579 2.19226 10.086C2.06104 9.93455 1.9888 9.74088 1.9888 9.54049C1.9888 9.34011 2.06104 9.14644 2.19226 8.99499C2.47347 8.82131 2.79258 8.71837 3.12226 8.69499L4.14226 8.54699L4.14626 8.71599ZM12.459 11.0325C12.7663 11.1638 13.0985 11.2261 13.4324 11.215C13.9273 11.227 14.4156 11.1006 14.8424 10.85L14.9654 10.775L14.9784 10.768V9.61504L14.5324 9.93504C14.2163 10.1592 13.8359 10.2747 13.4484 10.264C13.2499 10.2719 13.0522 10.2342 12.8705 10.1538C12.6889 10.0733 12.5281 9.95232 12.4004 9.80004C12.1146 9.42453 11.9728 8.95911 12.0004 8.48804C11.9739 7.98732 12.1355 7.49475 12.4534 7.10704C12.5936 6.94105 12.7698 6.80914 12.9685 6.7213C13.1672 6.63346 13.3833 6.592 13.6004 6.60004C13.9441 6.59844 14.281 6.69525 14.5714 6.87904L15.0004 7.14404V5.97004L14.8314 5.89704C14.4628 5.73432 14.0644 5.6502 13.6614 5.65004C13.3001 5.63991 12.9409 5.70762 12.608 5.84859C12.2752 5.98956 11.9766 6.20048 11.7324 6.46704C11.2263 7.02683 10.9584 7.76186 10.9854 8.51604C10.9569 9.22346 11.1958 9.91569 11.6544 10.455C11.8772 10.704 12.1518 10.9012 12.459 11.0325Z"
                        (:method :function :constructor) "M13.5103 4L8.51025 1H7.51025L2.51025 4L2.02026 4.85999V10.86L2.51025 11.71L7.51025 14.71H8.51025L13.5103 11.71L14.0002 10.86V4.85999L13.5103 4ZM7.51025 13.5601L3.01025 10.86V5.69995L7.51025 8.15002V13.5601ZM3.27026 4.69995L8.01025 1.85999L12.7502 4.69995L8.01025 7.29004L3.27026 4.69995ZM13.0103 10.86L8.51025 13.5601V8.15002L13.0103 5.69995V10.86Z"
                        :field "M14.4502 4.5L9.4502 2H8.55029L1.55029 5.5L1.00024 6.39001V10.89L1.55029 11.79L6.55029 14.29H7.4502L14.4502 10.79L15.0002 9.89001V5.39001L14.4502 4.5ZM6.4502 13.14L1.9502 10.89V7.17004L6.4502 9.17004V13.14ZM6.9502 8.33997L2.29028 6.22998L8.9502 2.89001L13.6202 5.22998L6.9502 8.33997ZM13.9502 9.89001L7.4502 13.14V9.20996L13.9502 6.20996V9.89001Z"
                        :variable "M2.00024 5H4.00024V4H1.50024L1.00024 4.5V12.5L1.50024 13H4.00024V12H2.00024V5ZM14.5002 4H12.0002V5H14.0002V12H12.0002V13H14.5002L15.0002 12.5V4.5L14.5002 4ZM11.7603 6.56995L12.0002 7V9.51001L11.7002 9.95996L7.2002 11.96H6.74023L4.24023 10.46L4.00024 10.03V7.53003L4.30029 7.06995L8.80029 5.06995H9.26025L11.7603 6.56995ZM5.00024 9.70996L6.50024 10.61V9.28003L5.00024 8.38V9.70996ZM5.5802 7.56006L7.03027 8.43005L10.4203 6.93005L8.97021 6.06006L5.5802 7.56006ZM7.53027 10.73L11.0303 9.17004V7.77002L7.53027 9.31995V10.73Z"
                        :class "M11.3403 9.70998H12.0503L14.7202 7.04005V6.32997L13.3803 5.00001H12.6803L10.8603 6.81007H5.86024V5.56007L7.72023 3.70997V3L5.72022 1H5.00025L1.00024 5.00001V5.70997L3.00025 7.70998H3.71027L4.85023 6.56007V12.35L5.35023 12.85H10.0003V13.37L11.3303 14.71H12.0402L14.7103 12.0401V11.33L13.3703 10H12.6703L10.8103 11.85H5.81025V7.84999H10.0003V8.32997L11.3403 9.70998ZM13.0303 6.06007L13.6602 6.68995L11.6602 8.68996L11.0303 8.06007L13.0303 6.06007ZM13.0303 11.0601L13.6602 11.69L11.6602 13.69L11.0303 13.0601L13.0303 11.0601ZM3.35022 6.65004L2.06024 5.34998L5.35023 2.06006L6.65028 3.34998L3.35022 6.65004Z"
                        :interface ""
                        :module "M6.00024 2.98361V2.97184V2H5.91107C5.59767 2 5.29431 2.06161 5.00152 2.18473C4.70842 2.30798 4.44967 2.48474 4.22602 2.71498C4.00336 2.94422 3.83816 3.19498 3.73306 3.46766L3.73257 3.46898C3.63406 3.7352 3.56839 4.01201 3.53557 4.29917L3.53543 4.30053C3.50702 4.5805 3.49894 4.86844 3.51108 5.16428C3.52297 5.45379 3.52891 5.74329 3.52891 6.03279C3.52891 6.23556 3.48999 6.42594 3.41225 6.60507L3.41185 6.60601C3.33712 6.78296 3.23447 6.93866 3.10341 7.07359C2.97669 7.20405 2.8249 7.31055 2.64696 7.3925C2.47084 7.46954 2.28522 7.5082 2.08942 7.5082H2.00024V7.6V8.4V8.4918H2.08942C2.2849 8.4918 2.47026 8.53238 2.64625 8.61334L2.64766 8.61396C2.82482 8.69157 2.97602 8.79762 3.10245 8.93161L3.10436 8.93352C3.23452 9.0637 3.33684 9.21871 3.41153 9.39942L3.41225 9.40108C3.49011 9.58047 3.52891 9.76883 3.52891 9.96721C3.52891 10.2567 3.52297 10.5462 3.51108 10.8357C3.49894 11.1316 3.50701 11.4215 3.5354 11.7055L3.5356 11.7072C3.56844 11.9903 3.63412 12.265 3.73256 12.531L3.73307 12.5323C3.83817 12.805 4.00336 13.0558 4.22602 13.285C4.44967 13.5153 4.70842 13.692 5.00152 13.8153C5.29431 13.9384 5.59767 14 5.91107 14H6.00024V13.2V13.0164H5.91107C5.71119 13.0164 5.52371 12.9777 5.34787 12.9008C5.17421 12.8191 5.02218 12.7126 4.89111 12.5818C4.76411 12.4469 4.66128 12.2911 4.58247 12.1137C4.50862 11.9346 4.47158 11.744 4.47158 11.541C4.47158 11.3127 4.47554 11.0885 4.48346 10.8686C4.49149 10.6411 4.49151 10.4195 4.48349 10.2039C4.47938 9.98246 4.46109 9.76883 4.42847 9.56312C4.39537 9.35024 4.33946 9.14757 4.26063 8.95536C4.18115 8.76157 4.07282 8.57746 3.9364 8.40298C3.8237 8.25881 3.68563 8.12462 3.52307 8C3.68563 7.87538 3.8237 7.74119 3.9364 7.59702C4.07282 7.42254 4.18115 7.23843 4.26063 7.04464C4.33938 6.85263 4.39537 6.65175 4.4285 6.44285C4.46107 6.2333 4.47938 6.01973 4.48349 5.80219C4.49151 5.58262 4.4915 5.36105 4.48345 5.13749C4.47554 4.9134 4.47158 4.68725 4.47158 4.45902C4.47158 4.26019 4.50857 4.07152 4.58263 3.89205C4.6616 3.71034 4.76445 3.55475 4.8911 3.42437C5.02218 3.28942 5.17485 3.18275 5.34826 3.10513C5.52404 3.02427 5.71138 2.98361 5.91107 2.98361H6.00024ZM10.0002 13.0164V13.0282V14H10.0894C10.4028 14 10.7062 13.9384 10.999 13.8153C11.2921 13.692 11.5508 13.5153 11.7745 13.285C11.9971 13.0558 12.1623 12.805 12.2674 12.5323L12.2679 12.531C12.3664 12.2648 12.4321 11.988 12.4649 11.7008L12.4651 11.6995C12.4935 11.4195 12.5015 11.1316 12.4894 10.8357C12.4775 10.5462 12.4716 10.2567 12.4716 9.96721C12.4716 9.76444 12.5105 9.57406 12.5882 9.39493L12.5886 9.39399C12.6634 9.21704 12.766 9.06134 12.8971 8.92642C13.0238 8.79595 13.1756 8.68945 13.3535 8.6075C13.5296 8.53046 13.7153 8.4918 13.9111 8.4918H14.0002V8.4V7.6V7.5082H13.9111C13.7156 7.5082 13.5302 7.46762 13.3542 7.38666L13.3528 7.38604C13.1757 7.30844 13.0245 7.20238 12.898 7.06839L12.8961 7.06648C12.766 6.9363 12.6637 6.78129 12.589 6.60058L12.5882 6.59892C12.5104 6.41953 12.4716 6.23117 12.4716 6.03279C12.4716 5.74329 12.4775 5.45379 12.4894 5.16428C12.5015 4.86842 12.4935 4.57848 12.4651 4.29454L12.4649 4.29285C12.4321 4.00971 12.3664 3.73502 12.2679 3.46897L12.2674 3.46766C12.1623 3.19499 11.9971 2.94422 11.7745 2.71498C11.5508 2.48474 11.2921 2.30798 10.999 2.18473C10.7062 2.06161 10.4028 2 10.0894 2H10.0002V2.8V2.98361H10.0894C10.2893 2.98361 10.4768 3.0223 10.6526 3.09917C10.8263 3.18092 10.9783 3.28736 11.1094 3.41823C11.2364 3.55305 11.3392 3.70889 11.418 3.88628C11.4919 4.0654 11.5289 4.25596 11.5289 4.45902C11.5289 4.68727 11.5249 4.91145 11.517 5.13142C11.509 5.35894 11.509 5.58049 11.517 5.79605C11.5211 6.01754 11.5394 6.23117 11.572 6.43688C11.6051 6.64976 11.661 6.85243 11.7399 7.04464C11.8193 7.23843 11.9277 7.42254 12.0641 7.59702C12.1768 7.74119 12.3149 7.87538 12.4774 8C12.3149 8.12462 12.1768 8.25881 12.0641 8.40298C11.9277 8.57746 11.8193 8.76157 11.7399 8.95536C11.6611 9.14737 11.6051 9.34825 11.572 9.55715C11.5394 9.7667 11.5211 9.98027 11.517 10.1978C11.509 10.4174 11.509 10.6389 11.517 10.8625C11.5249 11.0866 11.5289 11.3128 11.5289 11.541C11.5289 11.7398 11.4919 11.9285 11.4179 12.1079C11.3389 12.2897 11.236 12.4452 11.1094 12.5756C10.9783 12.7106 10.8256 12.8173 10.6522 12.8949C10.4764 12.9757 10.2891 13.0164 10.0894 13.0164H10.0002Z"
                        :property "M2.80747 14.9754C2.57144 14.9721 2.33851 14.9211 2.12271 14.8254C1.90692 14.7297 1.71273 14.5913 1.55183 14.4186C1.23875 14.1334 1.04458 13.7408 1.00799 13.3189C0.966469 12.8828 1.09293 12.4473 1.36158 12.1013C2.56804 10.8289 4.94755 8.4494 6.67836 6.75479C6.31007 5.75887 6.32729 4.66127 6.72661 3.67739C7.05499 2.85876 7.63893 2.16805 8.39153 1.70807C8.98195 1.31706 9.66055 1.07944 10.3659 1.01673C11.0713 0.954022 11.7812 1.06819 12.4313 1.34892L13.0485 1.6162L10.1827 4.56738L11.4374 5.82582L14.3811 2.94887L14.6484 3.56788C14.8738 4.08976 14.9933 4.65119 14.9999 5.21961C15.0066 5.78802 14.9004 6.35211 14.6874 6.87915C14.4763 7.40029 14.1626 7.87368 13.7649 8.27122C13.5396 8.49169 13.2907 8.68653 13.0225 8.85218C12.4676 9.22275 11.8327 9.45636 11.1699 9.5338C10.5071 9.61124 9.83546 9.5303 9.21007 9.29764C8.11219 10.4113 5.37167 13.1704 3.89143 14.5522C3.5945 14.8219 3.20856 14.9726 2.80747 14.9754ZM10.7451 1.92802C10.0873 1.92637 9.44383 2.12018 8.89639 2.48485C8.68289 2.6152 8.48461 2.76897 8.30522 2.9433C7.82813 3.42423 7.5095 4.03953 7.39206 4.70669C7.27462 5.37385 7.36398 6.06098 7.64816 6.67591L7.78366 6.97288L7.55072 7.20025C5.81249 8.89672 3.28171 11.4201 2.06504 12.7045C1.9567 12.8658 1.91037 13.0608 1.93459 13.2535C1.95881 13.4463 2.05195 13.6238 2.19682 13.7532C2.28029 13.8462 2.38201 13.9211 2.49565 13.9731C2.59581 14.0184 2.70408 14.043 2.81397 14.0455C2.98089 14.0413 3.14068 13.977 3.26407 13.8646C4.83711 12.3964 7.87646 9.32641 8.76832 8.42435L8.99754 8.19326L9.29266 8.32783C9.80642 8.56732 10.3734 8.66985 10.9385 8.62545C11.5036 8.58106 12.0476 8.39125 12.5176 8.07447C12.7316 7.9426 12.9299 7.78694 13.1088 7.61045C13.4186 7.30153 13.6634 6.93374 13.8288 6.52874C13.9943 6.12375 14.077 5.68974 14.0721 5.25228C14.0722 5.03662 14.0507 4.82148 14.0081 4.61007L11.4309 7.12508L8.87968 4.57759L11.3947 1.98834C11.1807 1.94674 10.9631 1.92653 10.7451 1.92802Z"
                        :unit "M4.00024 1L3.00024 2V14L4.00024 15H12.0002L13.0002 14V2L12.0002 1H4.00024ZM4.00024 3V2H12.0002V14H4.00024V13H6.00024V12H4.00024V10H8.00024V9H4.00024V7H6.00024V6H4.00024V4H8.00024V3H4.00024Z"
                        (:value :enum) "M14.0002 2H8.00024L7.00024 3V6H8.00024V3H14.0002V8H10.0002V9H14.0002L15.0002 8V3L14.0002 2ZM9.00024 6H13.0002V7H9.41024L9.00024 6.59V6ZM7.00024 7H2.00024L1.00024 8V13L2.00024 14H8.00024L9.00024 13V8L8.00024 7H7.00024ZM8.00024 13H2.00024V8H8.00024V9V13ZM3.00024 9H7.00024V10H3.00024V9ZM3.00024 11H7.00024V12H3.00024V11ZM9.00024 4H13.0002V5H9.00024V4Z"
                        :snippet "M2.50024 1L2.00024 1.5V13H3.00024V2H14.0002V13H15.0002V1.5L14.5002 1H2.50024ZM2.00024 15V14H3.00024V15H2.00024ZM5.00024 14.0001H4.00024V15.0001H5.00024V14.0001ZM6.00024 14.0001H7.00024V15.0001H6.00024V14.0001ZM9.00024 14.0001H8.00024V15.0001H9.00024V14.0001ZM10.0002 14.0001H11.0002V15.0001H10.0002V14.0001ZM15.0002 15.0001V14.0001H14.0002V15.0001H15.0002ZM12.0002 14.0001H13.0002V15.0001H12.0002V14.0001Z"
                        :color "M8.00024 1.00305C6.14373 1.00305 4.36323 1.74059 3.05048 3.05334C1.73772 4.3661 1.00024 6.14654 1.00024 8.00305V8.43311C1.09024 9.94311 2.91024 10.2231 4.00024 9.13306C4.35673 8.81625 4.82078 8.64759 5.29749 8.66162C5.77419 8.67565 6.22753 8.87127 6.56476 9.2085C6.90199 9.54572 7.0976 9.99912 7.11163 10.4758C7.12567 10.9525 6.95707 11.4166 6.64026 11.7731C5.54026 12.9331 5.85025 14.843 7.44025 14.973H8.04022C9.89674 14.973 11.6772 14.2356 12.99 12.9229C14.3027 11.6101 15.0402 9.82954 15.0402 7.97302C15.0402 6.11651 14.3027 4.33607 12.99 3.02332C11.6772 1.71056 9.89674 0.973022 8.04022 0.973022L8.00024 1.00305ZM8.00024 14.0031H7.48022C7.34775 13.9989 7.22072 13.9495 7.12024 13.863C7.04076 13.7807 6.98839 13.6761 6.97021 13.5631C6.93834 13.3682 6.95353 13.1684 7.0144 12.9806C7.07528 12.7927 7.18013 12.6222 7.32025 12.483C7.84072 11.9474 8.1319 11.2299 8.1319 10.483C8.1319 9.73615 7.84072 9.0187 7.32025 8.48303C7.05373 8.21635 6.73723 8.00481 6.38892 7.86047C6.0406 7.71614 5.66726 7.64185 5.29022 7.64185C4.91318 7.64185 4.53984 7.71614 4.19153 7.86047C3.84321 8.00481 3.52678 8.21635 3.26025 8.48303C3.15093 8.61081 3.01113 8.709 2.85382 8.76843C2.69651 8.82786 2.52673 8.84657 2.36023 8.823C2.27617 8.80694 2.19927 8.76498 2.14026 8.703C2.07155 8.6224 2.0358 8.5189 2.04022 8.41309V8.04309C2.04022 6.8564 2.39216 5.69629 3.05145 4.70959C3.71074 3.7229 4.64778 2.95388 5.74414 2.49976C6.8405 2.04563 8.04693 1.92681 9.21082 2.15833C10.3747 2.38984 11.4438 2.9613 12.2829 3.80042C13.122 4.63953 13.6934 5.70867 13.9249 6.87256C14.1564 8.03644 14.0376 9.24275 13.5835 10.3391C13.1294 11.4355 12.3604 12.3726 11.3737 13.0319C10.387 13.6911 9.22691 14.0431 8.04022 14.0431L8.00024 14.0031ZM9.00024 3.99683C9.00024 4.54911 8.55253 4.99683 8.00024 4.99683C7.44796 4.99683 7.00024 4.54911 7.00024 3.99683C7.00024 3.44454 7.44796 2.99683 8.00024 2.99683C8.55253 2.99683 9.00024 3.44454 9.00024 3.99683ZM12.0002 11.0037C12.0002 11.5559 11.5525 12.0037 11.0002 12.0037C10.448 12.0037 10.0002 11.5559 10.0002 11.0037C10.0002 10.4514 10.448 10.0037 11.0002 10.0037C11.5525 10.0037 12.0002 10.4514 12.0002 11.0037ZM5.00024 6.00415C5.55253 6.00415 6.00024 5.55644 6.00024 5.00415C6.00024 4.45187 5.55253 4.00415 5.00024 4.00415C4.44796 4.00415 4.00024 4.45187 4.00024 5.00415C4.00024 5.55644 4.44796 6.00415 5.00024 6.00415ZM12.0002 5.00415C12.0002 5.55644 11.5525 6.00415 11.0002 6.00415C10.448 6.00415 10.0002 5.55644 10.0002 5.00415C10.0002 4.45187 10.448 4.00415 11.0002 4.00415C11.5525 4.00415 12.0002 4.45187 12.0002 5.00415ZM13.0003 7.99939C13.0003 8.55167 12.5526 8.99939 12.0003 8.99939C11.448 8.99939 11.0003 8.55167 11.0003 7.99939C11.0003 7.4471 11.448 6.99939 12.0003 6.99939C12.5526 6.99939 13.0003 7.4471 13.0003 7.99939Z"
                        :file "M13.8502 4.44L10.5702 1.14L10.2202 1H2.50024L2.00024 1.5V14.5L2.50024 15H13.5002L14.0002 14.5V4.8L13.8502 4.44ZM13.0002 5H10.0002V2L13.0002 5ZM3.00024 14V2H9.00024V5.5L9.50024 6H13.0002V14H3.00024Z"
                        :reference "M11.1055 4.5613L7.67529 7.98827L6.54097 6.86834L8.61123 4.78848H3.81155C3.17507 4.78848 2.56466 5.04132 2.11461 5.49138C1.66455 5.94144 1.41171 6.55184 1.41171 7.18832C1.41171 7.8248 1.66455 8.43521 2.11461 8.88527C2.56466 9.33532 3.17507 9.58816 3.81155 9.58816H4.70109V11.1881H3.82115C2.79234 11.142 1.82094 10.7009 1.1092 9.95661C0.397467 9.21231 0.000244141 8.22216 0.000244141 7.19232C0.000244141 6.16249 0.397467 5.17234 1.1092 4.42803C1.82094 3.68373 2.79234 3.24263 3.82115 3.19659H8.62083L6.54097 1.13112L7.67529 0L11.1055 3.43177V4.5613ZM16.6203 24H7.02094L6.22099 23.2V10.4121L7.02094 9.61215H16.6203L17.4202 10.4121V23.2L16.6203 24ZM7.82089 22.4001H15.8204V11.212H7.82089V22.4001ZM13.4205 1.6015H23.0199L23.8198 2.40145V15.1878L23.0199 15.9877H19.0201V14.3878H22.2199V3.20139H14.2205V7.98828H12.6206V2.40145L13.4205 1.6015ZM14.2205 12.7879H9.42078V14.3878H14.2205V12.7879ZM9.42078 15.9877H14.2205V17.5876H9.42078V15.9877ZM14.2205 19.1875H9.42078V20.7874H14.2205V19.1875ZM15.8203 4.78848H20.62V6.38838H15.8203V4.78848ZM20.62 11.188H19.0201V12.7879H20.62V11.188ZM17.2827 8.01228V7.98828H20.62V9.58817H18.8585L17.2827 8.01228Z"
                        :folder "M14.5002 3H7.71021L6.86023 2.15002L6.51025 2H1.51025L1.01025 2.5V6.5V13.5L1.51025 14H14.5103L15.0103 13.5V9V3.5L14.5002 3ZM13.9902 11.49V13H1.99023V11.49V7.48999V7H6.48022L6.8302 6.84998L7.69019 5.98999H14.0002V7.48999L13.9902 11.49ZM13.9902 5H7.49023L7.14026 5.15002L6.28027 6.01001H2.00024V3.01001H6.29028L7.14026 3.85999L7.50024 4.01001H14.0002L13.9902 5Z"
                        :enum-member "M7.00024 3L8.00024 2H14.0002L15.0002 3V8L14.0002 9H10.0002V8H14.0002V3H8.00024V6H7.00024V3ZM9.00024 9V8L8.00024 7H7.00024H2.00024L1.00024 8V13L2.00024 14H8.00024L9.00024 13V9ZM8.00024 8V9V13H2.00024V8H7.00024H8.00024ZM9.41446 7L9.00024 6.58579V6H13.0002V7H9.41446ZM9.00024 4H13.0002V5H9.00024V4ZM7.00024 10H3.00024V11H7.00024V10Z"
                        :constant "M4.00024 6H12.0002V7H4.00024V6ZM12.0002 9H4.00024V10H12.0002V9Z M1.00024 4L2.00024 3H14.0002L15.0002 4V12L14.0002 13H2.00024L1.00024 12V4ZM2.00024 4V12H14.0002V4H2.00024Z"
                        :struct "M2.00024 2L1.00024 3V6L2.00024 7H14.0002L15.0002 6V3L14.0002 2H2.00024ZM2.00024 3H3.00024H13.0002H14.0002V4V5V6H13.0002H3.00024H2.00024V5V4V3ZM1.00024 10L2.00024 9H5.00024L6.00024 10V13L5.00024 14H2.00024L1.00024 13V10ZM3.00024 10H2.00024V11V12V13H3.00024H4.00024H5.00024V12V11V10H4.00024H3.00024ZM10.0002 10L11.0002 9H14.0002L15.0002 10V13L14.0002 14H11.0002L10.0002 13V10ZM12.0002 10H11.0002V11V12V13H12.0002H13.0002H14.0002V12V11V10H13.0002H12.0002Z"
                        :event "M7.41379 1.55996L8.31177 1H11.6059L12.4243 2.57465L10.2358 6H12.0176L12.7365 7.69512L5.61967 15L4.01699 13.837L6.11967 10H4.89822L4.00024 8.55996L7.41379 1.55996ZM7.78058 9L4.90078 14.3049L12.0176 7H8.31177L11.6059 2H8.31177L4.89822 9H7.78058Z"
                        :operator "M2.87313 1.10023C3.20793 1.23579 3.4757 1.498 3.61826 1.82988C3.69056 1.99959 3.72699 2.18242 3.72526 2.36688C3.72642 2.54999 3.69 2.7314 3.61826 2.89988C3.51324 3.14567 3.33807 3.35503 3.11466 3.50177C2.89126 3.64851 2.62955 3.72612 2.36226 3.72488C2.17948 3.72592 1.99842 3.68951 1.83026 3.61788C1.58322 3.51406 1.37252 3.33932 1.22478 3.11575C1.07704 2.89219 0.99891 2.62984 1.00026 2.36188C0.999374 2.17921 1.03543 1.99825 1.10626 1.82988C1.24362 1.50314 1.50353 1.24323 1.83026 1.10588C2.16357 0.966692 2.53834 0.964661 2.87313 1.10023ZM2.57526 2.86488C2.70564 2.80913 2.80951 2.70526 2.86526 2.57488C2.89314 2.50838 2.90742 2.43698 2.90726 2.36488C2.90838 2.2654 2.88239 2.1675 2.8321 2.08167C2.7818 1.99584 2.70909 1.92531 2.62176 1.87767C2.53443 1.83002 2.43577 1.80705 2.33638 1.81121C2.23698 1.81537 2.1406 1.8465 2.05755 1.90128C1.97451 1.95606 1.90794 2.03241 1.865 2.12215C1.82205 2.21188 1.80434 2.31161 1.81376 2.41065C1.82319 2.50968 1.85939 2.60428 1.9185 2.6843C1.9776 2.76433 2.05738 2.82675 2.14926 2.86488C2.28574 2.92089 2.43878 2.92089 2.57526 2.86488ZM6.43019 1.1095L1.10992 6.42977L1.79581 7.11567L7.11608 1.7954L6.43019 1.1095ZM11.5002 8.99999H12.5002V11.5H15.0002V12.5H12.5002V15H11.5002V12.5H9.00024V11.5H11.5002V8.99999ZM5.76801 9.52509L6.47512 10.2322L4.70735 12L6.47512 13.7677L5.76801 14.4748L4.00024 12.7071L2.23248 14.4748L1.52537 13.7677L3.29314 12L1.52537 10.2322L2.23248 9.52509L4.00024 11.2929L5.76801 9.52509ZM7.11826 5.32988C7.01466 5.08268 6.83997 4.87183 6.61636 4.72406C6.39275 4.57629 6.13028 4.49826 5.86226 4.49988C5.6796 4.49899 5.49864 4.53505 5.33026 4.60588C5.00353 4.74323 4.74362 5.00314 4.60626 5.32988C4.53612 5.49478 4.49922 5.67191 4.49766 5.8511C4.4961 6.0303 4.52992 6.20804 4.59718 6.37414C4.66443 6.54024 4.76381 6.69143 4.88961 6.81906C5.0154 6.94669 5.16515 7.04823 5.33026 7.11788C5.49892 7.18848 5.67993 7.22484 5.86276 7.22484C6.0456 7.22484 6.22661 7.18848 6.39526 7.11788C6.64225 7.01388 6.85295 6.83913 7.00082 6.61563C7.1487 6.39213 7.22713 6.12987 7.22626 5.86188C7.22679 5.67905 7.19005 5.49803 7.11826 5.32988ZM6.36526 6.07488C6.3379 6.13937 6.29854 6.19808 6.24926 6.24788C6.19932 6.29724 6.14066 6.33691 6.07626 6.36488C6.00878 6.39297 5.93635 6.40725 5.86326 6.40688C5.79015 6.40744 5.71769 6.39315 5.65026 6.36488C5.58565 6.33729 5.52693 6.29757 5.47726 6.24788C5.42715 6.19856 5.38738 6.13975 5.36026 6.07488C5.30425 5.9384 5.30425 5.78536 5.36026 5.64888C5.41561 5.51846 5.51965 5.41477 5.65026 5.35988C5.71761 5.33126 5.79008 5.31663 5.86326 5.31688C5.93642 5.31685 6.00884 5.33147 6.07626 5.35988C6.14062 5.38749 6.19928 5.42682 6.24926 5.47588C6.2981 5.52603 6.33741 5.58465 6.36526 5.64888C6.39364 5.7163 6.40827 5.78872 6.40827 5.86188C6.40827 5.93503 6.39364 6.00745 6.36526 6.07488ZM14.0002 3H10.0002V4H14.0002V3Z"
                        :type-parameter "M11.0003 6H10.0003V5.5C10.0003 5.22386 9.7764 5 9.50026 5H8.47926V10.5C8.47926 10.7761 8.70312 11 8.97926 11H9.47926V12H6.47926V11H6.97926C7.2554 11 7.47926 10.7761 7.47926 10.5V5H6.50026C6.22412 5 6.00026 5.22386 6.00026 5.5V6H5.00026V4H11.0003V6ZM13.9145 8.0481L12.4522 6.58581L13.1593 5.87871L14.9751 7.69454V8.40165L13.2074 10.1694L12.5003 9.46231L13.9145 8.0481ZM3.54835 9.4623L2.08605 8.00002L3.50026 6.58581L2.79316 5.8787L1.02539 7.64647V8.35357L2.84124 10.1694L3.54835 9.4623Z"
                        :keyword "M15.0002 4H10.0002V3H15.0002V4ZM14.0002 7H12.0002V8H14.0002V7ZM10.0002 7H1.00024V8H10.0002V7ZM12.0002 13H1.00024V14H12.0002V13ZM7.00024 10H1.00024V11H7.00024V10ZM15.0002 10H10.0002V11H15.0002V10ZM8.00024 2V5H1.00024V2H8.00024ZM7.00024 3H2.00024V4H7.00024V3Z")}}))

(defn- completion-list-cell-view [completion]
  (if completion
    (let [text (:display-string completion)
          matching-indices (fuzzy-choices/matching-indices completion)]
      {:graphic {:fx/type fx.h-box/lifecycle
                 :alignment :center-left
                 :spacing completion-icon-text-spacing
                 :children [{:fx/type fx.stack-pane/lifecycle
                             :min-width completion-type-icon-size
                             :max-width completion-type-icon-size
                             :children (if-let [type (:type completion)]
                                         [{:fx/type completion-type-icon
                                           :type type}]
                                         [])}
                            (fuzzy-choices/make-matched-text-flow-cljfx
                              text matching-indices
                              :deprecated (contains? (:tags completion) :deprecated))]}
       :on-mouse-clicked {:event :accept :completion completion}})
    {}))

(defn- completion-popup-view
  [{:keys [canvas-repaint-info font font-name font-size
           visible-completion-ranges query screen-bounds completions-showing
           completions-doc project completions-combined completions-selected-index
           completions-shortcut-text]}]
  (let [item-count (count completions-combined)]
    (if (or (not completions-showing) (zero? item-count))
      {:fx/type fxui/ext-value :value nil}
      (let [{:keys [^Canvas canvas ^LayoutInfo layout lines]} canvas-repaint-info
            ^Point2D cursor-bottom (or (and (pos? (count visible-completion-ranges))
                                            (cursor-bottom layout lines (data/cursor-range-start (first visible-completion-ranges))))
                                       Point2D/ZERO)
            anchor (.localToScreen canvas cursor-bottom)
            glyph-metrics (.-glyph layout)
            ^double cell-height (data/line-height glyph-metrics)
            max-visible-completions-count 10
            text (doto (Text.) (.setFont font))
            hint-height (if completions-shortcut-text 15 0)
            min-completions-height (* (min 3 item-count) cell-height)
            min-completions-width 150.0
            [^Point2D anchor ^Rectangle2D screen-bounds]
            (transduce (map (fn [^Rectangle2D bounds]
                              (pair
                                (Point2D.
                                  (-> (.getX anchor)
                                      (min (- (.getMaxX bounds) min-completions-width))
                                      (max (.getMinX bounds)))
                                  (-> (.getY anchor)
                                      (min (- (.getMaxY bounds) min-completions-height hint-height))
                                      (max (.getMinY bounds))))
                                bounds)))
                       (partial min-key #(.distance anchor ^Point2D (key %)))
                       (pair (Point2D. Double/MAX_VALUE Double/MAX_VALUE) nil)
                       screen-bounds)
            max-completions-width (min 700.0 (- (.getMaxX screen-bounds) (.getX anchor)))
            max-completions-height (min (* cell-height max-visible-completions-count)
                                        (-> (- (.getMaxY screen-bounds) (.getY anchor) hint-height)
                                            (/ cell-height)
                                            Math/floor
                                            (* cell-height)))
            display-string-widths (eduction
                                    (take 512)
                                    (map #(-> text
                                              (doto (.setText (:display-string %)))
                                              .getLayoutBounds
                                              .getWidth))
                                    completions-combined)
            ^double max-display-string-width (reduce max 0.0 display-string-widths)
            completions-width (-> (+ 12.0                      ;; paddings
                                     (if (< max-visible-completions-count item-count)
                                       10.0                    ;; scroll bar
                                       0.0)
                                     ;; clamp
                                     (max min-completions-width
                                          (+ completion-type-icon-size
                                             completion-icon-text-spacing
                                             max-display-string-width)))
                                  (min max-completions-width))
            refresh-key [query completions-combined]
            selected-completion (completions-combined completions-selected-index)]
        {:fx/type fx/ext-let-refs
         :refs
         {:content
          {:fx/type fx.stack-pane/lifecycle
           :style-class "flat-list-container"
           :stylesheets [(str (io/resource "editor.css"))]
           :children
           [{:fx/type fx.region/lifecycle
             :style-class "completion-popup-background"}
            {:fx/type fx.v-box/lifecycle
             :children
             (cond->
               [{:fx/type ext-with-list-view-props
                 :props {:selected-index [refresh-key completions-selected-index]}
                 :desc
                 {:fx/type ext-with-list-view-props
                  :props {:items [refresh-key completions-combined]}
                  :desc
                  {:fx/type fx.ext.list-view/with-selection-props
                   :props {:on-selected-index-changed {:event :select}}
                   :desc
                   {:fx/type fx.list-view/lifecycle
                    :style {:-fx-font-family (str \" font-name \") :-fx-font-size font-size}
                    :id "fuzzy-choices-list-view"
                    :style-class ["flat-list-view" "completion-popup-list-view"]
                    :fixed-cell-size cell-height
                    :event-filter {:event :completion-list-view-event-filter}
                    :min-width completions-width
                    :pref-width completions-width
                    :max-width completions-width
                    :min-height min-completions-height
                    :pref-height (* cell-height item-count)
                    :max-height (min (* cell-height max-visible-completions-count) max-completions-height)
                    :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                   :describe completion-list-cell-view}}}}}]
               completions-shortcut-text
               (conj {:fx/type fx.label/lifecycle
                      :style-class "completion-popup-hint"
                      :pref-height hint-height
                      :min-height hint-height
                      :max-height hint-height
                      :pref-width completions-width
                      :max-width completions-width
                      :min-width completions-width
                      :text (str completions-shortcut-text ": Toggle doc popup")}))}]}}
         :desc
         {:fx/type fx/ext-many
          :desc
          (cond->
            [{:fx/type fxui/with-popup-window
              :desc {:fx/type fxui/ext-value :value canvas}
              :popup
              {:fx/type fx.popup/lifecycle
               :anchor-x (- (.getX anchor) 12.0)
               :anchor-y (- (.getY anchor) 4.0)
               :anchor-location :window-top-left
               :showing completions-showing
               :auto-fix false
               :auto-hide true
               :on-auto-hide {:event :auto-hide}
               :hide-on-escape false
               :content [{:fx/type fx/ext-get-ref :ref :content}]}}]
            completions-doc
            (conj
              (let [pref-doc-width 350.0
                    doc-max-height (- (.getMaxY screen-bounds) (.getY anchor))
                    spacing 6.0
                    left-space (- (.getX anchor) spacing)
                    right-space (- (.getMaxX screen-bounds)
                                   (+ (.getX anchor) completions-width spacing))
                    align-right (or (<= pref-doc-width right-space)
                                    (<= left-space right-space))
                    doc-width (if align-right
                                (min pref-doc-width right-space)
                                (min pref-doc-width left-space))
                    {:keys [detail doc type tags]} selected-completion
                    deprecated (contains? tags :deprecated)
                    small-text (or detail (some-> type name (string/replace "-" " ")))
                    small-string (when (or deprecated small-text)
                                   (format "<small>%s</small>"
                                           (cond
                                             (and small-text deprecated) (str small-text " (deprecated)")
                                             small-text small-text
                                             deprecated "deprecated")))
                    doc-string (when doc
                                 (case (:type doc)
                                   :markdown (:value doc)
                                   :plaintext (format "<pre>%s</pre>" (:value doc))))]
                {:fx/type fxui/with-popup-window
                 :desc {:fx/type fx/ext-get-ref :ref :content}
                 :popup
                 {:fx/type fx.popup/lifecycle
                  :anchor-x (let [x (- (.getX anchor) 12.0)]
                              (if align-right
                                (+ x completions-width spacing)
                                (- x doc-width spacing)))
                  :anchor-y (- (.getY anchor) 5.0)
                  :anchor-location :window-top-left
                  :showing true
                  :auto-fix false
                  :auto-hide false
                  :hide-on-escape false
                  :content [{:fx/type fx.stack-pane/lifecycle
                             :stylesheets [(str (io/resource "editor.css"))]
                             :children [{:fx/type fx.region/lifecycle
                                         :style-class "flat-list-doc-background"}
                                        (cond->
                                          {:fx/type markdown/view
                                           :base-url (:base-url doc)
                                           :event-filter {:event :doc-event-filter}
                                           :max-width doc-width
                                           :max-height doc-max-height
                                           :project project
                                           :content (cond
                                                      (and small-string doc-string) (str small-string "\n\n" doc-string)
                                                      small-string small-string
                                                      doc-string doc-string
                                                      :else "<small>no documentation</small>")}
                                          (not align-right)
                                          (assoc :min-width doc-width))]}]}})))}}))))

(defn- handle-completion-popup-event [view-node e]
  (case (:event e)
    :doc-event-filter
    (let [e (:fx/event e)]
      (when (instance? KeyEvent e)
        (let [^KeyEvent e e
              ^Node source (.getSource e)
              ^PopupWindow window (.getWindow (.getScene source))
              target (.getFocusOwner (.getScene (.getOwnerWindow window)))]
          (ui/send-event! target e)
          (.consume e))))

    :auto-hide
    (hide-suggestions! view-node)

    :completion-list-view-event-filter
    (let [e (:fx/event e)]
      (when (instance? KeyEvent e)
        (let [^KeyEvent e e
              code (.getCode e)]
          ;; redirect everything except arrows to canvas
          (when-not (or (= KeyCode/UP code)
                        (= KeyCode/DOWN code)
                        (= KeyCode/PAGE_UP code)
                        (= KeyCode/PAGE_DOWN code))
            (ui/send-event! (get-property view-node :canvas) e)
            (.consume e)))))

    :select
    (do
      (set-properties! view-node nil {:completions-selected-index (:fx/event e)})
      (resolve-selected-completion! view-node))
    :accept
    (accept-suggestion! view-node (code-completion/insertion (:completion e)))))

;; -----------------------------------------------------------------------------

(defn move! [view-node move-type cursor-type]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/move (get-property view-node :lines)
                              (get-property view-node :cursor-ranges)
                              (get-property view-node :layout)
                              move-type
                              cursor-type)))

(defn page-up! [view-node move-type]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-up (get-property view-node :lines)
                                 (get-property view-node :cursor-ranges)
                                 (get-property view-node :scroll-y)
                                 (get-property view-node :layout)
                                 move-type)))

(defn page-down! [view-node move-type]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-down (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :scroll-y)
                                   (get-property view-node :layout)
                                   move-type)))

(defn delete! [view-node prefs delete-type]
  (let [cursor-ranges (get-property view-node :cursor-ranges)
        cursor-ranges-empty (every? data/cursor-range-empty? cursor-ranges)
        single-character-backspace (and cursor-ranges-empty (= :delete-before delete-type))
        undo-grouping (when cursor-ranges-empty :typing)
        delete-fn (case delete-type
                    :delete-before data/delete-character-before-cursor
                    :delete-word-before data/delete-word-before-cursor
                    :delete-after data/delete-character-after-cursor
                    :delete-word-after data/delete-word-after-cursor)]
    (set-properties!
      view-node undo-grouping
      (g/with-auto-evaluation-context evaluation-context
        (data/delete (get-property view-node :lines evaluation-context)
                     (get-property view-node :grammar evaluation-context)
                     (prefs/get prefs [:code :auto-closing-parens])
                     (get-current-syntax-info (get-property view-node :resource-node evaluation-context))
                     cursor-ranges
                     (get-property view-node :regions evaluation-context)
                     (get-property view-node :layout evaluation-context)
                     delete-fn)))
    (hide-hover! view-node)
    (if (and single-character-backspace
             (suggestions-shown? view-node)
             (implies-completions? view-node))
      (show-suggestions! view-node)
      (hide-suggestions! view-node))))

(defn toggle-comment! [view-node]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/toggle-comment (get-property view-node :lines)
                                        (get-property view-node :regions)
                                        (get-property view-node :cursor-ranges)
                                        (:line-comment (get-property view-node :grammar)))))

(defn select-all! [view-node]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/select-all (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges))))

(defn select-next-occurrence! [view-node]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/select-next-occurrence (get-property view-node :lines)
                                                (get-property view-node :cursor-ranges)
                                                (get-property view-node :layout))))

(defn- indent! [view-node]
  (hide-hover! view-node)
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
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/deindent (get-property view-node :lines)
                                  (get-property view-node :cursor-ranges)
                                  (get-property view-node :regions)
                                  (get-property view-node :tab-spaces))))

(defn- tab! [view-node]
  (cond
    (suggestions-visible? view-node)
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

(handler/defhandler :code.tab-forward :code-view
  (active? [editable] editable)
  (run [view-node] (tab! view-node)))

(handler/defhandler :code.tab-backward :code-view
  (active? [editable] editable)
  (run [view-node] (shift-tab! view-node)))

(handler/defhandler :code.show-completions :code-view
  (active? [editable] editable)
  (run [view-node] (show-suggestions! view-node :explicit true)))

;; -----------------------------------------------------------------------------

(defn- insert-text! [view-node prefs typed]
  (let [undo-grouping (if (= "\r" typed) :newline :typing)
        selected-suggestion (selected-suggestion view-node)
        grammar (get-property view-node :grammar)
        [insert-typed show-suggestions]
        (cond
          (and selected-suggestion
               (or (= "\r" typed)
                   (contains? (:commit-characters selected-suggestion) typed)))
          (let [insertion (code-completion/insertion selected-suggestion)]
            (do (accept-suggestion! view-node insertion)
                [;; insert-typed
                 (not
                   ;; exclude typed when...
                   (or (= typed "\r")
                       ;; At this point, we know we typed a commit character.
                       ;; If there are tab stops, and we typed a character
                       ;; before the tab stop, we assume the commit character
                       ;; is a shortcut for accepting a completion and jumping
                       ;; into the tab stop, e.g. foo($1) + "(" => don't
                       ;; insert. Otherwise, we insert typed, e.g.:
                       ;; - foo($1) + "{" => the typed character is expected
                       ;;   to be inside the tab stop, for example, when foo
                       ;;   expects a hash map
                       ;; - vmath + "." => the typed character is expected
                       ;;   to be after the snippet.
                       (when-let [^long i (->> insertion :tab-triggers first :ranges first first)]
                         (when (pos? i)
                           (= typed (subs (:insert-string insertion) (dec i) i))))))
                 ;; show-suggestions
                 (not
                   ;; hide suggestions when entering new scope
                   (#{"[" "(" "{"} typed))]))

          (data/typing-deindents-line? grammar (get-property view-node :lines) (get-property view-node :cursor-ranges) typed)
          [true false]

          :else
          [true true])]
    (when insert-typed
      (when (set-properties!
              view-node undo-grouping
              (g/with-auto-evaluation-context evaluation-context
                (data/key-typed (get-property view-node :indent-level-pattern evaluation-context)
                                (get-property view-node :indent-string evaluation-context)
                                grammar
                                (prefs/get prefs [:code :auto-closing-parens])
                                ;; NOTE: don't move :lines and :cursor-ranges
                                ;; to the let above the
                                ;; [insert-typed show-suggestion] binding:
                                ;; accepting suggestions might change them!
                                (get-property view-node :lines evaluation-context)
                                (get-property view-node :cursor-ranges evaluation-context)
                                (get-property view-node :regions evaluation-context)
                                (get-property view-node :layout evaluation-context)
                                (get-current-syntax-info (get-property view-node :resource-node evaluation-context))
                                typed)))
        (hide-hover! view-node)
        (if (and show-suggestions (implies-completions? view-node))
          (show-suggestions! view-node :typed typed)
          (hide-suggestions! view-node))))))

(defn handle-key-pressed! [view-node prefs ^KeyEvent event editable]
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
                  KeyCode/ENTER (when editable
                                  (insert-text! view-node prefs "\r"))
                  ::unhandled))
      (.consume event))))

(defn handle-key-typed! [view-node prefs ^KeyEvent event]
  (.consume event)
  (let [character (.getCharacter event)]
    (when (and (keymap/typable? event)
               ;; Ignore Alt+Space on macOS
               (not (and (os/is-mac-os?) (= " " character) (.isAltDown event)))
               ;; Ignore characters in the control range and the ASCII delete
               ;; as it is done by JavaFX in `TextInputControlBehavior`'s
               ;; `defaultKeyTyped` method.
               (pos? (.length character))
               (> (int (.charAt character 0)) 0x1f)
               (not= (int (.charAt character 0)) 0x7f))
      (insert-text! view-node prefs character))))

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

                 (some? (:on-click! (:region hovered-element)))
                 javafx.scene.Cursor/HAND

                 (data/rect-contains? (.canvas layout) x y)
                 javafx.scene.Cursor/TEXT

                 :else
                 javafx.scene.Cursor/DEFAULT)]
    (ui/set-cursor node cursor)))

(handler/register-menu! ::code-context-menu
  [{:command :edit.cut :label (localization/message "command.edit.cut")}
   {:command :edit.copy :label (localization/message "command.edit.copy")}
   {:command :edit.paste :label (localization/message "command.edit.paste")}
   {:command :code.select-all :label (localization/message "command.code.select-all")}
   (menu-items/separator-with-id :editor.app-view/edit-end)])

(defn handle-mouse-pressed! [view-node ^MouseEvent event]
  (let [^Node target (.getTarget event)
        scene ^Scene (.getScene target)
        button (mouse-button event)
        layout ^LayoutInfo (get-property view-node :layout)]
    (.consume event)
    (.requestFocus target)
    (refresh-mouse-cursor! view-node event)
    (hide-hover! view-node)
    (hide-suggestions! view-node)
    (set-properties! view-node (if (< 1 (.getClickCount event)) :selection :navigation)
                     (data/mouse-pressed (get-property view-node :lines)
                                         (get-property view-node :cursor-ranges)
                                         (get-property view-node :regions)
                                         layout
                                         (get-property view-node :minimap-layout)
                                         button
                                         (.getClickCount event)
                                         (.getX event)
                                         (.getY event)
                                         (.isAltDown event)
                                         (.isShiftDown event)
                                         (.isShortcutDown event)))

    (when (and (= button :secondary)
               (not (data/in-gutter? layout (.getX event))))
      (let [context-menu (ui/init-context-menu! ::code-context-menu scene)]
        (.show context-menu target (.getScreenX event) (.getScreenY event))))))

(defn- on-hover-response [view-node request-cursor hover-lsp-regions]
  (ui/run-later
    (when (g/node-exists? view-node)
      (let [current-cursor (get-property view-node :hover-cursor)
            showing-hover-cursor (get-property view-node :hover-showing-cursor)]
        (when-let [properties (coll/merge
                                (when (= current-cursor request-cursor)
                                  {:hover-cursor-lsp-regions (mapv #(assoc % :hoverable false) hover-lsp-regions)})
                                (when (= showing-hover-cursor request-cursor)
                                  {:hover-showing-lsp-regions hover-lsp-regions}))]
          (set-properties! view-node nil properties))))))

(defn- refresh-hover-state! [view-node]
  (when (g/node-exists? view-node)
    (set-properties!
      view-node nil
      (g/with-auto-evaluation-context evaluation-context
        (if-let [hover-cursor (when (some-> ^Canvas (get-property view-node :canvas evaluation-context) .getScene .getWindow .isFocused)
                                (get-property view-node :hover-cursor evaluation-context))]
          {:hover-showing-cursor hover-cursor
           :hover-showing-lsp-regions (mapv #(assoc % :hoverable true) (get-property view-node :hover-cursor-lsp-regions evaluation-context))}
          {:hover-showing-cursor nil
           :hover-showing-lsp-regions nil
           :hover-mouse-over-popup false})))))

(defn- schedule-hover-refresh!
  "Returns properties to set"
  ([view-node]
   (g/with-auto-evaluation-context evaluation-context
     (schedule-hover-refresh! view-node evaluation-context)))
  ([view-node evaluation-context]
   (some-> (g/node-value view-node :hover-request evaluation-context) ui/cancel)
   (let [delay (if (hover-visible? view-node evaluation-context) 0.2 1.0)]
     {:hover-request (ui/->future delay #(refresh-hover-state! view-node))})))

(defn- request-lsp-hover! [view-node lsp resource-node new-hover-cursor evaluation-context]
  (let [old-hover-cursor (get-property view-node :hover-cursor evaluation-context)
        _ (when (and new-hover-cursor (not= old-hover-cursor new-hover-cursor))
            (let [resource (g/node-value resource-node :resource evaluation-context)]
              (lsp/hover! lsp resource new-hover-cursor #(on-hover-response view-node new-hover-cursor %))))
        ret (if (g/node-value view-node :hover-showing-cursor evaluation-context)
              ;; we are showing a hover
              (when (not= old-hover-cursor new-hover-cursor)
                (let [hover-showing-region (g/node-value view-node :hover-showing-region evaluation-context)
                      was-in (and (some? old-hover-cursor)
                                  (data/cursor-range-contains-exclusive? hover-showing-region old-hover-cursor))
                      is-in (and (some? new-hover-cursor)
                                 (data/cursor-range-contains-exclusive? hover-showing-region new-hover-cursor))]
                  ;; when moving inside (is-in), re-request a refresh on every mouse move
                  ;; when just moved outside (was-in), request a refresh once
                  (when (or is-in was-in)
                    (schedule-hover-refresh! view-node evaluation-context))))
              ;; we are not showing a hover: schedule a refresh request
              (schedule-hover-refresh! view-node evaluation-context))]
    ;; save the new hover cursor in the graph if it changed
    (cond-> ret (not= old-hover-cursor new-hover-cursor) (assoc :hover-cursor new-hover-cursor))))

(def ^:private hover-pref-path [:code :hover])

(defn handle-mouse-moved! [view-node prefs ^MouseDragEvent event]
  (.consume event)
  (set-properties!
    view-node :selection
    (g/with-auto-evaluation-context evaluation-context
      (let [layout (get-property view-node :layout evaluation-context)
            lines (get-property view-node :lines evaluation-context)
            x (.getX event)
            y (.getY event)
            resource-node (get-property view-node :resource-node evaluation-context)
            lsp (lsp/get-node-lsp (:basis evaluation-context) resource-node)
            row (data/y->row layout y)]
        (-> (data/mouse-moved (get-property view-node :lines evaluation-context)
                              (get-property view-node :cursor-ranges evaluation-context)
                              (get-property view-node :visible-regions evaluation-context)
                              layout
                              (get-property view-node :minimap-layout evaluation-context)
                              (get-property view-node :gesture-start evaluation-context)
                              (get-property view-node :hovered-element evaluation-context)
                              (get-property view-node :hovered-row evaluation-context)
                              x
                              y)
            (cond->
              (and lsp
                (prefs/get prefs hover-pref-path)
                (not (get-property view-node :hover-mouse-over-popup evaluation-context)))
              (merge
                (let [hover-character-cursor (data/canvas->character-cursor layout lines x y)]
                  (request-lsp-hover! view-node lsp resource-node hover-character-cursor evaluation-context))))))))
  (refresh-mouse-cursor! view-node event))

(defn handle-mouse-released! [view-node ^MouseEvent event]
  (.consume event)
  (when-some [hovered-region (:region (get-property view-node :hovered-element))]
    (when-some [on-click! (:on-click! hovered-region)]
      (on-click! hovered-region event)))
  (refresh-mouse-cursor! view-node event)
  (set-properties! view-node :selection
                   (data/mouse-released (get-property view-node :lines)
                                        (get-property view-node :cursor-ranges)
                                        (get-property view-node :visible-regions)
                                        (get-property view-node :layout)
                                        (get-property view-node :minimap-layout)
                                        (get-property view-node :gesture-start)
                                        (mouse-button event)
                                        (get-property view-node :hovered-row)
                                        (.getX event)
                                        (.getY event))))

(defn handle-mouse-exited! [view-node ^MouseEvent event]
  (.consume event)
  (set-properties!
    view-node :selection
    (g/with-auto-evaluation-context evaluation-context
      (coll/merge
        (data/mouse-exited (get-property view-node :gesture-start evaluation-context)
                           (get-property view-node :hovered-element evaluation-context))
        (when-not (get-property view-node :hover-mouse-over-popup evaluation-context)
          (assoc (schedule-hover-refresh! view-node evaluation-context) :hover-cursor nil))))))

(defn handle-scroll! [view-node zoom-on-scroll ^ScrollEvent event]
  (.consume event)
  (when (if (.isShortcutDown event)
          (do (when zoom-on-scroll
                (-> (g/node-value view-node :canvas)
                    (ui/run-command (cond (pos? (.getDeltaY event)) :code.zoom.increase
                                          (neg? (.getDeltaY event)) :code.zoom.decrease))))
              true)
          (set-properties! view-node :navigation
                           (data/scroll (get-property view-node :lines)
                                        (get-property view-node :scroll-x)
                                        (get-property view-node :scroll-y)
                                        (get-property view-node :layout)
                                        (get-property view-node :gesture-start)
                                        (.getDeltaX event)
                                        (.getDeltaY event))))
    (hide-hover! view-node)
    (hide-suggestions! view-node)))

;; -----------------------------------------------------------------------------

(defn has-selection? [view-node evaluation-context]
  (not-every? data/cursor-range-empty?
              (get-property view-node :cursor-ranges evaluation-context)))

(defn can-paste? [view-node clipboard evaluation-context]
  (data/can-paste? (get-property view-node :cursor-ranges evaluation-context) clipboard))

(defn cut! [view-node clipboard]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/cut! (get-property view-node :lines)
                              (get-property view-node :cursor-ranges)
                              (get-property view-node :regions)
                              (get-property view-node :layout)
                              clipboard)))

(defn copy! [view-node clipboard]
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/copy! (get-property view-node :lines)
                               (get-property view-node :cursor-ranges)
                               clipboard)))

(defn paste! [view-node clipboard]
  (hide-hover! view-node)
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
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/split-selection-into-lines (get-property view-node :lines)
                                                    (get-property view-node :cursor-ranges))))

(handler/defhandler :edit.cut :code-view
  (active? [editable] editable)
  (enabled? [view-node evaluation-context]
            (has-selection? view-node evaluation-context))
  (run [view-node clipboard] (cut! view-node clipboard)))

(handler/defhandler :edit.paste :code-view
  (active? [editable] editable)
  (enabled? [view-node clipboard evaluation-context]
            (can-paste? view-node clipboard evaluation-context))
  (run [view-node clipboard] (paste! view-node clipboard)))

(handler/defhandler :code.delete-next-char :code-view
  (active? [editable] editable)
  (run [view-node prefs] (delete! view-node prefs :delete-after)))

(handler/defhandler :code.toggle-comment :code-view
  (active? [editable view-node evaluation-context]
           (and editable
                (contains? (get-property view-node :grammar evaluation-context) :line-comment)))
  (run [view-node] (toggle-comment! view-node)))

(handler/defhandler :code.delete-previous-char :code-view
  (active? [editable] editable)
  (run [view-node prefs] (delete! view-node prefs :delete-before)))

(handler/defhandler :code.delete-previous-word :code-view
  (active? [editable] editable)
  (run [view-node prefs] (delete! view-node prefs :delete-word-before)))

(handler/defhandler :code.delete-next-word :code-view
  (active? [editable] editable)
  (run [view-node prefs] (delete! view-node prefs :delete-word-after)))

(handler/defhandler :debugger.toggle-breakpoint :code-view
  (run [view-node]
    (let [lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          regions (get-property view-node :regions)
          breakpoint-rows (data/cursor-ranges->start-rows lines cursor-ranges)]
      (set-properties! view-node nil
                       (data/toggle-breakpoint-region lines
                                                      regions
                                                      breakpoint-rows)))))

(handler/defhandler :debugger.toggle-breakpoint-enabled :code-view
  (run [view-node]
    (let [lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          regions (get-property view-node :regions)
          breakpoint-rows (data/cursor-ranges->start-rows lines cursor-ranges)]
      (set-properties! view-node nil (data/toggle-breakpoint-enabled-region lines regions breakpoint-rows)))))

(handler/defhandler :edit.rename :code-view
  (active? [editable] editable)
  (run [view-node]
    (g/with-auto-evaluation-context evaluation-context
      (let [resource-node (get-property view-node :resource-node)
            lsp (lsp/get-node-lsp (:basis evaluation-context) resource-node)
            resource (g/node-value resource-node :resource)
            cursor (data/CursorRange->Cursor (first (get-property view-node :cursor-ranges)))]
        (lsp/prepare-rename
          lsp resource cursor
          (fn on-prepare-rename-response [maybe-rename-range]
            (when maybe-rename-range
              (ui/run-later
                (set-properties! view-node nil {:rename-cursor-range maybe-rename-range})))))))))

(handler/defhandler :debugger.edit-breakpoint :code-view
  (run [view-node]
    (let [lines (get-property view-node :lines)
          cursor-ranges (get-property view-node :cursor-ranges)
          regions (get-property view-node :regions)]
      (set-properties!
        view-node
        nil
        (data/edit-breakpoint-from-single-cursor-range lines cursor-ranges regions)))))

(handler/defhandler :code.reindent :code-view
  (active? [editable] editable)
  (enabled? [view-node evaluation-context]
            (not-every? data/cursor-range-empty?
                        (get-property view-node :cursor-ranges evaluation-context)))
  (run [view-node]
       (set-properties! view-node nil
                        (data/reindent (get-property view-node :indent-level-pattern)
                                       (get-property view-node :indent-string)
                                       (get-property view-node :grammar)
                                       (get-property view-node :lines)
                                       (get-property view-node :cursor-ranges)
                                       (get-property view-node :regions)
                                       (get-property view-node :layout)))))

(handler/defhandler :code.convert-indentation :code-view
  (label [user-data]
    (case user-data
      :tabs (localization/message "command.code.convert-indentation.to-tabs")
      :two-spaces (localization/message "command.code.convert-indentation.to-two-spaces")
      :four-spaces (localization/message "command.code.convert-indentation.to-four-spaces")
      nil (localization/message "command.code.convert-indentation")))
  (options [user-data]
    (when-not user-data
      [{:label (localization/message "command.code.convert-indentation.to-tabs")
        :command :code.convert-indentation
        :user-data :tabs}
       {:label (localization/message "command.code.convert-indentation.to-two-spaces")
        :command :code.convert-indentation
        :user-data :two-spaces}
       {:label (localization/message "command.code.convert-indentation.to-four-spaces")
        :command :code.convert-indentation
        :user-data :four-spaces}]))
  (active? [editable] editable)
  (run [view-node user-data]
       (set-properties! view-node nil
                        (data/convert-indentation (get-property view-node :indent-type)
                                                  user-data
                                                  (get-property view-node :lines)
                                                  (get-property view-node :cursor-ranges)
                                                  (get-property view-node :regions)))))

(defn- show-goto-popup! [view-node open-resource-fn results]
  (g/with-auto-evaluation-context evaluation-context
    (let [cursor (data/CursorRange->Cursor (first (get-property view-node :cursor-ranges evaluation-context)))
          ^Canvas canvas (get-property view-node :canvas evaluation-context)
          ^LayoutInfo layout (get-property view-node :layout evaluation-context)
          lines (get-property view-node :lines evaluation-context)
          cursor-bottom (cursor-bottom layout lines cursor)
          font-name (get-property view-node :font-name evaluation-context)
          font-size (get-property view-node :font-size evaluation-context)
          list-view (popup/make-choices-list-view
                      (data/line-height (.-glyph layout))
                      (fn [{:keys [resource cursor-range]}]
                        {:graphic
                         (doto (Text.
                                 (str (resource/proj-path resource)
                                      ":"
                                      (data/CursorRange->line-number cursor-range)))
                           (.setFont (Font/font font-name font-size)))}))
          [width height] (popup/update-list-view! list-view 200.0 results 0)
          anchor (pref-suggestions-popup-position canvas width height cursor-bottom)
          popup (doto (popup/make-choices-popup canvas list-view)
                  (.addEventHandler Event/ANY (ui/event-handler event (.consume event))))
          accept! (fn [{:keys [resource cursor-range]}]
                    (.hide popup)
                    (open-resource-fn resource {:cursor-range cursor-range}))]
      (hide-hover! view-node)
      (hide-suggestions! view-node)
      (doto list-view
        ui/apply-css!
        (.setOnMouseClicked
          (ui/event-handler event
            (when-let [item (ui/cell-item-under-mouse event)]
              (accept! item))))
        (.setOnKeyPressed
          (ui/event-handler event
            (let [^KeyEvent event event]
              (when (and (= KeyEvent/KEY_PRESSED (.getEventType event)))
                (condp = (.getCode event)
                  KeyCode/ENTER (accept! (first (ui/selection list-view)))
                  KeyCode/ESCAPE (.hide popup)
                  nil))))))
      (.show popup (.getWindow (.getScene canvas)) (.getX anchor) (.getY anchor))
      (popup/update-list-view! list-view 200.0 results 0))))

(defn- show-no-language-server-for-resource-language-notification! [resource]
  (let [language (resource/language resource)]
    (notifications/show!
      (workspace/notifications (resource/workspace resource))
      {:type :warning
       :id [::no-lsp language]
       :message (localization/message "notification.lsp.language-server-missing.prompt" {"language" language})
       :actions [{:message (localization/message "notification.lsp.language-server-missing.action.about")
                  :on-action #(ui/open-url "https://forum.defold.com/t/linting-in-the-code-editor/72465")}]})))

(handler/defhandler :code.goto-definition :code-view
  (enabled? [view-node evaluation-context]
    (let [resource-node (get-property view-node :resource-node evaluation-context)
          resource (g/node-value resource-node :resource evaluation-context)]
      (resource/file-resource? resource)))
  (run [view-node user-data open-resource-fn]
    (let [resource-node (get-property view-node :resource-node)
          resource (g/node-value resource-node :resource)
          lsp (lsp/get-node-lsp resource-node)]
      (if (lsp/has-language-servers-running-for-language? lsp (resource/language resource))
        (lsp/goto-definition!
          lsp
          resource
          (data/CursorRange->Cursor (first (get-property view-node :cursor-ranges)))
          (fn [results]
            (fx/on-fx-thread
              (case (count results)
                0 nil
                1 (open-resource-fn
                    (:resource (first results))
                    {:cursor-range (:cursor-range (first results))})
                (show-goto-popup! view-node open-resource-fn results)))))
        (show-no-language-server-for-resource-language-notification! resource)))))

(handler/defhandler :code.show-references :code-view
  (enabled? [view-node evaluation-context]
    (let [resource-node (get-property view-node :resource-node evaluation-context)
          resource (g/node-value resource-node :resource evaluation-context)]
      (resource/file-resource? resource)))
  (run [view-node user-data open-resource-fn]
    (let [resource-node (get-property view-node :resource-node)
          lsp (lsp/get-node-lsp resource-node)
          resource (g/node-value resource-node :resource)]
      (if (lsp/has-language-servers-running-for-language? lsp (resource/language resource))
        (lsp/find-references!
          lsp
          resource
          (data/CursorRange->Cursor (first (get-property view-node :cursor-ranges)))
          (fn [results]
            (when (pos? (count results))
              (fx/on-fx-thread
                (show-goto-popup! view-node open-resource-fn results)))))
        (show-no-language-server-for-resource-language-notification! resource)))))

;; -----------------------------------------------------------------------------
;; Sort Lines
;; -----------------------------------------------------------------------------

(defn- can-sort-lines? [view-node evaluation-context]
  (some data/cursor-range-multi-line? (get-property view-node :cursor-ranges evaluation-context)))

(defn- sort-lines! [view-node sort-key-fn]
  (set-properties! view-node nil
                   (data/sort-lines (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges)
                                    (get-property view-node :regions)
                                    sort-key-fn)))

(handler/defhandler :code.sort-lines :code-view
  (label [user-data]
    (case user-data
      :case-insensitive (localization/message "command.code.sort-lines")
      :case-sensitive (localization/message "command.code.sort-lines.case-sensitive")
      nil (localization/message "command.code.sort-lines")))
  (options [user-data]
    (when-not user-data
      [{:label (localization/message "command.code.sort-lines.option.case-insensitive")
        :command :code.sort-lines
        :user-data :case-insensitive}
       {:label (localization/message "command.code.sort-lines.option.case-sensitive")
        :command :code.sort-lines
        :user-data :case-sensitive}]))
  (active? [editable] editable)
  (enabled? [view-node evaluation-context] (can-sort-lines? view-node evaluation-context))
  (run [view-node user-data]
    (sort-lines! view-node (case user-data
                             :case-insensitive string/lower-case
                             :case-sensitive identity))))

;; -----------------------------------------------------------------------------
;; Properties shared among views
;; -----------------------------------------------------------------------------

;; WARNING:
;; Observing or binding to an observable that lives longer than the observer will
;; cause a memory leak. You must manually unhook them or use weak listeners.
;; Source: https://community.oracle.com/message/10360893#10360893

(defonce ^:private ^SimpleObjectProperty bar-ui-type-property (SimpleObjectProperty. :hidden))
(defonce ^:private ^SimpleStringProperty find-term-property (SimpleStringProperty. ""))
(defonce ^:private ^SimpleStringProperty find-replacement-property (SimpleStringProperty. ""))
(defonce ^:private ^SimpleBooleanProperty find-whole-word-property (SimpleBooleanProperty. false))
(defonce ^:private ^SimpleBooleanProperty find-case-sensitive-property (SimpleBooleanProperty. false))
(defonce ^:private ^SimpleBooleanProperty find-wrap-property (SimpleBooleanProperty. true))
(defonce ^:private ^SimpleDoubleProperty font-size-property (SimpleDoubleProperty. default-font-size))
(defonce ^:private ^SimpleStringProperty font-name-property (SimpleStringProperty. ""))
(defonce ^:private ^SimpleBooleanProperty visible-indentation-guides-property (SimpleBooleanProperty. true))
(defonce ^:private ^SimpleBooleanProperty visible-minimap-property (SimpleBooleanProperty. true))
(defonce ^:private ^SimpleBooleanProperty visible-whitespace-property (SimpleBooleanProperty. true))

(defonce ^:private ^ObjectBinding highlighted-find-term-property
  (b/if (b/or (b/= :find bar-ui-type-property)
              (b/= :replace bar-ui-type-property))
    find-term-property
    ""))

(defn- init-property-and-bind-preference! [^Property property prefs preference]
  (.setValue property (prefs/get prefs preference))
  (ui/observe property (fn [_ _ new] (prefs/set! prefs preference new))))

(defn initialize! [prefs]
  (init-property-and-bind-preference! find-term-property prefs [:code :find :term])
  (init-property-and-bind-preference! find-replacement-property prefs [:code :find :replacement])
  (init-property-and-bind-preference! find-whole-word-property prefs [:code :find :whole-word])
  (init-property-and-bind-preference! find-case-sensitive-property prefs [:code :find :case-sensitive])
  (init-property-and-bind-preference! find-wrap-property prefs [:code :find :wrap])
  (init-property-and-bind-preference! font-size-property prefs [:code :font :size])
  (init-property-and-bind-preference! font-name-property prefs [:code :font :name])
  (init-property-and-bind-preference! visible-indentation-guides-property prefs [:code :visibility :indentation-guides])
  (init-property-and-bind-preference! visible-minimap-property prefs [:code :visibility :minimap])
  (init-property-and-bind-preference! visible-whitespace-property prefs [:code :visibility :whitespace])
  (when (clojure.string/blank? (.getValue font-name-property))
    (.setValue font-name-property "Dejavu Sans Mono")))

;; -----------------------------------------------------------------------------
;; View Settings
;; -----------------------------------------------------------------------------

(handler/defhandler :code.zoom.decrease :code-view-tools
  (enabled? [] (<= 4.0 ^double (.getValue font-size-property)))
  (run [] (when (<= 4.0 ^double (.getValue font-size-property))
            (.setValue font-size-property (dec ^double (.getValue font-size-property))))))

(handler/defhandler :code.zoom.increase :code-view-tools
  (enabled? [] (>= 32.0 ^double (.getValue font-size-property)))
  (run [] (when (>= 32.0 ^double (.getValue font-size-property))
            (.setValue font-size-property (inc ^double (.getValue font-size-property))))))

(handler/defhandler :code.zoom.reset :code-view-tools
  (run [] (.setValue font-size-property default-font-size)))

(handler/defhandler :code.toggle-indentation-guides :code-view-tools
  (run [] (.setValue visible-indentation-guides-property (not (.getValue visible-indentation-guides-property))))
  (state [] (.getValue visible-indentation-guides-property)))

(handler/defhandler :code.toggle-minimap :code-view-tools
  (run [] (.setValue visible-minimap-property (not (.getValue visible-minimap-property))))
  (state [] (.getValue visible-minimap-property)))

(handler/defhandler :code.toggle-visible-whitespace :code-view-tools
  (run [] (.setValue visible-whitespace-property (not (.getValue visible-whitespace-property))))
  (state [] (.getValue visible-whitespace-property)))

;; -----------------------------------------------------------------------------
;; Go to Line
;; -----------------------------------------------------------------------------

(defn- bar-ui-visible? []
  (not= :hidden (.getValue bar-ui-type-property)))

(defn- set-bar-ui-type! [ui-type]
  (case ui-type (:hidden :goto-line :find :replace) (.setValue bar-ui-type-property ui-type)))

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

(defn- try-parse-goto-line-text
  ([view-node text]
   (g/with-auto-evaluation-context evaluation-context
     (try-parse-goto-line-text view-node text evaluation-context)))
  ([view-node ^String text evaluation-context]
   (let [lines (get-property view-node :lines evaluation-context)]
     (try-parse-row (count lines) text))))

(defn- try-parse-goto-line-bar-row
  ([view-node goto-line-bar]
   (g/with-auto-evaluation-context evaluation-context
     (try-parse-goto-line-bar-row view-node goto-line-bar evaluation-context)))
  ([view-node ^GridPane goto-line-bar evaluation-context]
   (ui/with-controls goto-line-bar [^TextField line-field]
     (let [maybe-row (try-parse-goto-line-text view-node (.getText line-field) evaluation-context)]
       (when (number? maybe-row)
         maybe-row)))))

(defn- setup-goto-line-bar! [^GridPane goto-line-bar view-node localization]
  (b/bind-presence! goto-line-bar (b/= :goto-line bar-ui-type-property))
  (ui/with-controls goto-line-bar [^TextField line-field ^Button go-button]
    (ui/bind-keys! goto-line-bar {KeyCode/ENTER :private/goto-entered-line})
    (ui/bind-action! go-button :private/goto-entered-line)
    (ui/observe (.textProperty line-field)
                (fn [_ _ line-field-text]
                  (g/let-ec [can-go (ui/bound-action-enabled? go-button evaluation-context)]
                    (ui/enable! go-button can-go))
                  (let [maybe-row (try-parse-goto-line-text view-node line-field-text)
                        error-message (when-not (number? maybe-row) maybe-row)]
                    (assert (or (nil? error-message) (string? error-message)))
                    (ui/tooltip! line-field error-message localization)
                    (if (some? error-message)
                      (ui/add-style! line-field "field-error")
                      (ui/remove-style! line-field "field-error"))))))
  (doto goto-line-bar
    (ui/context! :goto-line-bar {:goto-line-bar goto-line-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1)))

(defn- dispose-goto-line-bar! [^GridPane goto-line-bar]
  (b/unbind! (.visibleProperty goto-line-bar)))

(handler/defhandler :code.goto-line :code-view-tools
  (run [goto-line-bar]
       (set-bar-ui-type! :goto-line)
       (ui/with-controls goto-line-bar [^TextField line-field]
         (.requestFocus line-field)
         (.selectAll line-field))))

(handler/defhandler :private/goto-entered-line :goto-line-bar
  (enabled? [goto-line-bar view-node evaluation-context]
            (some? (try-parse-goto-line-bar-row view-node goto-line-bar evaluation-context)))
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

(defn- setup-find-bar! [^GridPane find-bar view-node localization]
  (doto find-bar
    (b/bind-presence! (b/= :find bar-ui-type-property))
    (ui/context! :code-view-find-bar {:find-bar find-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^Button next ^Button prev]
    (localization/localize! whole-word localization (localization/message "code.find.word"))
    (localization/localize! case-sensitive localization (localization/message "code.find.case"))
    (localization/localize! wrap localization (localization/message "code.find.wrap"))
    (localization/localize! next localization (localization/message "code.find.next"))
    (localization/localize! prev localization (localization/message "code.find.prev"))
    (b/bind-bidirectional! (.textProperty term) find-term-property)
    (b/bind-bidirectional! (.selectedProperty whole-word) find-whole-word-property)
    (b/bind-bidirectional! (.selectedProperty case-sensitive) find-case-sensitive-property)
    (b/bind-bidirectional! (.selectedProperty wrap) find-wrap-property)
    (ui/bind-key-commands! find-bar {"Enter" :code.find-next
                                     "Shift+Enter" :code.find-previous})
    (ui/bind-action! next :code.find-next)
    (ui/bind-action! prev :code.find-previous))
  find-bar)

(defn- dispose-find-bar! [^GridPane find-bar]
  (b/unbind! (.visibleProperty find-bar))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term]
    (b/unbind-bidirectional! (.textProperty term) find-term-property)
    (b/unbind-bidirectional! (.selectedProperty whole-word) find-whole-word-property)
    (b/unbind-bidirectional! (.selectedProperty case-sensitive) find-case-sensitive-property)
    (b/unbind-bidirectional! (.selectedProperty wrap) find-wrap-property)))

(defn- setup-replace-bar! [^GridPane replace-bar view-node editable localization]
  (doto replace-bar
    (b/bind-presence! (b/= :replace bar-ui-type-property))
    (ui/context! :code-view-replace-bar {:editable editable :replace-bar replace-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1))
  (ui/with-controls replace-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^TextField replacement ^Button next ^Button replace ^Button replace-all]
    (localization/localize! whole-word localization (localization/message "code.find.word"))
    (localization/localize! case-sensitive localization (localization/message "code.find.case"))
    (localization/localize! wrap localization (localization/message "code.find.wrap"))
    (localization/localize! next localization (localization/message "code.find.next"))
    (localization/localize! replace localization (localization/message "code.replace.next"))
    (localization/localize! replace-all localization (localization/message "code.replace.all"))
    (b/bind-bidirectional! (.textProperty term) find-term-property)
    (b/bind-bidirectional! (.textProperty replacement) find-replacement-property)
    (b/bind-bidirectional! (.selectedProperty whole-word) find-whole-word-property)
    (b/bind-bidirectional! (.selectedProperty case-sensitive) find-case-sensitive-property)
    (b/bind-bidirectional! (.selectedProperty wrap) find-wrap-property)
    (ui/bind-action! next :code.find-next)
    (ui/bind-action! replace :code.replace-next)
    (ui/bind-keys! replace-bar {KeyCode/ENTER :code.replace-next})
    (ui/bind-action! replace-all :code.replace-all))
  replace-bar)

(defn- dispose-replace-bar! [^GridPane replace-bar]
  (b/unbind! (.visibleProperty replace-bar))
  (ui/with-controls replace-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^TextField replacement]
    (b/unbind-bidirectional! (.textProperty term) find-term-property)
    (b/unbind-bidirectional! (.textProperty replacement) find-replacement-property)
    (b/unbind-bidirectional! (.selectedProperty whole-word) find-whole-word-property)
    (b/unbind-bidirectional! (.selectedProperty case-sensitive) find-case-sensitive-property)
    (b/unbind-bidirectional! (.selectedProperty wrap) find-wrap-property)))

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
  (hide-hover! view-node)
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
  (hide-hover! view-node)
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
  (hide-hover! view-node)
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
  (hide-hover! view-node)
  (hide-suggestions! view-node)
  (let [^String find-term (.getValue find-term-property)]
    (when (pos? (.length find-term))
      (set-properties! view-node nil
                       (data/replace-all (get-property view-node :lines)
                                         (get-property view-node :regions)
                                         (get-property view-node :layout)
                                         (split-lines find-term)
                                         (split-lines (.getValue find-replacement-property))
                                         (.getValue find-case-sensitive-property)
                                         (.getValue find-whole-word-property))))))

(handler/defhandler :edit.find :code-view
  (run [find-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-bar-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :code.replace-text :code-view
  (active? [editable] editable)
  (run [replace-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-bar-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :edit.find :code-view-tools ;; In practice, from find / replace and go to line bars.
  (run [find-bar]
       (set-bar-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :code.replace-text :code-view-tools
  (active? [editable] editable)
  (run [replace-bar]
       (set-bar-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :code.escape :code-view-tools
  (run [find-bar replace-bar view-node]
       (cond
         (in-tab-trigger? view-node)
         (exit-tab-trigger! view-node)

         (hover-visible? view-node)
         (hide-hover! view-node)

         (suggestions-visible? view-node)
         (hide-suggestions! view-node)

         (bar-ui-visible?)
         (do (set-bar-ui-type! :hidden)
             (focus-code-editor! view-node))

         :else
         (set-properties! view-node :selection
                          (data/escape (get-property view-node :cursor-ranges))))))

(handler/defhandler :code.find-next :code-view-find-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :code.find-next :code-view-replace-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :code.find-next :code-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :code.find-previous :code-view-find-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :code.find-previous :code-view-replace-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :code.find-previous :code-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :code.replace-next :code-view-replace-bar
  (active? [editable] editable)
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :code.replace-next :code-view
  (active? [editable] editable)
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :code.replace-all :code-view-replace-bar
  (active? [editable] editable)
  (run [view-node] (replace-all! view-node)))

;; -----------------------------------------------------------------------------

(handler/register-menu! ::menubar-edit :editor.app-view/edit-end
  [{:command :edit.find :label (localization/message "command.edit.find")}
   {:command :code.find-next :label (localization/message "command.code.find-next")}
   {:command :code.find-previous :label (localization/message "command.code.find-previous")}
   {:label :separator}
   {:command :code.replace-text :label (localization/message "command.code.replace")}
   {:command :code.replace-next :label (localization/message "command.code.replace-next")}
   {:label :separator}
   {:command :code.toggle-comment :label (localization/message "command.code.toggle-comment")}
   {:command :code.reindent :label (localization/message "command.code.reindent-lines")}
   {:command :code.convert-indentation :expand true}
   {:label :separator}
   {:command :code.sort-lines :user-data :case-insensitive}
   {:command :code.sort-lines :user-data :case-sensitive}
   {:label :separator}
   {:command :code.select-next-occurrence :label (localization/message "command.code.select-next-occurrence")}
   {:command :code.split-selection-into-lines :label (localization/message "command.code.split-selection-into-lines")}
   {:label :separator}
   {:command :edit.rename :label (localization/message "command.edit.rename")}
   {:command :code.goto-definition :label (localization/message "command.code.goto-definition")}
   {:command :code.show-references :label (localization/message "command.code.show-references")}
   {:label :separator}
   {:command :debugger.toggle-breakpoint :label (localization/message "command.debugger.toggle-breakpoint")}
   {:command :debugger.toggle-breakpoint-enabled :label (localization/message "command.debugger.toggle-breakpoint-enabled")}
   {:command :debugger.edit-breakpoint :label (localization/message "command.debugger.edit-breakpoint")}])

(handler/register-menu! ::menubar-view :editor.app-view/view-end
  [{:command :code.toggle-minimap :label (localization/message "command.code.toggle-minimap") :check true}
   {:command :code.toggle-indentation-guides :label (localization/message "command.code.toggle-indentation-guides") :check true}
   {:command :code.toggle-visible-whitespace :label (localization/message "command.code.toggle-visible-whitespace") :check true}
   {:label :separator}
   {:command :code.zoom.increase :label (localization/message "command.code.zoom.increase")}
   {:command :code.zoom.decrease :label (localization/message "command.code.zoom.decrease")}
   {:command :code.zoom.reset :label (localization/message "command.code.zoom.reset")}
   {:label :separator}
   {:command :code.goto-line :label (localization/message "command.code.goto-line")}])

;; -----------------------------------------------------------------------------

(defn- setup-view! [resource-node view-node app-view lsp]
  ;; Grab the unmodified lines or io error before opening the
  ;; file. Otherwise, this will happen on the first edit. If a
  ;; background process has modified (or even deleted) the file
  ;; without the editor knowing, the "original" unmodified lines
  ;; reached after a series of undo's could be something else entirely
  ;; than what the user saw.
  (let [resource (resource-node/resource resource-node)
        resource-type (resource/resource-type resource)
        resource-node-type (:node-type resource-type)
        is-code-resource-type (r/code-resource-type? resource-type)]
    (when is-code-resource-type
      (g/with-auto-evaluation-context evaluation-context
        (r/ensure-loaded! resource-node evaluation-context)))
    (g/transact
      (concat
        (g/connect app-view :debugger-execution-locations view-node :debugger-execution-locations)
        (gu/connect-existing-outputs resource-node-type resource-node view-node
          [[:completions :completions]
           [:cursor-ranges :cursor-ranges]
           [:indent-type :indent-type]
           [:invalidated-rows :invalidated-rows]
           [:lines :lines]
           [:regions :regions]])))
    (g/transact
      (g/with-auto-evaluation-context evaluation-context
        (update-document-width evaluation-context view-node)))
    (when (and is-code-resource-type
               (resource/file-resource? resource))
      (let [lines (g/node-value view-node :lines)]
        (lsp/open-view! lsp view-node resource lines)))
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

(defn- plaintext->markdown [s]
  (str "<p>"
       (string/replace s #"[<>]" {"<" "&lt;" ">" "&gt;"})
       "</p>"))

(defn- handle-hover-popup-mouse-entered! [view-node _]
  (some-> (get-property view-node :hover-request) ui/cancel)
  (set-properties! view-node nil {:hover-cursor nil :hover-mouse-over-popup true}))

(defn- handle-hover-popup-mouse-exited! [view-node _]
  (set-properties! view-node nil (assoc (schedule-hover-refresh! view-node) :hover-mouse-over-popup false)))

(defn- handle-hover-popup-on-hidden! [view-node _]
  (set-properties! view-node nil {:hover-mouse-over-popup false}))

(defn handle-hover-popup-auto-hide! [view-node _]
  (hide-hover! view-node))

(defn- hover-popup-view [props]
  (let [{:keys [canvas-repaint-info project screen-bounds
                hover-showing-regions hover-showing-region view-node]} props
        {:keys [^Canvas canvas layout lines]} canvas-repaint-info
        ^Rect r (->> hover-showing-region
                     (data/adjust-cursor-range lines)
                     (data/cursor-range-rects layout lines)
                     first)
        anchor (.localToScreen canvas
                               (min (max 0.0 (.-x r)) (.getWidth canvas))
                               (min (max 0.0 (.-y r)) (.getHeight canvas)))
        ^Rectangle2D screen-rect (coll/some #(when (.contains ^Rectangle2D % anchor) %)
                                            screen-bounds)
        max-popup-height (- (.getY anchor) (.getMinY screen-rect))]
    {:fx/type fxui/with-popup-window
     :desc {:fx/type fxui/ext-value :value canvas}
     :popup
     {:fx/type fx.popup/lifecycle
      :showing true
      :anchor-x (.getX anchor)
      :anchor-y (.getY anchor)
      :anchor-location :window-bottom-left
      :auto-hide true
      :on-auto-hide (fn/partial handle-hover-popup-auto-hide! view-node)
      :on-hidden (fn/partial handle-hover-popup-on-hidden! view-node)
      :auto-fix true
      :hide-on-escape true
      :consume-auto-hiding-events true
      :content
      [{:fx/type fx.stack-pane/lifecycle
        :stylesheets [(str (io/resource "editor.css"))]
        :on-mouse-entered (fn/partial handle-hover-popup-mouse-entered! view-node)
        :on-mouse-exited (fn/partial handle-hover-popup-mouse-exited! view-node)
        :children
        [{:fx/type fx.region/lifecycle
          :min-width 10
          :min-height 10
          :style-class "hover-background"}
         {:fx/type markdown/view
          :content (->> hover-showing-regions
                        (e/mapcat
                          (fn [region]
                            (when (:hoverable region)
                              (case (:type region)
                                :diagnostic (map plaintext->markdown (:messages region))
                                :hover [(let [{:keys [content]} region
                                              {:keys [type value]} content]
                                          (case type
                                            :plaintext (plaintext->markdown value)
                                            :markdown value))]))))
                        (coll/join-to-string "\n\n<hr>\n\n"))
          :project project
          :max-width 350.0
          :max-height max-popup-height}]}]}}))

(defn repaint-view! [view-node elapsed-time {:keys [cursor-visible editable] :as _opts}]
  (assert (boolean? cursor-visible))

  ;; Since the elapsed time updates at 60 fps, we store it as user-data to avoid transaction churn.
  (g/user-data! view-node :elapsed-time elapsed-time)

  ;; Perform necessary property updates in preparation for repaint.
  (g/let-ec [tick-props (data/tick (g/node-value view-node :lines evaluation-context)
                                   (g/node-value view-node :layout evaluation-context)
                                   (g/node-value view-node :gesture-start evaluation-context))
             props (if-not cursor-visible
                     tick-props
                     (let [elapsed-time-at-last-action (g/node-value view-node :elapsed-time-at-last-action evaluation-context)
                           old-cursor-opacity (g/node-value view-node :cursor-opacity evaluation-context)
                           new-cursor-opacity (cursor-opacity elapsed-time-at-last-action elapsed-time)]
                       (cond-> tick-props
                               (not= old-cursor-opacity new-cursor-opacity)
                               (assoc :cursor-opacity new-cursor-opacity))))]
    (set-properties! view-node nil props))

  ;; Repaint the view.
  (g/let-ec [prev-canvas-repaint-info (g/user-data view-node :canvas-repaint-info)
             prev-cursor-repaint-info (g/user-data view-node :cursor-repaint-info)
             resource-node (g/node-value view-node :resource-node evaluation-context)
             canvas-repaint-info (g/node-value view-node :canvas-repaint-info evaluation-context)
             cursor-repaint-info (g/node-value view-node :cursor-repaint-info evaluation-context)]

    ;; Repaint canvas if needed.
    (when-not (identical? prev-canvas-repaint-info canvas-repaint-info)
      (g/user-data! view-node :canvas-repaint-info canvas-repaint-info)
      (let [row (data/last-visible-row (:minimap-layout canvas-repaint-info))]
        (repaint-canvas! canvas-repaint-info (get-valid-syntax-info resource-node canvas-repaint-info row))))

    (when editable
      (g/with-auto-evaluation-context evaluation-context
        ;; Show rename popup if appropriate
        (fxui/advance-graph-user-data-component!
          view-node :rename-popup
          (when (g/node-value view-node :rename-cursor-range evaluation-context)
            (g/node-value view-node :rename-view evaluation-context)))
        ;; Show completion suggestions if appropriate.
        (let [renderer (or (g/user-data view-node :completion-popup-renderer)
                           (g/user-data!
                             view-node
                             :completion-popup-renderer
                             (fx/create-renderer
                               :error-handler error-reporting/report-exception!
                               :middleware (comp
                                             fxui/wrap-dedupe-desc
                                             (fx/wrap-map-desc #'completion-popup-view))
                               :opts {:fx.opt/map-event-handler #(handle-completion-popup-event view-node %)})))
              ^Canvas canvas (:canvas canvas-repaint-info)]
          (renderer {:completions-combined (g/node-value view-node :completions-combined evaluation-context)
                     :completions-selected-index (g/node-value view-node :completions-selected-index evaluation-context)
                     :completions-showing (g/node-value view-node :completions-showing evaluation-context)
                     :completions-doc (g/node-value view-node :completions-doc evaluation-context)
                     :completions-shortcut-text (g/node-value view-node :completions-shortcut-text evaluation-context)
                     :project (g/node-value view-node :project evaluation-context)
                     :canvas-repaint-info canvas-repaint-info
                     :visible-completion-ranges (g/node-value view-node :visible-completion-ranges evaluation-context)
                     :query (:query (g/node-value view-node :completion-context evaluation-context))
                     :font (g/node-value view-node :font evaluation-context)
                     :font-name (g/node-value view-node :font-name evaluation-context)
                     :font-size (g/node-value view-node :font-size evaluation-context)
                     :window-x (some-> (.getScene canvas) .getWindow .getX)
                     :window-y (some-> (.getScene canvas) .getWindow .getY)
                     :screen-bounds (mapv #(.getVisualBounds ^Screen %) (Screen/getScreens))}))))

    ;; Repaint hovered regions
    (g/with-auto-evaluation-context evaluation-context
      (let [hover-renderer (or (g/user-data view-node :hover-popup-renderer)
                               (g/user-data!
                                 view-node
                                 :hover-popup-renderer
                                 (fx/create-renderer
                                   :error-handler error-reporting/report-exception!
                                   :middleware (comp fxui/wrap-dedupe-desc
                                                     (fx/wrap-map-desc #'hover-popup-view)))))
            hover-showing-regions (g/node-value view-node :hover-showing-regions evaluation-context)]
        (hover-renderer
          (when hover-showing-regions
            (let [scene (.getScene ^Canvas (:canvas canvas-repaint-info))]
              {:hover-showing-regions hover-showing-regions
               :hover-showing-region (g/node-value view-node :hover-showing-region evaluation-context)
               :canvas-repaint-info canvas-repaint-info
               :project (g/node-value view-node :project evaluation-context)
               :view-node view-node
               :screen-bounds (mapv #(.getVisualBounds ^Screen %) (Screen/getScreens))
               :window-x (some-> scene .getWindow .getX)
               :window-y (some-> scene .getWindow .getY)})))))

    ;; Repaint cursors if needed.
    (when-not (identical? prev-cursor-repaint-info cursor-repaint-info)
      (g/user-data! view-node :cursor-repaint-info cursor-repaint-info)
      (repaint-cursors! cursor-repaint-info))

    ;; Draw average fps indicator if enabled.
    (when-some [^PerformanceTracker performance-tracker @*performance-tracker]
      (let [{:keys [^Canvas canvas ^long repaint-trigger]} canvas-repaint-info]
        (g/set-property! view-node :repaint-trigger (unchecked-inc repaint-trigger))
        (draw-fps-counters! (.getGraphicsContext2D canvas) (.getInstantFPS performance-tracker))
        (when (= 0 (mod repaint-trigger 10))
          (.resetAverageFPS performance-tracker))))))

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

(defonce/type CodeEditorGutterView []
  GutterView

  (gutter-metrics [this lines regions glyph-metrics]
    (let [gutter-margin (data/line-height glyph-metrics)]
      (data/gutter-metrics glyph-metrics gutter-margin (count lines))))

  (draw-gutter! [this gc gutter-rect layout hovered-ui-element font color-scheme lines regions visible-cursors hovered-row]
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
            indicator-offset 3.0
            indicator-diameter (- line-height indicator-offset indicator-offset)
            breakpoint-row->condition (coll/into-> regions {}
                                        (filter data/breakpoint-region?)
                                        (map (juxt data/breakpoint-row
                                                   #(-> {:condition (:condition % true)
                                                         :enabled (:enabled %)}))))
            execution-markers-by-type (group-by :location-type (filter data/execution-marker? regions))
            execution-marker-current-rows (data/cursor-ranges->start-rows lines (:current-line execution-markers-by-type))
            execution-marker-frame-rows (data/cursor-ranges->start-rows lines (:current-frame execution-markers-by-type))]
        (loop [drawn-line-index 0
               source-line-index dropped-line-count]
          (when (and (< drawn-line-index drawn-line-count)
                     (< source-line-index source-line-count))
            (let [y (data/row->y layout source-line-index)
                  breakpoint (breakpoint-row->condition source-line-index)
                  hovered? (and (= hovered-ui-element :gutter)
                                (= hovered-row source-line-index))]
              (when (and hovered? (nil? breakpoint))
                (.setFill gc ^Color (.deriveColor ^Color gutter-breakpoint-color 0.0 1.0 1.0 0.3))
                (.fillOval gc
                           (+ (.x line-numbers-rect) (.w line-numbers-rect) indicator-offset)
                           (+ y indicator-offset) indicator-diameter indicator-diameter))
              (when breakpoint
                (if (:enabled breakpoint)
                  (do
                    (.setFill gc gutter-breakpoint-color)
                    (.fillOval gc
                               (+ (.x line-numbers-rect) (.w line-numbers-rect) indicator-offset)
                               (+ y indicator-offset) indicator-diameter indicator-diameter))
                  (do
                    (.setStroke gc gutter-breakpoint-color)
                    (.setLineWidth gc 2)
                    (.setFill gc gutter-background-color)
                    (.fillOval gc
                               (+ (.x line-numbers-rect) (.w line-numbers-rect) indicator-offset)
                               (+ y indicator-offset) indicator-diameter indicator-diameter)
                    (.strokeOval gc
                                 (+ (.x line-numbers-rect) (.w line-numbers-rect) indicator-offset 1)
                                 (+ y indicator-offset 1) (- indicator-diameter 2) (- indicator-diameter 2))))
                (when (string? (:condition breakpoint))
                  (doto gc
                    (.save)
                    (.setFill gutter-background-color)
                    (.translate
                      ;; align to the center of the breakpoint indicator
                      (+ (.x line-numbers-rect)
                         (.w line-numbers-rect)
                         indicator-offset
                         (* indicator-diameter 0.5))
                      (+ y indicator-offset (* indicator-diameter 0.5)))
                    ;; magic scaling constant to make the icon fit
                    (.scale (/ indicator-diameter 180.0) (/ indicator-diameter 180.0))
                    (.beginPath)
                    ;; The following SVG path is taken from here:
                    ;; https://uxwing.com/question-mark-icon/
                    ;; License: All icons are free to use any personal and commercial projects without any attribution or credit
                    ;; Then, the path was edited on this svg editor site:
                    ;; https://yqnn.github.io/svg-path-editor/
                    ;; I translated it up and to the left so the center of the question mark is on 0,0
                    (.appendSVGPath "M 15.68 24.96 H -15.63 V 21.84 C -15.63 16.52 -15.04 12.19 -13.83 8.87 C -12.62 5.52 -10.82 2.51 -8.43 -0.24 C -6.04 -3 -0.67 -7.84 7.69 -14.76 C 12.14 -18.39 14.36 -21.71 14.36 -24.72 C 14.36 -27.76 13.46 -30.09 11.69 -31.78 C 9.89 -33.44 7.19 -34.28 3.56 -34.28 C -0.35 -34.28 -3.56 -32.99 -6.12 -30.4 C -8.68 -27.84 -10.31 -23.31 -11.02 -16.9 L -43 -20.87 C -41.9 -32.63 -37.63 -42.08 -30.2 -49.26 C -22.75 -56.43 -11.32 -60 4.06 -60 C 16.04 -60 25.69 -57.5 33.06 -52.52 C 43.05 -45.74 48.06 -36.74 48.06 -25.48 C 48.06 -20.81 46.77 -16.28 44.18 -11.95 C 41.62 -7.62 36.33 -2.3 28.37 3.94 C 22.83 8.36 19.31 11.87 17.85 14.55 C 16.42 17.19 15.68 20.68 15.68 24.96 L 15.68 24.96 Z M -16.72 33.29 H 16.84 V 62.89 H -16.72 V 33.29 L -16.72 33.29 Z")
                    (.fill)
                    (.restore))))
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
  (^ChangeListener [node-id prop-kw]
   (make-property-change-setter node-id prop-kw identity))
  (^ChangeListener [node-id prop-kw observable-value->node-value]
   (assert (integer? node-id))
   (assert (keyword? prop-kw))
   (reify ChangeListener
     (changed [_this _observable _old new]
       (g/set-property! node-id prop-kw (observable-value->node-value new))))))

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

;; JavaFX generally reports wrong key-typed events when typing tilde on Swedish
;; keyboard layout, which is a problem when writing Lua because it uses ~ for negation,
;; so typing "AltGr ` =" inserts "~¨=" instead of "~="
;; Original JavaFX issue: https://bugs.openjdk.java.net/browse/JDK-8183521
;; See also: https://github.com/javafxports/openjdk-jfx/issues/358

(defn- wrap-disallow-diaeresis-after-tilde
  ^EventHandler [^EventHandler handler]
  (let [last-event-volatile (volatile! nil)]
    (ui/event-handler event
      (let [^KeyEvent new-event event
            ^KeyEvent prev-event @last-event-volatile]
        (vreset! last-event-volatile event)
        (when-not (and (some? prev-event)
                       (= "~" (.getCharacter prev-event))
                       (= "¨" (.getCharacter new-event)))
          (.handle handler event))))))

(defn handle-input-method-changed! [view-node prefs ^InputMethodEvent e]
  (let [x (.getCommitted e)]
    (when-not (.isEmpty x)
      (insert-text! view-node prefs ({"≃" "~="} x x)))))

(defn- setup-input-method-requests! [^Canvas canvas view-node prefs]
  (when-not (os/is-win32?)
    (doto canvas
      (.setInputMethodRequests
        (reify InputMethodRequests
          (getTextLocation [_this _offset] Point2D/ZERO)
          (getLocationOffset [_this _x _y] 0)
          (cancelLatestCommittedText [_this] "")
          (getSelectedText [_this] "")))
      (.setOnInputMethodTextChanged
        (ui/event-handler e
          (handle-input-method-changed! view-node prefs e))))))

(defn- consume-breakpoint-popup-events [^Event e]
  (when-not (and (instance? KeyEvent e)
                 (= KeyEvent/KEY_PRESSED (.getEventType e))
                 (= KeyCode/ESCAPE (.getCode ^KeyEvent e)))
    (.consume e)))

(defn- breakpoint-editor-view [^Canvas canvas {:keys [edited-breakpoint gutter-metrics layout]}]
  (let [[^double gutter-width ^double gutter-margin] gutter-metrics
        spacing 4
        padding 8
        anchor (.localToScreen canvas
                               (- gutter-width
                                  (* gutter-margin 0.5)
                                  12) ;; shadow offset
                               (+ (* ^double (data/line-height (:glyph layout)) 0.5)
                                  (data/row->y layout (data/breakpoint-row edited-breakpoint))))]
    {:fx/type fxui/with-popup-window
     :desc {:fx/type fxui/ext-value :value (.getWindow (.getScene canvas))}
     :popup
     {:fx/type fx.popup/lifecycle
      :showing true
      :anchor-x (.getX anchor)
      :anchor-y (.getY anchor)
      :anchor-location :window-top-left
      :auto-hide true
      :auto-fix true
      :hide-on-escape true
      :on-auto-hide {:event :cancel}
      :consume-auto-hiding-events true
      :event-handler consume-breakpoint-popup-events
      :content
      [{:fx/type fx.v-box/lifecycle
        :stylesheets [(str (io/resource "editor.css"))]
        :children
        [{:fx/type fx.v-box/lifecycle
          :style-class "breakpoint-editor"
          :fill-width false
          :spacing -1
          :children
          [{:fx/type fx.region/lifecycle
            :view-order -1
            :style-class "breakpoint-editor-arrow"
            :min-width 10
            :min-height 10}
           {:fx/type fx.stack-pane/lifecycle
            :children
            [{:fx/type fx.region/lifecycle
              :style-class "breakpoint-editor-background"}
             {:fx/type fx.grid-pane/lifecycle
              :style-class "breakpoint-editor-content"
              :column-constraints [{:fx/type fx.column-constraints/lifecycle :percent-width 20}
                                   {:fx/type fx.column-constraints/lifecycle :percent-width 80 :fill-width true}]
              :children
              [{:fx/type fx.label/lifecycle
                :style-class ["label" "breakpoint-editor-label" "breakpoint-editor-header"]
                :text (format "Breakpoint on line %d" (data/CursorRange->line-number edited-breakpoint))
                :grid-pane/column 0
                :grid-pane/row 0
                :grid-pane/column-span 2}
               {:fx/type fx.label/lifecycle
                :style-class ["label" "breakpoint-editor-label"]
                :text "Enabled"
                :grid-pane/column 0
                :grid-pane/row 1}
               {:fx/type fx.check-box/lifecycle
                :style-class ["check-box" "breakpoint-editor-checkbox"]
                :selected (get edited-breakpoint :enabled true)
                :on-selected-changed {:event :toggle-enabled}
                :grid-pane/column 1
                :grid-pane/row 1}
               {:fx/type fx.label/lifecycle
                :style-class ["label" "breakpoint-editor-label"]
                :text "Condition"
                :grid-pane/column 0
                :grid-pane/row 2}
               {:fx/type fxui/ext-focused-by-default
                :grid-pane/column 1
                :grid-pane/row 2
                :desc {:fx/type fx.text-field/lifecycle
                       :style-class ["text-field" "breakpoint-editor-label"]
                       :prompt-text "e.g. i == 1"
                       :text (:condition edited-breakpoint "")
                       :on-text-changed {:event :edit}
                       :on-action {:event :apply}}}
               {:fx/type fx.button/lifecycle
                :style-class ["button" "breakpoint-editor-button"]
                :text "OK"
                :on-action {:event :apply}
                :grid-pane/column 1
                :grid-pane/row 3
                :grid-pane/halignment :right}]}]}]}]}]}}))

(defn- create-breakpoint-editor! [view-node canvas ^Tab tab]
  (let [state (atom nil)
        timer (ui/->timer
                10
                "breakpoint-code-editor-timer"
                (fn [_ _ _]
                  (when (and (.isSelected tab) (not (ui/ui-disabled?)))
                    (g/with-auto-evaluation-context evaluation-context
                      (reset! state
                              (when-let [edited-breakpoint (g/node-value view-node :edited-breakpoint evaluation-context)]
                                {:edited-breakpoint edited-breakpoint
                                 :gutter-metrics (g/node-value view-node :gutter-metrics evaluation-context)
                                 :layout (g/node-value view-node :layout evaluation-context)}))))))]
    (fx/mount-renderer
      state
      (fx/create-renderer
        :error-handler error-reporting/report-exception!
        :opts {:fx.opt/map-event-handler
               (fn [event]
                 (g/with-auto-evaluation-context evaluation-context
                   (when-let [edited-breakpoint (g/node-value view-node :edited-breakpoint evaluation-context)]
                     (set-properties!
                       view-node
                       nil
                       (case (:event event)
                         :edit
                         {:edited-breakpoint (assoc edited-breakpoint :condition (:fx/event event))}

                         :cancel
                         {:edited-breakpoint nil}

                         :toggle-enabled
                         (let [edited-breakpoint (assoc edited-breakpoint :enabled (:fx/event event))]
                           (assoc (data/ensure-breakpoint-region
                                    (g/node-value view-node :lines evaluation-context)
                                    (g/node-value view-node :regions evaluation-context)
                                    edited-breakpoint)
                                  :edited-breakpoint edited-breakpoint))

                         :apply
                         (assoc (data/ensure-breakpoint-region
                                  (g/node-value view-node :lines evaluation-context)
                                  (g/node-value view-node :regions evaluation-context)
                                  edited-breakpoint)
                                :edited-breakpoint nil)))
                     (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true))))}
        :middleware (comp
                      fxui/wrap-dedupe-desc
                      (fx/wrap-map-desc #(breakpoint-editor-view canvas %)))))
    (ui/timer-start! timer)
    (fn dispose-breakpoint-editor! []
      (ui/timer-stop! timer)
      (reset! state nil))))

(defn- make-view! [graph parent resource-node opts]
  (let [{:keys [^Tab tab app-view grammar open-resource-fn project prefs localization]} opts
        basis (g/now)
        resource-node-type (g/node-type* basis resource-node)
        editable (g/has-property? resource-node-type :modified-lines)
        grid (GridPane.)
        canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        undo-grouping-info (pair :navigation (gensym))
        lsp (lsp/get-node-lsp basis resource-node)
        view-node
        (setup-view!
          resource-node
          (-> (g/make-nodes graph
                [view [CodeEditorView
                       :canvas canvas
                       :color-scheme code-color-scheme
                       :font-size (.getValue font-size-property)
                       :font-name (.getValue font-name-property)
                       :grammar grammar
                       :gutter-view (->CodeEditorGutterView)
                       :highlighted-find-term (.getValue highlighted-find-term-property)
                       :line-height-factor 1.2
                       :undo-grouping-info undo-grouping-info
                       :visible-indentation-guides? (.getValue visible-indentation-guides-property)
                       :visible-minimap? (.getValue visible-minimap-property)
                       :visible-whitespace (boolean->visible-whitespace (.getValue visible-whitespace-property))]]
                (g/connect project :_node-id view :project)
                (g/connect app-view :keymap view :keymap)
                (g/connect app-view :open-views view :open-views))
              g/transact
              g/tx-nodes-added
              first)
          app-view
          lsp)
        goto-line-bar (setup-goto-line-bar! (ui/load-fxml "goto-line.fxml") view-node localization)
        find-bar (setup-find-bar! (ui/load-fxml "find.fxml") view-node localization)
        replace-bar (setup-replace-bar! (ui/load-fxml "replace.fxml") view-node editable localization)
        repainter (ui/->timer "repaint-code-editor-view"
                              (fn [_ elapsed-time _]
                                (when (and (.isSelected tab) (not (ui/ui-disabled?)))
                                  (repaint-view! view-node elapsed-time {:cursor-visible true :editable editable}))))
        dispose-breakpoint-editor! (create-breakpoint-editor! view-node canvas tab)
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :editable editable
                     :goto-line-bar goto-line-bar
                     :find-bar find-bar
                     :replace-bar replace-bar
                     :view-node view-node
                     :prefs prefs
                     :open-resource-fn open-resource-fn}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (b/bind! (.widthProperty canvas) (.widthProperty canvas-pane))
    (b/bind! (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.setCursor javafx.scene.Cursor/TEXT)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (handle-key-pressed! view-node prefs event editable)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (handle-mouse-moved! view-node prefs event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (handle-mouse-moved! view-node prefs event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (handle-mouse-released! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_EXITED (ui/event-handler event (handle-mouse-exited! view-node event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (handle-scroll! view-node (prefs/get prefs [:code :zoom-on-scroll]) event))))

    (when editable
      (doto canvas
        (setup-input-method-requests! view-node prefs)
        (.addEventHandler KeyEvent/KEY_TYPED (wrap-disallow-diaeresis-after-tilde
                                               (ui/event-handler event (handle-key-typed! view-node prefs event))))))

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
                    ;; Must run-later here since we're not part of the Scene when we observe the property change.
                    ;; Also note that we don't want to steal focus from the inactive tab pane, if present.
                    (ui/run-later
                      (when (identical? (.getTabPane tab)
                                        (g/node-value app-view :active-tab-pane))
                        (.requestFocus canvas))))))

    ;; Highlight occurrences of search term while find bar is open.
    (let [find-case-sensitive-setter (make-property-change-setter view-node :find-case-sensitive?)
          find-whole-word-setter (make-property-change-setter view-node :find-whole-word?)
          font-size-setter (make-property-change-setter view-node :font-size)
          highlighted-find-term-setter (make-property-change-setter view-node :highlighted-find-term)
          visible-indentation-guides-setter (make-property-change-setter view-node :visible-indentation-guides?)
          visible-minimap-setter (make-property-change-setter view-node :visible-minimap?)
          visible-whitespace-setter (make-property-change-setter view-node :visible-whitespace boolean->visible-whitespace)]
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
                             (lsp/close-view! lsp view-node)
                             (ui/kill-event-dispatch! canvas)
                             (ui/timer-stop! repainter)
                             (dispose-breakpoint-editor!)
                             (dispose-goto-line-bar! goto-line-bar)
                             (dispose-find-bar! find-bar)
                             (dispose-replace-bar! replace-bar)
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
    ;; Initial draw
    (ui/run-later (repaint-view! view-node 0 {:cursor-visible true :editable editable})
                  (ui/run-later (slog/smoke-log "code-view-visible")))
    view-node))

(def ^:private fundamental-read-only-handlers
  {[:view/code.select-up]
   {:run (g/fnk [view-node]
           (move! view-node :selection :up))}

   [:view/code.select-down]
   {:run (g/fnk [view-node]
           (move! view-node :selection :down))}

   [:view/code.select-left]
   {:run (g/fnk [view-node]
           (move! view-node :selection :left))}

   [:view/code.select-right]
   {:run (g/fnk [view-node]
           (move! view-node :selection :right))}

   [:view/code.goto-previous-word]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :prev-word))}

   [:view/code.select-previous-word]
   {:run (g/fnk [view-node]
           (move! view-node :selection :prev-word))}

   [:view/code.goto-next-word]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :next-word))}

   [:view/code.select-next-word]
   {:run (g/fnk [view-node]
           (move! view-node :selection :next-word))}

   [:view/code.goto-line-start]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :line-start))}

   [:view/code.select-line-start]
   {:run (g/fnk [view-node]
           (move! view-node :selection :line-start))}

   [:view/code.goto-line-text-start]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :home))}

   [:view/code.select-line-text-start]
   {:run (g/fnk [view-node]
           (move! view-node :selection :home))}

   [:view/code.goto-line-end]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :end))}

   [:view/code.select-line-end]
   {:run (g/fnk [view-node]
           (move! view-node :selection :end))}

   [:view/code.page-up]
   {:run (g/fnk [view-node]
           (page-up! view-node :navigation))}

   [:view/code.select-page-up]
   {:run (g/fnk [view-node]
           (page-up! view-node :selection))}

   [:view/code.page-down]
   {:run (g/fnk [view-node]
           (page-down! view-node :navigation))}

   [:view/code.select-page-down]
   {:run (g/fnk [view-node]
           (page-down! view-node :selection))}

   [:view/code.goto-file-start]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :file-start))}

   [:view/code.select-file-start]
   {:run (g/fnk [view-node]
           (move! view-node :selection :file-start))}

   [:view/code.goto-file-end]
   {:run (g/fnk [view-node]
           (move! view-node :navigation :file-end))}

   [:view/code.select-file-end]
   {:run (g/fnk [view-node]
           (move! view-node :selection :file-end))}

   [:view/edit.copy]
   {:enabled? (g/fnk [view-node evaluation-context]
                (has-selection? view-node evaluation-context))
    :run (g/fnk [view-node clipboard]
           (copy! view-node clipboard))}

   [:view/code.select-all]
   {:run (g/fnk [view-node]
           (select-all! view-node))}

   [:view/code.select-next-occurrence :tool/code.select-next-occurrence]
   {:run (g/fnk [view-node]
           (select-next-occurrence! view-node))}

   [:view/code.split-selection-into-lines]
   {:run (g/fnk [view-node]
           (split-selection-into-lines! view-node))}})

(defn register-fundamental-read-only-handlers!
  "Register UI handlers for fundamental navigation and selection actions. Shared
  between the code editor view and the console view."
  [ns view-context-definition tool-context-definition]
  (let [namespace-name (str ns)]
    (doseq [[prefixed-commands fnks] fundamental-read-only-handlers
            prefixed-command prefixed-commands]
      (let [prefix (keyword (namespace prefixed-command))
            command-name (name prefixed-command)
            command (keyword command-name)
            handler-id (keyword namespace-name command-name)]
        (assert (#{:tool :view} prefix))
        (when (= :view prefix)
          (handler/register-handler! handler-id command view-context-definition fnks))
        (when (= :tool prefix)
          (handler/register-handler! handler-id command tool-context-definition fnks))))))

(defn- focus-view! [view-node opts done-fn]
  (.requestFocus ^Node (g/node-value view-node :canvas))
  (when-some [cursor-range (:cursor-range opts)]
    (set-properties! view-node :navigation
                     (data/select-and-frame (get-property view-node :lines)
                                            (get-property view-node :layout)
                                            cursor-range)))
  (done-fn))

(defn register-view-types [workspace]
  (concat
    (workspace/register-view-type workspace
      :id :code
      :label (localization/message "resource.view.code")
      :make-view-fn make-view!
      :focus-fn focus-view!
      :text-selection-fn non-empty-single-selection-text)
    (workspace/register-view-type workspace
      :id :text
      :label (localization/message "resource.view.text")
      :make-view-fn make-view!
      :focus-fn focus-view!
      :text-selection-fn non-empty-single-selection-text)))

(register-fundamental-read-only-handlers! *ns* :code-view :code-view-tools)
