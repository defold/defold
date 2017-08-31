(ns editor.code.view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [editor.handler :as handler]
            [internal.util :as util]
            [schema.core :as s])
  (:import (clojure.lang IPersistentVector MapEntry)
           (com.sun.javafx.tk FontLoader Toolkit)
           (editor.code.data Cursor CursorRange LayoutInfo Rect)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleBooleanProperty SimpleStringProperty)
           (javafx.scene Node Parent)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.control Button CheckBox Tab TextField)
           (javafx.scene.input Clipboard DataFormat KeyCode KeyEvent MouseButton MouseDragEvent MouseEvent ScrollEvent)
           (javafx.scene.layout ColumnConstraints GridPane Pane Priority)
           (javafx.scene.paint Color LinearGradient)
           (javafx.scene.text Font FontSmoothingType TextAlignment)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private undo-groupings #{:navigation :newline :selection :typing})
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
(def ^:private ^Color find-term-occurrence-color (Color/valueOf "#60C1FF"))
(def ^:private ^Color gutter-foreground-color (Color/valueOf "#A2B0BE"))
(def ^:private ^Color gutter-background-color background-color)
(def ^:private ^Color gutter-cursor-line-background-color (Color/valueOf "#393C41"))
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

(defn- draw-cursor-ranges! [^GraphicsContext gc fill-color stroke-color rect-sets]
  (when-not (empty? rect-sets)
    (when (some? fill-color)
      (.setFill gc fill-color)
      (doseq [rects rect-sets
              ^Rect r rects]
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))))
    (when (some? stroke-color)
      (.setStroke gc stroke-color)
      (.setLineWidth gc 1.0)
      (doseq [rects rect-sets
              polyline (cursor-range-outline rects)]
        (let [[xs ys] polyline]
          (.strokePolyline gc (double-array xs) (double-array ys) (count xs)))))))

(defn- visible-occurrences [^LayoutInfo layout lines needle-lines]
  (when (not-every? empty? needle-lines)
    (loop [from-cursor (data/->Cursor (max 0 (- (.dropped-line-count layout) (count needle-lines))) 0)
           rect-sets (transient [])]
      (let [matching-cursor-range (data/find-next-occurrence lines needle-lines from-cursor)]
        (if (and (some? matching-cursor-range)
                 (< (.row (data/cursor-range-start matching-cursor-range))
                    (+ (.dropped-line-count layout) (.drawn-line-count layout))))
          (recur (data/cursor-range-end matching-cursor-range)
                 (conj! rect-sets (data/cursor-range-rects layout lines matching-cursor-range)))
          (persistent! rect-sets))))))

(defn- highlight-selected-word-occurrences! [^GraphicsContext gc ^LayoutInfo layout lines visible-cursor-ranges selected-word-cursor-range]
  (when (some? selected-word-cursor-range)
    (let [dropped-line-count (.dropped-line-count layout)
          drawn-line-count (.drawn-line-count layout)
          needle-lines (data/subsequence->lines (data/cursor-range-subsequence lines selected-word-cursor-range))]
      (.setStroke gc selection-occurrence-outline-color)
      (.setLineWidth gc 1.0)
      (loop [from-cursor (data/->Cursor (max 0 (- dropped-line-count (count needle-lines))) 0)]
        (when-some [matching-cursor-range (data/find-next-occurrence lines needle-lines from-cursor)]
          (when (< (.row (data/cursor-range-start matching-cursor-range))
                   (+ dropped-line-count drawn-line-count))
            (when (and (data/word-cursor-range? lines matching-cursor-range)
                       (not-any? (partial data/cursor-range-equals? matching-cursor-range) visible-cursor-ranges))
              (let [^Rect word-rect (first (data/cursor-range-rects layout lines matching-cursor-range))
                    ^Rect r (data/expand-rect word-rect 1.5 -0.5)]
                (.strokeRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0)))
            (recur (data/cursor-range-end matching-cursor-range))))))))

