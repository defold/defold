(ns editor.code.view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :refer [pair split-lines]]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices-popup :as popup]
            [editor.util :as eutil]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [editor.handler :as handler]
            [internal.util :as util]
            [schema.core :as s])
  (:import (clojure.lang IPersistentVector)
           (com.defold.control ListView)
           (com.sun.javafx.tk FontLoader Toolkit)
           (com.sun.javafx.util Utils)
           (editor.code.data Cursor CursorRange GestureInfo LayoutInfo Rect)
           (java.util.regex Pattern)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleBooleanProperty SimpleStringProperty)
           (javafx.geometry HPos Point2D VPos)
           (javafx.scene Node Parent)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.control Button CheckBox PopupControl Tab TextField)
           (javafx.scene.input Clipboard DataFormat KeyCode KeyEvent MouseButton MouseDragEvent MouseEvent ScrollEvent)
           (javafx.scene.layout ColumnConstraints GridPane Pane Priority)
           (javafx.scene.paint Color LinearGradient Paint)
           (javafx.scene.text Font FontSmoothingType TextAlignment)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defrecord CursorRangeDrawInfo [type fill stroke cursor-range])

(defn- cursor-range-draw-info [type fill stroke cursor-range]
  (assert (case type (:range :word) true false))
  (assert (or (nil? fill) (instance? Paint fill)))
  (assert (or (nil? stroke) (instance? Paint stroke)))
  (assert (instance? CursorRange cursor-range))
  (->CursorRangeDrawInfo type fill stroke cursor-range))

(def ^:private undo-groupings #{:navigation :newline :selection :typing})
(g/deftype CursorRangeDrawInfos [CursorRangeDrawInfo])
(g/deftype UndoGroupingInfo [(s/one (s/->EnumSchema undo-groupings) "undo-grouping") (s/one s/Symbol "opseq")])
(g/deftype SyntaxInfo IPersistentVector)
(g/deftype SideEffect (s/eq nil))

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

(defn- tab-char? [c]
  (= \tab c))

(defrecord GlyphMetrics [^FontLoader font-loader ^Font font ^double line-height ^double ascent ^double tab-width]
  data/GlyphMetrics
  (ascent [_this] ascent)
  (line-height [_this] line-height)
  (string-width [_this text]
    ;; The computeStringWidth method returns zero width for tab characters.
    (let [width-without-tabs (.computeStringWidth font-loader text font)
          tab-count (count (filter tab-char? text))]
      (if (pos? tab-count)
        (+ width-without-tabs (* tab-count tab-width))
        width-without-tabs))))

(def ^:private ^Color foreground-color (Color/valueOf "#DDDDDD"))
(def ^:private ^Color background-color (Color/valueOf "#272B30"))
(def ^:private ^Color selection-background-color (Color/valueOf "#4E4A46"))
(def ^:private ^Color selection-occurrence-outline-color (Color/valueOf "#A2B0BE"))
(def ^:private ^Color tab-trigger-word-outline-color (Color/valueOf "#A2B0BE"))
(def ^:private ^Color find-term-occurrence-color (Color/valueOf "#60C1FF"))
(def ^:private ^Color gutter-foreground-color (Color/valueOf "#A2B0BE"))
(def ^:private ^Color gutter-background-color background-color)
(def ^:private ^Color gutter-cursor-line-background-color (Color/valueOf "#393C41"))
(def ^:private ^Color gutter-breakpoint-color (Color/valueOf "#AD4051"))
(def ^:private ^Color gutter-shadow-gradient (LinearGradient/valueOf "to right, rgba(0, 0, 0, 0.3) 0%, transparent 100%"))
(def ^:private ^Color scroll-tab-color (.deriveColor foreground-color 0 1 1 0.15))
(def ^:private ^Color visible-whitespace-color (.deriveColor foreground-color 0 1 1 0.2))

(def ^:private scope-colors
  [["comment" (Color/valueOf "#B0B0B0")]
   ["string" (Color/valueOf "#FBCE2F")]
   ["constant" (Color/valueOf "#AAAAFF")]
   ["keyword" (Color/valueOf "#FD6623")]
   ["support.function" (Color/valueOf "#33CCCC")]
   ["name.function" (Color/valueOf "#33CC33")]
   ["parameter.function" (Color/valueOf "#E3A869")]
   ["variable.language" (Color/valueOf "#E066FF")]])

(defn- scope->color
  ^Color [scope]
  (loop [scope-colors scope-colors]
    (if-some [[pattern color] (first scope-colors)]
      (if (string/includes? scope pattern)
        color
        (recur (next scope-colors)))
      foreground-color)))

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
      :word (let [^Rect r (data/expand-rect (first rects) 1.5 -0.5)]
              (assert (= 1 (count rects)))
              (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0))
      :range (doseq [^Rect r rects]
               (.fillRect gc (.x r) (.y r) (.w r) (.h r))))))