(defn- highlight-find-term-occurrences! [^GraphicsContext gc ^LayoutInfo layout lines find-term-lines]
  (draw-cursor-ranges! gc nil find-term-occurrence-color (visible-occurrences layout lines find-term-lines)))

(defn- draw! [^GraphicsContext gc ^Font font ^LayoutInfo layout lines cursor-ranges syntax-info tab-spaces visible-whitespace? highlighted-find-term]
  (let [source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double ascent (data/ascent (.glyph layout))
        ^double line-height (data/line-height (.glyph layout))
        tab-string (string/join (repeat tab-spaces \space))
        visible-cursor-ranges (into []
                                    (comp (drop-while (partial data/cursor-range-ends-before-row? dropped-line-count))
                                          (take-while (partial data/cursor-range-starts-before-row? (+ dropped-line-count drawn-line-count)))
                                          (map (partial data/adjust-cursor-range lines)))
                                    cursor-ranges)]
    (.setFill gc background-color)
    (.fillRect gc 0 0 (.. gc getCanvas getWidth) (.. gc getCanvas getHeight))
    (.setFont gc font)
    (.setFontSmoothingType gc FontSmoothingType/LCD)

    ;; Draw selection.
    (draw-cursor-ranges! gc selection-background-color background-color
                         (map (partial data/cursor-range-rects layout lines)
                              visible-cursor-ranges))

    ;; Highlight occurrences.
    (if (empty? highlighted-find-term)
      (highlight-selected-word-occurrences! gc layout lines visible-cursor-ranges (data/selected-word-cursor-range lines cursor-ranges))
      (highlight-find-term-occurrences! gc layout lines (string/split-lines highlighted-find-term)))

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
    (doseq [^CursorRange cursor-range visible-cursor-ranges]
      (let [r (data/cursor-rect layout lines (.to cursor-range))]
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))))

    ;; Draw scroll bar.
    (when-some [rect ^Rect (.scroll-tab-y layout)]
      (.setFill gc scroll-tab-color)
      (.fillRoundRect gc (.x rect) (.y rect) (.w rect) (.h rect) (.w rect) (.w rect)))

    ;; Draw gutter background when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-background-color)
      (.fillRect gc 0 0 (.x ^Rect (.canvas layout)) (.h ^Rect (.canvas layout))))

    ;; Highlight lines with cursors in gutter.
    (.setFill gc gutter-cursor-line-background-color)
    (let [highlight-width (- (.x ^Rect (.canvas layout)) (Math/ceil (data/string-width (.glyph layout) " ")))
          highlight-height (dec line-height)]
      (doseq [row (sequence (comp (map data/CursorRange->Cursor)
                                  (map (fn [^Cursor cursor] (.row cursor)))
                                  (drop-while (partial > dropped-line-count))
                                  (take-while (partial > (+ dropped-line-count drawn-line-count))))
                            visible-cursor-ranges)]
        (let [y (+ (data/row->y layout row) 0.5)]
          (.fillRect gc 0 y highlight-width highlight-height))))

    ;; Draw gutter shadow when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-shadow-gradient)
      (.fillRect gc (.x ^Rect (.canvas layout)) 0 8 (.. gc getCanvas getHeight)))

    ;; Draw line numbers.
    (.setFill gc gutter-foreground-color)
    (.setTextAlign gc TextAlignment/RIGHT)
    (loop [drawn-line-index 0
           source-line-index dropped-line-count]
      (when (and (< drawn-line-index drawn-line-count)
                 (< source-line-index source-line-count))
        (.fillText gc (str (inc source-line-index))
                   (- (.x ^Rect (.canvas layout)) line-height)
                   (+ ascent
                      (.scroll-y-remainder layout)
                      (* drawn-line-index line-height)))
        (recur (inc drawn-line-index)
               (inc source-line-index))))))

;; -----------------------------------------------------------------------------

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

(g/defnk produce-syntax-info [canvas invalidated-syntax-info layout lines resource-node]
  (assert (nil? invalidated-syntax-info))
  (if-some [grammar (some-> resource-node (g/node-value :resource) resource/resource-type :view-opts :grammar)]
    (let [invalidated-syntax-info (or (ui/user-data canvas ::syntax-info) [])
          syntax-info (data/highlight-visible-syntax lines invalidated-syntax-info layout grammar)]
      (when-not (identical? invalidated-syntax-info syntax-info)
        (ui/user-data! canvas ::syntax-info syntax-info))
      syntax-info)
    []))

(g/defnk produce-repaint [^Canvas canvas font layout lines cursor-ranges syntax-info tab-spaces visible-whitespace? highlighted-find-term]
  (draw! (.getGraphicsContext2D canvas) font layout lines cursor-ranges syntax-info tab-spaces visible-whitespace? highlighted-find-term)
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
  (property scroll-x g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-y g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property highlighted-find-term g/Str (default ""))
  (property font-name g/Str (default "Dejavu Sans Mono"))
  (property font-size g/Num (default 12.0))
  (property indent-string g/Str (default "\t"))
  (property tab-spaces g/Num (default 4))
  (property visible-whitespace? g/Bool (default true))

  (input cursor-ranges r/CursorRanges)
  (input invalidated-rows r/InvalidatedRows)
  (input lines r/Lines)

  (output font Font :cached produce-font)
  (output glyph-metrics data/GlyphMetrics :cached produce-glyph-metrics)
  (output layout LayoutInfo :cached produce-layout)
  (output invalidated-syntax-info SideEffect :cached produce-invalidated-syntax-info)
  (output syntax-info SyntaxInfo :cached produce-syntax-info)
  (output repaint SideEffect :cached produce-repaint))

(defn- pair [a b]
  (MapEntry/create a b))

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

(defn- set-properties! [view-node undo-grouping values-by-prop-kw]
  (when-not (empty? values-by-prop-kw)
    (let [resource-node (g/node-value view-node :resource-node)]
      (g/transact
        (into (operation-sequence-tx-data! view-node undo-grouping)
              (mapcat (fn [[prop-kw value]]
                        (case prop-kw
                          ;; Several rows could have been invalidated between repaints.
                          ;; We accumulate these to compare against seen invalidations.
                          :invalidated-row
                          (g/update-property resource-node :invalidated-rows conj value)

                          (:cursor-ranges :lines)
                          (g/set-property resource-node prop-kw value)

                          (g/set-property view-node prop-kw value))))
              values-by-prop-kw))
      nil)))

(defn- mouse-button [^MouseEvent event]
  (condp = (.getButton event)
    MouseButton/NONE nil
    MouseButton/PRIMARY :primary
    MouseButton/SECONDARY :secondary
    MouseButton/MIDDLE :middle))

;; -----------------------------------------------------------------------------

(defn- move! [view-node move-type cursor-type]
  (set-properties! view-node move-type
                   (data/move (g/node-value view-node :lines)
                              (g/node-value view-node :cursor-ranges)
                              (g/node-value view-node :layout)
                              move-type
                              cursor-type)))

(defn- page-up! [view-node move-type]
  (set-properties! view-node move-type
                   (data/page-up (g/node-value view-node :lines)
                                 (g/node-value view-node :cursor-ranges)
                                 (g/node-value view-node :scroll-y)
                                 (g/node-value view-node :layout)
                                 move-type)))

(defn- page-down! [view-node move-type]
  (set-properties! view-node move-type
                   (data/page-down (g/node-value view-node :lines)
                                   (g/node-value view-node :cursor-ranges)
                                   (g/node-value view-node :scroll-y)
                                   (g/node-value view-node :layout)
                                   move-type)))