(defn- stroke-cursor-range! [^GraphicsContext gc type ^Paint _fill ^Paint stroke rects]
  (when (some? stroke)
    (.setStroke gc stroke)
    (.setLineWidth gc 1.0)
    (case type
      :word (let [^Rect r (data/expand-rect (first rects) 1.5 -0.5)]
              (assert (= 1 (count rects)))
              (.strokeRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0))
      :range (doseq [polyline (cursor-range-outline rects)]
               (let [[xs ys] polyline]
                 (.strokePolyline gc (double-array xs) (double-array ys) (count xs)))))))

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

(defn- draw! [^GraphicsContext gc ^Font font ^LayoutInfo layout lines syntax-info tab-spaces cursor-range-draw-infos breakpoint-rows visible-cursors visible-whitespace?]
  (let [source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double ascent (data/ascent (.glyph layout))
        ^double line-height (data/line-height (.glyph layout))
        tab-string (string/join (repeat tab-spaces \space))]
    (.setFill gc background-color)
    (.fillRect gc 0 0 (.. gc getCanvas getWidth) (.. gc getCanvas getHeight))
    (.setFont gc font)
    (.setFontSmoothingType gc FontSmoothingType/LCD)

    ;; Draw cursor ranges.
    (draw-cursor-ranges! gc layout lines cursor-range-draw-infos)

    ;; Draw syntax-highlighted code.
    (.setTextAlign gc TextAlignment/LEFT)
    (loop [drawn-line-index 0
           source-line-index dropped-line-count]
      (when (and (< drawn-line-index drawn-line-count)
                 (< source-line-index source-line-count))
        (let [line (lines source-line-index)
              line-x (+ (.x ^Rect (.canvas layout))
                        (.scroll-x layout))
              line-y (+ ascent
                        (.scroll-y-remainder layout)
                        (* drawn-line-index line-height))]
          (if-some [runs (second (get syntax-info source-line-index))]
            ;; Draw syntax highlighted runs.
            (loop [i 0
                   run-offset 0.0]
              (when-some [[start scope] (get runs i)]
                (let [end (or (first (get runs (inc i)))
                              (count line))
                      run-text (subs line start end)
                      ^double run-width (data/string-width (.glyph layout) run-text)]
                  (when-not (string/blank? run-text)
                    (.setFill gc (scope->color scope))
                    (.fillText gc (string/replace run-text #"\t" tab-string)
                               (+ line-x run-offset)
                               line-y))
                  (recur (inc i)
                         (+ run-offset run-width)))))

            ;; Just draw line as plain text.
            (when-not (string/blank? line)
              (.setFill gc foreground-color)
              (.fillText gc (string/replace line #"\t" tab-string) line-x line-y)))

          (when visible-whitespace?
            (.setFill gc visible-whitespace-color)
            (let [^double space-width (data/string-width (.glyph layout) " ")
                  ^double tab-width (data/string-width (.glyph layout) tab-string)
                  runs (sequence (comp (partition-by #(Character/isWhitespace ^char %))
                                       (map string/join))
                                 line)
                  first-run (first runs)
                  first-run-blank? (string/blank? first-run)]
              (loop [run-offset (+ line-x (if first-run-blank? 0.0 ^double (data/string-width (.glyph layout) first-run)))
                     runs (if first-run-blank? runs (next runs))]
                (when-some [^String whitespace-run (first runs)]
                  (let [whitespace-run-length (count whitespace-run)
                        next-runs (next runs)
                        ^double next-run-offset (loop [i 0
                                                       x run-offset]
                                                  (if (< i whitespace-run-length)
                                                    (case (.charAt whitespace-run i)
                                                      \space (do (.fillText gc "\u00B7" x line-y) ; "*" (MIDDLE DOT)
                                                                 (recur (inc i) (+ x space-width)))
                                                      \tab (do (.fillText gc "\u2192" x line-y) ; "->" (RIGHTWARDS ARROW)
                                                               (recur (inc i) (+ x tab-width)))
                                                      (recur (inc i) (+ x ^double (data/string-width (.glyph layout) (subs whitespace-run i (inc i))))))
                                                    x))]
                    (when-some [non-whitespace-run (first next-runs)]
                      (recur (+ next-run-offset ^double (data/string-width (.glyph layout) non-whitespace-run))
                             (next next-runs))))))))

          (recur (inc drawn-line-index)
                 (inc source-line-index)))))

    ;; Draw cursors.
    (.setFill gc foreground-color)
    (doseq [^Cursor cursor visible-cursors]
      (let [r (data/cursor-rect layout lines cursor)]
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))))

    ;; Draw scroll bar.
    (when-some [^Rect r (some-> (.scroll-tab-y layout) (data/expand-rect -3.0 -3.0))]
      (.setFill gc scroll-tab-color)
      (.fillRoundRect gc (.x r) (.y r) (.w r) (.h r) (.w r) (.w r)))

    ;; Draw gutter background and shadow when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-background-color)
      (.fillRect gc 0 0 (.x ^Rect (.canvas layout)) (.h ^Rect (.canvas layout)))
      (.setFill gc gutter-shadow-gradient)
      (.fillRect gc (.x ^Rect (.canvas layout)) 0 8 (.. gc getCanvas getHeight)))

    ;; Highlight lines with cursors in gutter.
    (.setFill gc gutter-cursor-line-background-color)
    (let [highlight-width (- (.x ^Rect (.canvas layout)) (/ line-height 2.0))
          highlight-height (dec line-height)]
      (doseq [^Cursor cursor visible-cursors]
        (let [y (+ (data/row->y layout (.row cursor)) 0.5)]
          (.fillRect gc 0 y highlight-width highlight-height))))

    ;; Draw line numbers and markers in gutter.
    (.setTextAlign gc TextAlignment/RIGHT)
    (let [^Rect line-numbers-rect (.line-numbers layout)
          indicator-diameter (- line-height 6.0)]
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
            (.setFill gc gutter-foreground-color)
            (.fillText gc (str (inc source-line-index))
                       (+ (.x line-numbers-rect) (.w line-numbers-rect))
                       (+ ascent y))
            (recur (inc drawn-line-index)
                   (inc source-line-index))))))))