(defn- delete! [view-node delete-fn]
  (let [cursor-ranges (g/node-value view-node :cursor-ranges)
        undo-grouping (when (every? data/cursor-range-empty? cursor-ranges) :typing)]
    (set-properties! view-node undo-grouping
                     (data/delete (g/node-value view-node :lines)
                                  cursor-ranges
                                  (g/node-value view-node :layout)
                                  delete-fn))))

(defn- select-next-occurrence! [view-node]
  (set-properties! view-node :selection
                   (data/select-next-occurrence (g/node-value view-node :lines)
                                                (g/node-value view-node :cursor-ranges)
                                                (g/node-value view-node :layout))))

(defn- indent! [view-node]
  (set-properties! view-node nil
                   (data/indent (g/node-value view-node :lines)
                                (g/node-value view-node :cursor-ranges)
                                (g/node-value view-node :indent-string)
                                (g/node-value view-node :layout))))

(defn- deindent! [view-node]
  (set-properties! view-node nil
                   (data/deindent (g/node-value view-node :lines)
                                  (g/node-value view-node :cursor-ranges)
                                  (g/node-value view-node :indent-string))))

(defn- single-selection! [view-node]
  (set-properties! view-node :selection
                   (data/single-selection (g/node-value view-node :cursor-ranges))))

;; -----------------------------------------------------------------------------

(defn- handle-key-pressed! [view-node ^KeyEvent event]
  (let [alt-key? (.isAltDown event)
        shift-key? (.isShiftDown event)
        shortcut-key? (.isShortcutDown event)
        ;; -----
        alt? (and alt-key? (not (or shift-key? shortcut-key?)))
        alt-shift? (and alt-key? shift-key? (not shortcut-key?))
        alt-shift-shortcut? (and shortcut-key? alt-key? shift-key?)
        alt-shortcut? (and shortcut-key? alt-key? (not shift-key?))
        bare? (not (or alt-key? shift-key? shortcut-key?))
        shift? (and shift-key? (not (or alt-key? shortcut-key?)))
        shift-shortcut? (and shortcut-key? shift-key? (not alt-key?))
        shortcut? (and shortcut-key? (not (or alt-key? shift-key?)))
        ;; -----
        diff (cond
               alt?
               (condp = (.getCode event)
                 KeyCode/LEFT  (move! view-node :navigation :prev-word)
                 KeyCode/RIGHT (move! view-node :navigation :next-word)
                 KeyCode/UP    (move! view-node :navigation :line-start)
                 KeyCode/DOWN  (move! view-node :navigation :line-end)
                 ::unhandled)

               alt-shift?
               (condp = (.getCode event)
                 KeyCode/LEFT  (move! view-node :selection :prev-word)
                 KeyCode/RIGHT (move! view-node :selection :next-word)
                 KeyCode/UP    (move! view-node :selection :line-start)
                 KeyCode/DOWN  (move! view-node :selection :line-end)
                 ::unhandled)

               alt-shift-shortcut?
               ::unhandled

               alt-shortcut?
               ::unhandled

               bare?
               (condp = (.getCode event)
                 KeyCode/PAGE_UP    (page-up! view-node :navigation)
                 KeyCode/PAGE_DOWN  (page-down! view-node :navigation)
                 KeyCode/HOME       (move! view-node :navigation :home)
                 KeyCode/END        (move! view-node :navigation :end)
                 KeyCode/LEFT       (move! view-node :navigation :left)
                 KeyCode/RIGHT      (move! view-node :navigation :right)
                 KeyCode/UP         (move! view-node :navigation :up)
                 KeyCode/DOWN       (move! view-node :navigation :down)
                 KeyCode/TAB        (indent! view-node)
                 KeyCode/BACK_SPACE (delete! view-node data/delete-character-before-cursor)
                 ::unhandled)

               shift?
               (condp = (.getCode event)
                 KeyCode/PAGE_UP   (page-up! view-node :selection)
                 KeyCode/PAGE_DOWN (page-down! view-node :selection)
                 KeyCode/HOME      (move! view-node :selection :home)
                 KeyCode/END       (move! view-node :selection :end)
                 KeyCode/LEFT      (move! view-node :selection :left)
                 KeyCode/RIGHT     (move! view-node :selection :right)
                 KeyCode/UP        (move! view-node :selection :up)
                 KeyCode/DOWN      (move! view-node :selection :down)
                 KeyCode/TAB       (deindent! view-node)
                 ::unhandled)

               shift-shortcut?
               (condp = (.getCode event)
                 KeyCode/LEFT  (move! view-node :selection :home)
                 KeyCode/RIGHT (move! view-node :selection :end)
                 ::unhandled)

               shortcut?
               (condp = (.getCode event)
                 KeyCode/LEFT  (move! view-node :navigation :home)
                 KeyCode/RIGHT (move! view-node :navigation :end)
                 ::unhandled))]

    ;; The event will be consumed even if its action had no effect.
    ;; Only unhandled events will be left unconsumed.
    (if (= ::unhandled diff)
      nil
      (do (.consume event)
          diff))))

(defn- handle-key-typed! [view-node ^KeyEvent event]
  (.consume event)
  (let [typed (.getCharacter event)
        undo-grouping (if (= "\r" typed) :newline :typing)]
    (set-properties! view-node undo-grouping
                     (data/key-typed (g/node-value view-node :lines)
                                     (g/node-value view-node :cursor-ranges)
                                     (g/node-value view-node :layout)
                                     typed
                                     (.isMetaDown event)
                                     (.isControlDown event)))))

(defn- handle-mouse-pressed! [view-node ^MouseEvent event]
  (.consume event)
  (.requestFocus ^Node (.getTarget event))
  (let [undo-grouping (if (< 1 (.getClickCount event)) :selection :navigation)
        diff (data/mouse-pressed (g/node-value view-node :lines)
                                 (g/node-value view-node :cursor-ranges)
                                 (g/node-value view-node :layout)
                                 (mouse-button event)
                                 (.getClickCount event)
                                 (.getX event)
                                 (.getY event)
                                 (.isShiftDown event)
                                 (.isShortcutDown event))]
    (when-some [reference-cursor-range (:reference-cursor-range diff)]
      (ui/user-data! (.getTarget event) :reference-cursor-range reference-cursor-range))
    (set-properties! view-node undo-grouping (dissoc diff :reference-cursor-range))))

(defn- handle-mouse-dragged! [view-node ^MouseDragEvent event]
  (.consume event)
  (set-properties! view-node :selection
                   (data/mouse-dragged (g/node-value view-node :lines)
                                       (g/node-value view-node :cursor-ranges)
                                       (ui/user-data (.getTarget event) :reference-cursor-range)
                                       (g/node-value view-node :layout)
                                       (mouse-button event)
                                       (.getClickCount event)
                                       (.getX event)
                                       (.getY event))))

(defn- handle-mouse-released! [^MouseEvent event]
  (.consume event)
  (ui/user-data! (.getTarget event) :reference-cursor-range nil))

(defn- handle-scroll! [view-node ^ScrollEvent event]
  (.consume event)
  (set-properties! view-node :navigation
                   (data/scroll (g/node-value view-node :lines)
                                (g/node-value view-node :scroll-x)
                                (g/node-value view-node :scroll-y)
                                (g/node-value view-node :layout)
                                (.getDeltaX event)
                                (.getDeltaY event))))

;; -----------------------------------------------------------------------------

(handler/defhandler :cut :new-code-view
  (enabled? [view-node] (not-every? data/cursor-range-empty? (g/node-value view-node :cursor-ranges)))
  (run [view-node clipboard]
       (set-properties! view-node nil
                        (data/cut! (g/node-value view-node :lines)
                                   (g/node-value view-node :cursor-ranges)
                                   (g/node-value view-node :layout)
                                   clipboard))))