;; -----------------------------------------------------------------------------

(g/defnk produce-grammar [node-id+resource]
  (some-> node-id+resource second resource/resource-type :view-opts :grammar))

(g/defnk produce-indent-level-pattern [tab-spaces]
  (data/indent-level-pattern tab-spaces))

(g/defnk produce-font [font-name font-size]
  (Font. font-name font-size))

(g/defnk produce-glyph-metrics [font tab-spaces]
  (let [font-loader (.getFontLoader (Toolkit/getToolkit))
        font-metrics (.getFontMetrics font-loader font)
        line-height (Math/ceil (inc (.getLineHeight font-metrics)))
        ascent (Math/floor (inc (.getAscent font-metrics)))
        tab-width (.computeStringWidth font-loader (string/join (repeat tab-spaces \space)) font)]
    (->GlyphMetrics font-loader font line-height ascent tab-width)))

(g/defnk produce-layout [canvas-width canvas-height scroll-x scroll-y lines glyph-metrics]
  (data/layout-info canvas-width canvas-height scroll-x scroll-y (count lines) glyph-metrics))

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

(g/defnk produce-tab-trigger-scope-regions [regions]
  (filterv #(= :tab-trigger-scope (:type %)) regions))

(g/defnk produce-tab-trigger-word-regions-by-scope-id [regions]
  (group-by :scope-id (filter #(= :tab-trigger-word (:type %)) regions)))

(g/defnk produce-visible-cursors [visible-cursor-ranges]
  (mapv data/CursorRange->Cursor visible-cursor-ranges))

(g/defnk produce-visible-cursor-ranges [lines cursor-ranges layout]
  (data/visible-cursor-ranges lines layout cursor-ranges))

(g/defnk produce-visible-regions-by-type [lines regions layout]
  (group-by :type (data/visible-cursor-ranges lines layout regions)))

(g/defnk produce-cursor-range-draw-infos [lines cursor-ranges layout visible-cursors visible-cursor-ranges visible-regions-by-type highlighted-find-term find-case-sensitive? find-whole-word?]
  (let [active-tab-trigger-scope-ids (into #{}
                                           (keep (fn [tab-trigger-scope-region]
                                                   (when (some #(data/cursor-range-contains? tab-trigger-scope-region (data/CursorRange->Cursor %))
                                                               cursor-ranges)
                                                     (:id tab-trigger-scope-region))))
                                           (visible-regions-by-type :tab-trigger-scope))]
    (vec
      (concat
        (map (partial cursor-range-draw-info :range selection-background-color background-color)
             visible-cursor-ranges)
        (map (partial cursor-range-draw-info :range nil gutter-breakpoint-color)
             (visible-regions-by-type :breakpoint))
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

(g/defnk produce-repaint [^Canvas canvas visible? font layout lines syntax-info tab-spaces cursor-range-draw-infos breakpoint-rows visible-cursors visible-whitespace?]
  (when visible?
    (draw! (.getGraphicsContext2D canvas) font layout lines syntax-info tab-spaces cursor-range-draw-infos breakpoint-rows visible-cursors visible-whitespace?))
  nil)

(g/defnode CodeEditorView
  (inherits view/WorkbenchView)

  (property undo-grouping-info UndoGroupingInfo (dynamic visible (g/constantly false)))
  (property canvas Canvas (dynamic visible (g/constantly false)))
  (property canvas-width g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property canvas-height g/Num (default 0.0) (dynamic visible (g/constantly false))
            (set (fn [basis self _old-value _new-value]
                   (let [opts {:basis basis :no-cache true}
                         lines (g/node-value self :lines opts)
                         layout (g/node-value self :layout opts)
                         scroll-y (g/node-value self :scroll-y opts)
                         new-scroll-y (data/limit-scroll-y layout lines scroll-y)]
                     (when (not= scroll-y new-scroll-y)
                       (g/set-property self :scroll-y new-scroll-y))))))
  (property visible? g/Bool (default false) (dynamic visible (g/constantly false)))
  (property scroll-x g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-y g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property suggested-completions g/Any (dynamic visible (g/constantly false)))
  (property suggestions-list-view ListView (dynamic visible (g/constantly false)))
  (property suggestions-popup PopupControl (dynamic visible (g/constantly false)))
  (property gesture-start GestureInfo (dynamic visible (g/constantly false)))
  (property highlighted-find-term g/Str (default ""))
  (property find-case-sensitive? g/Bool (dynamic visible (g/constantly false)))
  (property find-whole-word? g/Bool (dynamic visible (g/constantly false)))

  (property font-name g/Str (default "Dejavu Sans Mono"))
  (property font-size g/Num (default 12.0))
  (property indent-string g/Str (default "\t"))
  (property tab-spaces g/Num (default 4))
  (property visible-whitespace? g/Bool (default true))

  (input breakpoint-rows r/BreakpointRows)
  (input completions g/Any)
  (input cursor-ranges r/CursorRanges)
  (input invalidated-rows r/InvalidatedRows)
  (input lines r/Lines)
  (input regions r/Regions)

  (output grammar g/Any produce-grammar)
  (output indent-level-pattern Pattern :cached produce-indent-level-pattern)
  (output font Font :cached produce-font)
  (output glyph-metrics data/GlyphMetrics :cached produce-glyph-metrics)
  (output layout LayoutInfo :cached produce-layout)
  (output invalidated-syntax-info SideEffect :cached produce-invalidated-syntax-info)
  (output syntax-info SyntaxInfo :cached produce-syntax-info)
  (output tab-trigger-scope-regions r/Regions :cached produce-tab-trigger-scope-regions)
  (output tab-trigger-word-regions-by-scope-id r/RegionGrouping :cached produce-tab-trigger-word-regions-by-scope-id)
  (output visible-cursors r/Cursors :cached produce-visible-cursors)
  (output visible-cursor-ranges r/CursorRanges :cached produce-visible-cursor-ranges)
  (output visible-regions-by-type r/RegionGrouping :cached produce-visible-regions-by-type)
  (output cursor-range-draw-infos CursorRangeDrawInfos :cached produce-cursor-range-draw-infos)
  (output repaint SideEffect :cached produce-repaint))

(defn- mouse-button [^MouseEvent event]
  (condp = (.getButton event)
    MouseButton/NONE nil
    MouseButton/PRIMARY :primary
    MouseButton/SECONDARY :secondary
    MouseButton/MIDDLE :middle))

(defn- operation-sequence-tx-data! [view-node undo-grouping]
  (when (some? undo-grouping)
    (assert (contains? undo-groupings undo-grouping))
    (let [[prev-undo-grouping prev-opseq] (g/node-value view-node :undo-grouping-info)]
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

;; -----------------------------------------------------------------------------

;; The functions that perform actions in the core.data module return maps of
;; properties that were modified by the operation. Some of these properties need
;; to be stored at various locations in order to be undoable, some are transient,
;; and so on. These two functions should be used to read and write these managed
;; properties at all times. Basically, get-property needs to be able to get any
;; property that is supplied to set-properties! in values-by-prop-kw.

(defn- get-property
  "Gets the value of a property that is managed by the functions in the code.data module."
  [view-node prop-kw]
  (case prop-kw
    :invalidated-row
    (invalidated-row (ui/user-data (g/node-value view-node :canvas) ::invalidated-rows)
                     (g/node-value (g/node-value view-node :resource-node) :invalidated-rows))

    (g/node-value view-node prop-kw)))

(defn- set-properties!
  "Sets values of properties that are managed by the functions in the code.data module.
  Returns true if any property changed, false otherwise."
  [view-node undo-grouping values-by-prop-kw]
  (if (empty? values-by-prop-kw)
    false
    (let [resource-node (g/node-value view-node :resource-node)]
      (g/transact
        (into (operation-sequence-tx-data! view-node undo-grouping)
              (mapcat (fn [[prop-kw value]]
                        (case prop-kw
                          (:cursor-ranges :lines :regions)
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
    (Point2D. (+ (.x r) (.scroll-x layout))
              (- (+ (.y r) (.h r) (.scroll-y layout)) (.y canvas)))))

(defn- pref-suggestions-popup-position
  ^Point2D [^Canvas canvas width height ^Point2D cursor-bottom]
  (Utils/pointRelativeTo canvas width height HPos/CENTER VPos/CENTER (.getX cursor-bottom) (.getY cursor-bottom) true))

(defn- show-suggestions! [view-node]
  ;; Snapshot completions when the popup is first opened to prevent
  ;; typed words from showing up in the completions list.
  (when (nil? (get-property view-node :suggested-completions))
    (set-properties! view-node :typing {:suggested-completions (get-property view-node :completions)}))

  (let [lines (get-property view-node :lines)
        cursor-ranges (get-property view-node :cursor-ranges)
        [replaced-cursor-range context query] (suggestion-info lines cursor-ranges)
        query-text (if (empty? context) query (str context \. query))
        ^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    (if-not (some #(Character/isLetter ^char %) query-text)
      (when (.isShowing suggestions-popup)
        (.hide suggestions-popup))
      (let [completions (get-property view-node :suggested-completions)
            context-completions (get completions context)
            filtered-completions (popup/fuzzy-option-filter-fn :name query-text context-completions)]
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
            nil))))))

(defn- hide-suggestions! [view-node]
  (set-properties! view-node :typing {:suggested-completions nil})
  (let [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    (when (.isShowing suggestions-popup)
      (.hide suggestions-popup))))

(defn- suggestions-shown? [view-node]
  (let [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)]
    (.isShowing suggestions-popup)))

(defn- selected-suggestion [view-node]
  (let [^PopupControl suggestions-popup (g/node-value view-node :suggestions-popup)
        ^ListView suggestions-list-view (g/node-value view-node :suggestions-list-view)]
    (when (.isShowing suggestions-popup)
      (first (ui/selection suggestions-list-view)))))

(defn- find-tab-trigger-scope-region-at-cursor
  ^CursorRange [tab-trigger-scope-regions ^Cursor cursor]
  (some (fn [scope-region]
          (when (data/cursor-range-contains? scope-region cursor)
            scope-region))
        tab-trigger-scope-regions))

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
                                :prev #(not (pos? (compare from-cursor (cursor-range->cursor %))))
                                :next #(not (neg? (compare from-cursor (cursor-range->cursor %)))))
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

(defn- find-tab-trigger-word-ranges [lines suggestion ^CursorRange scope-region]
  (when-some [tab-trigger-words (some-> suggestion :tab-triggers :select not-empty)]
    (let [search-range (case (:type suggestion)
                         :function (if-some [opening-paren (data/find-next-occurrence lines ["("] (data/cursor-range-start scope-region) true false)]
                                     (data/->CursorRange (data/cursor-range-end opening-paren)
                                                         (data/cursor-range-end scope-region))
                                     scope-region)
                         scope-region)]
      (data/find-sequential-words-in-scope lines tab-trigger-words search-range))))

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
          props (data/replace-typed-chars indent-level-pattern indent-string grammar lines cursor-ranges regions replaced-char-count replacement-lines)]
      (when (some? props)
        (hide-suggestions! view-node)
        (let [cursor-ranges (:cursor-ranges props)
              tab-trigger-scope-regions (map #(assoc % :type :tab-trigger-scope :id (gensym "tab-scope")) cursor-ranges)
              tab-trigger-word-cursor-range-colls (map (partial find-tab-trigger-word-ranges (:lines props) selected-suggestion) tab-trigger-scope-regions)
              tab-trigger-word-regions (mapcat (fn [scope-region word-cursor-ranges]
                                                 (map #(assoc % :type :tab-trigger-word
                                                                :scope-id (:id scope-region))
                                                      word-cursor-ranges))
                                               tab-trigger-scope-regions
                                               tab-trigger-word-cursor-range-colls)
              new-cursor-ranges (if (seq tab-trigger-word-regions)
                                  (mapv (comp data/sanitize-cursor-range first) tab-trigger-word-cursor-range-colls)
                                  (mapv data/cursor-range-end-range cursor-ranges))]
          (set-properties! view-node :typing
                           (cond-> (assoc props :cursor-ranges new-cursor-ranges)

                                   (seq tab-trigger-word-regions)
                                   (assoc :regions (vec (sort (concat (remove tab-trigger-related-region? regions)
                                                                      tab-trigger-scope-regions
                                                                      tab-trigger-word-regions))))

                                   true
                                   (data/frame-cursor layout))))))))

;; -----------------------------------------------------------------------------

(defn- move! [view-node move-type cursor-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/move (get-property view-node :lines)
                              (get-property view-node :cursor-ranges)
                              (get-property view-node :layout)
                              move-type
                              cursor-type)))

(defn- page-up! [view-node move-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-up (get-property view-node :lines)
                                 (get-property view-node :cursor-ranges)
                                 (get-property view-node :scroll-y)
                                 (get-property view-node :layout)
                                 move-type)))

(defn- page-down! [view-node move-type]
  (hide-suggestions! view-node)
  (set-properties! view-node move-type
                   (data/page-down (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :scroll-y)
                                   (get-property view-node :layout)
                                   move-type)))

(defn- delete! [view-node delete-type]
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

(defn- select-next-occurrence! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node :selection
                   (data/select-next-occurrence (get-property view-node :lines)
                                                (get-property view-node :cursor-ranges)
                                                (get-property view-node :layout))))

(defn- can-indent? [view-node]
  (data/can-indent? (get-property view-node :lines)
                    (get-property view-node :cursor-ranges)))

(defn- indent! [view-node]
  (hide-suggestions! view-node)
  (set-properties! view-node nil
                   (data/indent (get-property view-node :indent-level-pattern)
                                (get-property view-node :indent-string)
                                (get-property view-node :grammar)
                                (get-property view-node :lines)
                                (get-property view-node :cursor-ranges)
                                (get-property view-node :regions)
                                (get-property view-node :indent-string)
                                (get-property view-node :layout))))

(defn- can-deindent? [view-node]
  (data/can-deindent? (get-property view-node :lines)
                      (get-property view-node :cursor-ranges)))

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

    (can-indent? view-node)
    (indent! view-node)

    (in-tab-trigger? view-node)
    (next-tab-trigger! view-node)))

(defn- shift-tab! [view-node]
  (cond
    (can-deindent? view-node)
    (deindent! view-node)

    (in-tab-trigger? view-node)
    (prev-tab-trigger! view-node)))

(handler/defhandler :tab :new-code-view
  (run [view-node] (tab! view-node)))

(handler/defhandler :backwards-tab-trigger :new-code-view
  (run [view-node] (shift-tab! view-node)))

(handler/defhandler :proposals :new-code-view
  (run [view-node] (show-suggestions! view-node)))

;; -----------------------------------------------------------------------------

(defn- handle-key-pressed! [view-node ^KeyEvent event]
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

(defn- handle-key-typed! [view-node ^KeyEvent event]
  (.consume event)
  (let [typed (.getCharacter event)
        undo-grouping (if (= "\r" typed) :newline :typing)
        selected-suggestion (selected-suggestion view-node)
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

                                            :else
                                            [true true])]
    (when insert-typed?
      (when (set-properties! view-node undo-grouping
                             (data/key-typed (get-property view-node :indent-level-pattern)
                                             (get-property view-node :indent-string)
                                             (get-property view-node :grammar)
                                             (get-property view-node :lines)
                                             (get-property view-node :cursor-ranges)
                                             (get-property view-node :regions)
                                             (get-property view-node :layout)
                                             typed
                                             (.isMetaDown event)
                                             (.isControlDown event)))
        (when show-suggestions?
          (show-suggestions! view-node))))))

(defn- refresh-mouse-cursor! [view-node ^MouseEvent event]
  (let [gesture-type (:type (get-property view-node :gesture-start))
        ^LayoutInfo layout (get-property view-node :layout)
        ^Node node (.getTarget event)
        x (.getX event)
        y (.getY event)
        cursor (cond
                 (= :scroll-tab-y-drag gesture-type)
                 javafx.scene.Cursor/DEFAULT

                 (some-> (.scroll-tab-y layout) (data/rect-contains? x y))
                 javafx.scene.Cursor/DEFAULT

                 (data/rect-contains? (.canvas layout) x y)
                 ui/text-cursor-white

                 :else
                 javafx.scene.Cursor/DEFAULT)]
    ;; The cursor refresh appears buggy at the moment.
    ;; Calling setCursor with DISAPPEAR before setting the cursor forces it to refresh.
    (.setCursor node javafx.scene.Cursor/DISAPPEAR)
    (.setCursor node cursor)))

(defn- handle-mouse-pressed! [view-node ^MouseEvent event]
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

(defn- handle-mouse-moved! [view-node ^MouseDragEvent event]
  (.consume event)
  (refresh-mouse-cursor! view-node event)
  (set-properties! view-node :selection
                   (data/mouse-moved (get-property view-node :lines)
                                     (get-property view-node :cursor-ranges)
                                     (get-property view-node :layout)
                                     (get-property view-node :gesture-start)
                                     (.getX event)
                                     (.getY event))))

(defn- handle-mouse-released! [view-node ^MouseEvent event]
  (.consume event)
  (refresh-mouse-cursor! view-node event)
  (set-properties! view-node :selection
                   (data/mouse-released)))

(defn- handle-scroll! [view-node ^ScrollEvent event]
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

(handler/defhandler :select-up :new-code-view
  (run [view-node] (move! view-node :selection :up)))

(handler/defhandler :select-down :new-code-view
  (run [view-node] (move! view-node :selection :down)))

(handler/defhandler :select-left :new-code-view
  (run [view-node] (move! view-node :selection :left)))

(handler/defhandler :select-right :new-code-view
  (run [view-node] (move! view-node :selection :right)))

(handler/defhandler :prev-word :new-code-view
  (run [view-node] (move! view-node :navigation :prev-word)))

(handler/defhandler :select-prev-word :new-code-view
  (run [view-node] (move! view-node :selection :prev-word)))

(handler/defhandler :next-word :new-code-view
  (run [view-node] (move! view-node :navigation :next-word)))

(handler/defhandler :select-next-word :new-code-view
  (run [view-node] (move! view-node :selection :next-word)))

(handler/defhandler :beginning-of-line :new-code-view
  (run [view-node] (move! view-node :navigation :line-start)))

(handler/defhandler :select-beginning-of-line :new-code-view
  (run [view-node] (move! view-node :selection :line-start)))

(handler/defhandler :beginning-of-line-text :new-code-view
  (run [view-node] (move! view-node :navigation :home)))

(handler/defhandler :select-beginning-of-line-text :new-code-view
  (run [view-node] (move! view-node :selection :home)))

(handler/defhandler :end-of-line :new-code-view
  (run [view-node] (move! view-node :navigation :end)))

(handler/defhandler :select-end-of-line :new-code-view
  (run [view-node] (move! view-node :selection :end)))

(handler/defhandler :page-up :new-code-view
  (run [view-node] (page-up! view-node :navigation)))

(handler/defhandler :select-page-up :new-code-view
  (run [view-node] (page-up! view-node :selection)))

(handler/defhandler :page-down :new-code-view
  (run [view-node] (page-down! view-node :navigation)))

(handler/defhandler :select-page-down :new-code-view
  (run [view-node] (page-down! view-node :selection)))

(handler/defhandler :beginning-of-file :new-code-view
  (run [view-node] (move! view-node :navigation :file-start)))

(handler/defhandler :select-beginning-of-file :new-code-view
  (run [view-node] (move! view-node :selection :file-start)))

(handler/defhandler :end-of-file :new-code-view
  (run [view-node] (move! view-node :navigation :file-end)))

(handler/defhandler :select-end-of-file :new-code-view
  (run [view-node] (move! view-node :selection :file-end)))

(handler/defhandler :cut :new-code-view
  (enabled? [view-node] (not-every? data/cursor-range-empty? (get-property view-node :cursor-ranges)))
  (run [view-node clipboard]
       (hide-suggestions! view-node)
       (set-properties! view-node nil
                        (data/cut! (get-property view-node :lines)
                                   (get-property view-node :cursor-ranges)
                                   (get-property view-node :regions)
                                   (get-property view-node :layout)
                                   clipboard))))

(handler/defhandler :copy :new-code-view
  (enabled? [view-node] (not-every? data/cursor-range-empty? (get-property view-node :cursor-ranges)))
  (run [view-node clipboard]
       (hide-suggestions! view-node)
       (set-properties! view-node nil
                        (data/copy! (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges)
                                    clipboard))))

(handler/defhandler :paste :new-code-view
  (enabled? [view-node clipboard] (data/can-paste? (get-property view-node :cursor-ranges) clipboard))
  (run [view-node clipboard]
       (hide-suggestions! view-node)
       (set-properties! view-node nil
                        (data/paste (get-property view-node :indent-level-pattern)
                                    (get-property view-node :indent-string)
                                    (get-property view-node :grammar)
                                    (get-property view-node :lines)
                                    (get-property view-node :cursor-ranges)
                                    (get-property view-node :regions)
                                    (get-property view-node :layout)
                                    clipboard))))

(handler/defhandler :delete :new-code-view
  (run [view-node] (delete! view-node :delete-after)))

(handler/defhandler :delete-backward :new-code-view
  (run [view-node] (delete! view-node :delete-before)))

(handler/defhandler :select-next-occurrence :new-code-view
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :select-next-occurrence :new-console
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :split-selection-into-lines :new-code-view
  (run [view-node]
       (hide-suggestions! view-node)
       (set-properties! view-node :selection
                        (data/split-selection-into-lines (get-property view-node :lines)
                                                         (get-property view-node :cursor-ranges)))))

(handler/defhandler :toggle-breakpoint :new-code-view
  (active? []
           ;; TODO: Disabled until we have a debugger.
           false)
  (run [view-node]
       (set-properties! view-node nil
                        (data/toggle-breakpoint (get-property view-node :lines)
                                                (get-property view-node :regions)
                                                (data/cursor-ranges->rows (get-property view-node :cursor-ranges))))))


;; -----------------------------------------------------------------------------
;; Find & Replace
;; -----------------------------------------------------------------------------

;; these are global for now, reasonable to find/replace same thing in several files
(defonce ^SimpleStringProperty find-ui-type-property (doto (SimpleStringProperty.) (.setValue (name :hidden))))
(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleStringProperty find-replacement-property (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleBooleanProperty find-whole-word-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-case-sensitive-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-wrap-property (doto (SimpleBooleanProperty.) (.setValue false)))

(defonce ^SimpleStringProperty highlighted-find-term-property
  (-> (Bindings/when (Bindings/notEqual (name :hidden) find-ui-type-property))
      (.then find-term-property)
      (.otherwise "")))

(defn- setup-find-bar! [^GridPane find-bar view-node]
  (.bind (.visibleProperty find-bar) (Bindings/equal (name :find) find-ui-type-property))
  (.bind (.managedProperty find-bar) (.visibleProperty find-bar))
  (doto find-bar
    (ui/context! :new-find-bar {:find-bar find-bar :view-node view-node} nil)
    (.setMaxWidth Double/MAX_VALUE)
    (GridPane/setConstraints 0 1))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^Button next ^Button prev]
    (.bindBidirectional (.textProperty term) find-term-property)
    (.bindBidirectional (.selectedProperty whole-word) find-whole-word-property)
    (.bindBidirectional (.selectedProperty case-sensitive) find-case-sensitive-property)
    (.bindBidirectional (.selectedProperty wrap) find-wrap-property)
    (ui/bind-keys! find-bar {KeyCode/ENTER :find-next})
    (ui/bind-action! next :find-next)
    (ui/bind-action! prev :find-prev))
  find-bar)

(defn- setup-replace-bar! [^GridPane replace-bar view-node]
  (.bind (.visibleProperty replace-bar) (Bindings/equal (name :replace) find-ui-type-property))
  (.bind (.managedProperty replace-bar) (.visibleProperty replace-bar))
  (doto replace-bar
    (ui/context! :new-replace-bar {:replace-bar replace-bar :view-node view-node} nil)
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

(defn- focus-code-editor! [view-node]
  (let [^Canvas canvas (g/node-value view-node :canvas)]
    (.requestFocus canvas)))

(defn- focus-term-field! [^Parent bar]
  (ui/with-controls bar [^TextField term]
    (.requestFocus term)
    (.selectAll term)))

(defn- find-ui-visible? []
  (not= (name :hidden) (.getValue find-ui-type-property)))

(defn- set-find-ui-type! [ui-type]
  (case ui-type (:hidden :find :replace) (.setValue find-ui-type-property (name ui-type))))

(defn- set-find-term! [^String term-text]
  (.setValue find-term-property (or term-text "")))

(defn- non-empty-single-selection-text [view-node]
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
                                     (split-lines (.getValue find-term-property))
                                     (split-lines (.getValue find-replacement-property))
                                     (.getValue find-case-sensitive-property)
                                     (.getValue find-whole-word-property))))

(handler/defhandler :find-text :new-code-view
  (run [find-bar grid view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-find-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :replace-text :new-code-view
  (run [grid replace-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-find-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :find-text :new-console ;; In practice, from find / replace bars.
  (run [find-bar grid]
       (set-find-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :replace-text :new-console
  (run [grid replace-bar]
       (set-find-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :escape :new-console
  (run [find-bar grid replace-bar view-node]
       (cond
         (in-tab-trigger? view-node)
         (exit-tab-trigger! view-node)

         (suggestions-shown? view-node)
         (hide-suggestions! view-node)

         (find-ui-visible?)
         (do (set-find-ui-type! :hidden)
             (focus-code-editor! view-node))

         :else
         (set-properties! view-node :selection
                          (data/escape (get-property view-node :cursor-ranges))))))

(handler/defhandler :find-next :new-find-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :new-replace-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :new-code-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-prev :new-find-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :new-replace-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :new-code-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :replace-next :new-replace-bar
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :replace-next :new-code-view
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :replace-all :new-replace-bar
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
                 {:command :goto-line                  :label "Go to Line..."}
                 {:label :separator}
                 {:command :toggle-comment             :label "Toggle Comment"}
                 {:command :indent                     :label "Indent"}
                 {:label :separator}
                 {:command :select-next-occurrence     :label "Select Next Occurrence"}
                 {:command :split-selection-into-lines :label "Split Selection Into Lines"}
                 {:label :separator}
                 {:command :toggle-breakpoint          :label "Toggle Breakpoint"}])

;; -----------------------------------------------------------------------------

(defn- setup-view! [resource-node view-node]
  (g/transact
    (concat
      (g/connect resource-node :breakpoint-rows view-node :breakpoint-rows)
      (g/connect resource-node :completions view-node :completions)
      (g/connect resource-node :cursor-ranges view-node :cursor-ranges)
      (g/connect resource-node :invalidated-rows view-node :invalidated-rows)
      (g/connect resource-node :lines view-node :lines)
      (g/connect resource-node :regions view-node :regions)))
  view-node)

(defn- repaint-view! [view-node]
  (g/node-value view-node :repaint))

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

(defn- make-view! [graph parent resource-node opts]
  (let [grid (GridPane.)
        canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        suggestions-list-view (make-suggestions-list-view canvas)
        suggestions-popup (popup/make-choices-popup canvas-pane suggestions-list-view)
        undo-grouping-info (pair :navigation (gensym))
        view-node (setup-view! resource-node
                               (g/make-node! graph CodeEditorView
                                             :canvas canvas
                                             :suggestions-list-view suggestions-list-view
                                             :suggestions-popup suggestions-popup
                                             :undo-grouping-info undo-grouping-info
                                             :highlighted-find-term (.getValue highlighted-find-term-property)))
        find-bar (setup-find-bar! (ui/load-fxml "find.fxml") view-node)
        replace-bar (setup-replace-bar! (ui/load-fxml "replace.fxml") view-node)
        repainter (ui/->timer 60 "repaint-code-editor-view" (fn [_ _] (repaint-view! view-node)))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :find-bar find-bar
                     :grid grid
                     :replace-bar replace-bar
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Highlight occurrences of search term while find bar is open.
    (ui/observe highlighted-find-term-property (fn [_ _ find-term] (g/set-property! view-node :highlighted-find-term find-term)))
    (ui/observe find-case-sensitive-property (fn [_ _ find-case-sensitive?] (g/set-property! view-node :find-case-sensitive? find-case-sensitive?)))
    (ui/observe find-whole-word-property (fn [_ _ find-whole-word?] (g/set-property! view-node :find-whole-word? find-whole-word?)))

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
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (handle-scroll! view-node event))))

    (ui/context! grid :new-console context-env nil)

    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints canvas-pane 0 0)
    (GridPane/setVgrow canvas-pane Priority/ALWAYS)

    (ui/children! grid [canvas-pane find-bar replace-bar])
    (ui/children! parent [grid])
    (ui/fill-control grid)
    (ui/context! canvas :new-code-view context-env nil)

    (ui/observe (.selectedProperty ^Tab (:tab opts))
                (fn [_ _ became-selected?]
                  ;; We can skip expensive repaints while we're not visible.
                  (g/set-property! view-node :visible? became-selected?)

                  ;; Steal input focus when our tab becomes selected.
                  (when became-selected?
                    (ui/run-later (.requestFocus canvas)))))

    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! (:tab opts) repainter)
    view-node))

(defn- focus-view! [view-node {:keys [line]}]
  ;; TODO: Scroll to line.
  (.requestFocus ^Node (g/node-value view-node :canvas)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :new-code
                                :label "Code"
                                :make-view-fn (fn [graph parent resource-node opts] (make-view! graph parent resource-node opts))
                                :focus-fn (fn [view-node opts] (focus-view! view-node opts))))