(handler/defhandler :copy :new-code-view
  (enabled? [view-node] (not-every? data/cursor-range-empty? (g/node-value view-node :cursor-ranges)))
  (run [view-node clipboard]
       (set-properties! view-node nil
                        (data/copy! (g/node-value view-node :lines)
                                    (g/node-value view-node :cursor-ranges)
                                    clipboard))))

(handler/defhandler :paste :new-code-view
  (enabled? [view-node clipboard] (data/can-paste? (g/node-value view-node :cursor-ranges) clipboard))
  (run [view-node clipboard]
       (set-properties! view-node nil
                        (data/paste (g/node-value view-node :lines)
                                    (g/node-value view-node :cursor-ranges)
                                    (g/node-value view-node :layout)
                                    clipboard))))

(handler/defhandler :delete :new-code-view
  (run [view-node] (delete! view-node data/delete-character-after-cursor)))

(handler/defhandler :select-next-occurrence :new-code-view
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :select-next-occurrence :new-console
  (run [view-node] (select-next-occurrence! view-node)))

(handler/defhandler :split-selection-into-lines :new-code-view
  (run [view-node]
       (set-properties! view-node :selection
                        (data/split-selection-into-lines (g/node-value view-node :lines)
                                                         (g/node-value view-node :cursor-ranges)))))

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
    (ui/bind-keys! find-bar {KeyCode/ENTER :new-find-next})
    (ui/bind-action! next :new-find-next)
    (ui/bind-action! prev :new-find-prev))
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
    (ui/bind-action! next :new-find-next)
    (ui/bind-action! replace :new-replace-next)
    (ui/bind-keys! replace-bar {KeyCode/ENTER :new-replace-next})
    (ui/bind-action! replace-all :new-replace-all))
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
  (when-some [single-cursor-range (util/only (g/node-value view-node :cursor-ranges))]
    (when-not (data/cursor-range-empty? single-cursor-range)
      (let [lines (g/node-value view-node :lines)
            selected-lines (data/subsequence->lines (data/cursor-range-subsequence lines single-cursor-range))
            selected-text (string/join "\n" selected-lines)]
        selected-text))))

(defn- find-next! [view-node]
  (set-properties! view-node :selection
                   (data/find-next (g/node-value view-node :lines)
                                   (g/node-value view-node :cursor-ranges)
                                   (g/node-value view-node :layout)
                                   (string/split-lines (.getValue find-term-property))
                                   (.getValue find-wrap-property))))

(defn- find-prev! [view-node]
  (set-properties! view-node :selection
                   (data/find-prev (g/node-value view-node :lines)
                                   (g/node-value view-node :cursor-ranges)
                                   (g/node-value view-node :layout)
                                   (string/split-lines (.getValue find-term-property))
                                   (.getValue find-wrap-property))))

(defn- replace-next! [view-node]
  (set-properties! view-node nil
                   (data/replace-next (g/node-value view-node :lines)
                                      (g/node-value view-node :cursor-ranges)
                                      (g/node-value view-node :layout)
                                      (string/split-lines (.getValue find-term-property))
                                      (string/split-lines (.getValue find-replacement-property))
                                      (.getValue find-wrap-property))))

(defn- replace-all! [view-node]
  (set-properties! view-node nil
                   (data/replace-all (g/node-value view-node :lines)
                                     (g/node-value view-node :cursor-ranges)
                                     (g/node-value view-node :layout)
                                     (string/split-lines (.getValue find-term-property))
                                     (string/split-lines (.getValue find-replacement-property)))))

(handler/defhandler :new-find-text :new-code-view
  (run [find-bar grid view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-find-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :new-replace-text :new-code-view
  (run [grid replace-bar view-node]
       (when-some [selected-text (non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (set-find-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :new-find-text :new-console ;; In practice, from find / replace bars.
  (run [find-bar grid]
       (set-find-ui-type! :find)
       (focus-term-field! find-bar)))

(handler/defhandler :new-replace-text :new-console
  (run [grid replace-bar]
       (set-find-ui-type! :replace)
       (focus-term-field! replace-bar)))

(handler/defhandler :new-code-view-escape :new-console
  (run [find-bar grid replace-bar view-node]
       (let [cursor-ranges (g/node-value view-node :cursor-ranges)]
         (cond
           (find-ui-visible?)
           (do (set-find-ui-type! :hidden)
               (focus-code-editor! view-node))

           (< 1 (count cursor-ranges))
           (single-selection! view-node)

           (not (data/cursor-range-empty? (first cursor-ranges)))
           (let [cursor (data/CursorRange->Cursor (first cursor-ranges))
                 cursor-range (data/Cursor->CursorRange cursor)]
             (set-properties! view-node :selection {:cursor-ranges [cursor-range]}))))))

(handler/defhandler :new-find-next :new-find-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :new-find-next :new-replace-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :new-find-next :new-code-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :new-find-prev :new-find-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :new-find-prev :new-replace-bar
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :new-find-prev :new-code-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :new-replace-next :new-replace-bar
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :new-replace-next :new-code-view
  (run [view-node] (replace-next! view-node)))

(handler/defhandler :new-replace-all :new-replace-bar
  (run [view-node] (replace-all! view-node)))

;; -----------------------------------------------------------------------------

(ui/extend-menu ::menubar :editor.app-view/edit-end
                [{:label "Find..."
                  :command :new-find-text
                  :acc "Shortcut+F"}
                 {:label "Find Next"
                  :command :new-find-next
                  :acc "Shortcut+G"}
                 {:label "Find Previous"
                  :command :new-find-prev
                  :acc "Shift+Shortcut+G"}
                 {:label :separator}
                 {:label "Replace..."
                  :command :new-replace-text
                  :acc "Alt+E"}
                 {:label "Replace Next"
                  :command :new-replace-next
                  :acc "Alt+Shortcut+E"}
                 {:label :separator}
                 {:label "Select Next Occurrence"
                  :command :select-next-occurrence
                  :acc "Shortcut+D"}
                 {:label "Split Selection Into Lines"
                  :command :split-selection-into-lines
                  :acc "Shift+Shortcut+L"}])

;; -----------------------------------------------------------------------------

(defn- setup-view! [view-node resource-node]
  (g/transact
    (concat
      (g/connect resource-node :cursor-ranges view-node :cursor-ranges)
      (g/connect resource-node :invalidated-rows view-node :invalidated-rows)
      (g/connect resource-node :lines view-node :lines)))
  view-node)

(defn- repaint-view! [view-node]
  (g/node-value view-node :repaint))

(defn- make-view! [graph parent resource-node opts]
  (let [grid (GridPane.)
        canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        undo-grouping-info (pair :navigation (gensym))
        view-node (setup-view! (g/make-node! graph CodeEditorView :canvas canvas :undo-grouping-info undo-grouping-info :highlighted-find-term (.getValue highlighted-find-term-property)) resource-node)
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

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.setCursor javafx.scene.Cursor/TEXT)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (handle-key-pressed! view-node event)))
      (.addEventHandler KeyEvent/KEY_TYPED (ui/event-handler event (handle-key-typed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (handle-mouse-dragged! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (handle-mouse-released! event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (handle-scroll! view-node event))))

    (ui/context! grid :new-console context-env nil)
    (ui/bind-keys! grid {KeyCode/ESCAPE :new-code-view-escape})

    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints canvas-pane 0 0)
    (GridPane/setVgrow canvas-pane Priority/ALWAYS)

    (ui/children! grid [canvas-pane find-bar replace-bar])
    (ui/children! parent [grid])
    (ui/fill-control grid)
    (ui/context! canvas :new-code-view context-env nil)

    ;; Steal input focus when our tab becomes selected.
    (ui/observe (.selectedProperty ^Tab (:tab opts))
                (fn [_ _ became-selected?]
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
