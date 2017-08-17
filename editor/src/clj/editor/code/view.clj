(ns editor.code.view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import (com.sun.javafx.tk FontLoader Toolkit)
           (editor.code.data Cursor CursorRange LayoutInfo Rect)
           (javafx.scene Node)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.input Clipboard KeyCode KeyEvent MouseButton MouseDragEvent MouseEvent ScrollEvent)
           (javafx.scene.layout ColumnConstraints GridPane Pane Priority)
           (javafx.scene.paint Color LinearGradient)
           (javafx.scene.text Font FontSmoothingType TextAlignment)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(def ^:private ^Color foreground-color (Color/valueOf "#F8F8F2"))
(def ^:private ^Color background-color (Color/valueOf "#272822"))
(def ^:private ^Color selection-background-color (Color/valueOf "#49483E"))
(def ^:private ^Color selection-occurrence-outline-color (Color/valueOf "#C4C4BD"))
(def ^:private ^Color gutter-foreground-color (Color/valueOf "#90908A"))
(def ^:private ^Color gutter-background-color background-color)
(def ^:private ^Color gutter-cursor-line-background-color (Color/valueOf "#3E3D32"))
(def ^:private ^Color gutter-shadow-gradient (LinearGradient/valueOf "to right, rgba(0, 0, 0, 0.3) 0%, transparent 100%"))
(def ^:private ^Color scroll-tab-color (.deriveColor foreground-color 0 1 1 0.15))

(def ^:private scope-colors
  [["comment" (Color/valueOf "#75715E")]
   ["string" (Color/valueOf "#E6DB74")]
   ["constant" (Color/valueOf "#AE81FF")]
   ["keyword" (Color/valueOf "#F92672")]
   ["support.function" (Color/valueOf "#66D9EF")]
   ["name.function" (Color/valueOf "#A6E22E")]
   ["parameter.function" (Color/valueOf "#FD971F")]
   ["variable.language" (Color/valueOf "#FD971F")]])

(defn- scope->color
  ^Color [scope]
  (loop [scope-colors scope-colors]
    (if-some [[pattern color] (first scope-colors)]
      (if (string/includes? scope pattern)
        color
        (recur (next scope-colors)))
      foreground-color)))

(defn- draw! [^GraphicsContext gc ^Font font ^LayoutInfo layout lines cursor-ranges syntax-info tab-spaces]
  (let [source-line-count (count lines)
        dropped-line-count (.dropped-line-count layout)
        drawn-line-count (.drawn-line-count layout)
        ^double ascent (data/ascent (.glyph layout))
        ^double line-height (data/line-height (.glyph layout))
        indent-string (string/join (repeat tab-spaces \space))]
    (.setFill gc background-color)
    (.fillRect gc 0 0 (.. gc getCanvas getWidth) (.. gc getCanvas getHeight))
    (.setFont gc font)
    (.setFontSmoothingType gc FontSmoothingType/LCD)

    ;; Draw selection backgrounds.
    (.setFill gc selection-background-color)
    (doseq [^CursorRange cursor-range cursor-ranges]
      (doseq [^Rect r (data/cursor-range-rects layout lines (data/adjust-cursor-range lines cursor-range))]
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))))

    ;; Highlight occurrences of selected word.
    (when-some [selected-word-cursor-range (data/selected-word-cursor-range lines cursor-ranges)]
      (let [needle-lines (data/subsequence->lines (data/cursor-range-subsequence lines selected-word-cursor-range))]
        (.setStroke gc selection-occurrence-outline-color)
        (.setLineWidth gc 1.0)
        (loop [from-cursor (data/->Cursor (max 0 (- dropped-line-count (count needle-lines))) 0)]
          (when-some [matching-cursor-range (data/find-next-occurrence lines needle-lines from-cursor)]
            (when (< (- (.row (data/cursor-range-start matching-cursor-range))
                        dropped-line-count)
                     drawn-line-count)
              (when (and (data/word-cursor-range? lines matching-cursor-range)
                         (not (data/cursor-range-equals? selected-word-cursor-range matching-cursor-range)))
                (let [^Rect word-rect (first (data/cursor-range-rects layout lines matching-cursor-range))
                      ^Rect r (data/expand-rect word-rect 1.5 -0.5)]
                  (.strokeRoundRect gc (.x r) (.y r) (.w r) (.h r) 5.0 5.0)))
              (recur (data/cursor-range-end matching-cursor-range)))))))

    ;; Draw syntax-highlighted code.
    (.setTextAlign gc TextAlignment/LEFT)
    (loop [drawn-line-index 0
           source-line-index dropped-line-count]
      (when (and (< drawn-line-index drawn-line-count)
                 (< source-line-index source-line-count))
        (let [line (lines source-line-index)
              runs (second (syntax-info source-line-index))]
          (loop [i 0
                 text-offset 0.0]
            (when-some [[start scope] (get runs i)]
              (let [end (or (first (get runs (inc i)))
                            (count line))
                    text (subs line start end)
                    ^double text-width (data/string-width (.glyph layout) text)]
                (when-not (string/blank? text)
                  (.setFill gc (scope->color scope))
                  (.fillText gc (string/replace text #"\t" indent-string)
                             (+ (.x ^Rect (.canvas layout))
                                (.scroll-x layout)
                                text-offset)
                             (+ ascent
                                (.scroll-y-remainder layout)
                                (* drawn-line-index line-height))))
                (recur (inc i)
                       (+ text-offset text-width)))))
          (recur (inc drawn-line-index)
                 (inc source-line-index)))))

    ;; Draw cursors.
    (.setFill gc foreground-color)
    (doseq [^CursorRange cursor-range cursor-ranges]
      (let [r (data/cursor-rect layout lines (data/adjust-cursor lines (.to cursor-range)))]
        (.fillRect gc (.x r) (.y r) (.w r) (.h r))))

    ;; Draw scroll bar.
    (.setFill gc scroll-tab-color)
    (let [rect ^Rect (.scroll-tab-y layout)
          radius (.w rect)]
      (.fillRoundRect gc (.x rect) (.y rect) (.w rect) (.h rect) radius radius))

    ;; Draw gutter background when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-background-color)
      (.fillRect gc 0 0 (.x ^Rect (.canvas layout)) (.h ^Rect (.canvas layout))))

    ;; Highlight lines with cursors in gutter.
    (.setFill gc gutter-cursor-line-background-color)
    (let [highlight-width (- (.x ^Rect (.canvas layout)) (Math/ceil (data/string-width (.glyph layout) " ")))]
      (doseq [row (sequence (comp (map data/CursorRange->Cursor)
                                  (map (fn [^Cursor cursor] (.row cursor)))
                                  (map (partial data/adjust-row lines))
                                  (drop-while (partial > dropped-line-count))
                                  (take-while (partial > (+ dropped-line-count drawn-line-count))))
                            cursor-ranges)]
        (let [y (data/row->y layout row)]
          (.fillRect gc 0 y highlight-width line-height))))

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

(g/defnk produce-repaint [^Canvas canvas font layout lines cursor-ranges syntax-info tab-spaces]
  (draw! (.getGraphicsContext2D canvas) font layout lines cursor-ranges syntax-info tab-spaces))

(g/defnode CodeEditorView
  (inherits view/WorkbenchView)

  (property canvas Canvas (dynamic visible (g/constantly false)))
  (property canvas-width g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property canvas-height g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-x g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property scroll-y g/Num (default 0.0) (dynamic visible (g/constantly false)))
  (property font-name g/Str (default "Dejavu Sans Mono"))
  (property font-size g/Num (default 12.0))
  (property tab-spaces g/Num (default 4))

  (input lines r/Lines)
  (input cursor-ranges r/CursorRanges)
  (input syntax-info r/SyntaxInfo)

  (output font Font :cached produce-font)
  (output glyph-metrics data/GlyphMetrics :cached produce-glyph-metrics)
  (output layout LayoutInfo :cached produce-layout)
  (output repaint g/Any :cached produce-repaint))

(def ^:private resource-property? #{:cursor-ranges :lines :syntax-info})

(defn- set-properties! [view-node values-by-prop-kw]
  (when-not (empty? values-by-prop-kw)
    (let [resource-node (g/node-value view-node :resource-node)]
      (g/transact
        (into []
              (mapcat (fn [[prop-kw value]]
                        (let [node-id (if (resource-property? prop-kw) resource-node view-node)]
                          (g/set-property node-id prop-kw value))))
              values-by-prop-kw)))))

(defn- mouse-button [^MouseEvent event]
  (condp = (.getButton event)
    MouseButton/NONE nil
    MouseButton/PRIMARY :primary
    MouseButton/SECONDARY :secondary
    MouseButton/MIDDLE :middle))

;; -----------------------------------------------------------------------------

(defn- handle-scroll! [view-node ^ScrollEvent event]
  (.consume event)
  (set-properties! view-node
                   (data/scroll (g/node-value view-node :lines)
                                (g/node-value view-node :scroll-x)
                                (g/node-value view-node :scroll-y)
                                (g/node-value view-node :layout)
                                (.getDeltaX event)
                                (.getDeltaY event))))

(defn- handle-key-typed! [view-node ^KeyEvent event]
  (.consume event)
  (set-properties! view-node
                   (data/key-typed (g/node-value view-node :lines)
                                   (g/node-value view-node :syntax-info)
                                   (g/node-value view-node :cursor-ranges)
                                   (g/node-value view-node :layout)
                                   (.getCharacter event)
                                   (.isMetaDown event)
                                   (.isControlDown event))))

(defn- handle-mouse-pressed! [view-node ^MouseEvent event]
  (.consume event)
  (let [diff (data/mouse-pressed (g/node-value view-node :lines)
                                 (g/node-value view-node :cursor-ranges)
                                 (g/node-value view-node :layout)
                                 (mouse-button event)
                                 (.getClickCount event)
                                 (.getX event)
                                 (.getY event)
                                 (.isShortcutDown event))]
    (when-some [reference-cursor-range (:reference-cursor-range diff)]
      (ui/user-data! (.getTarget event) :reference-cursor-range reference-cursor-range))
    (set-properties! view-node (dissoc diff :reference-cursor-range))))

(defn- handle-mouse-dragged! [view-node ^MouseDragEvent event]
  (.consume event)
  (set-properties! view-node
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

;; -----------------------------------------------------------------------------

(defn- setup-view! [view-node resource-node]
  (g/transact
    (concat
      (g/connect resource-node :lines view-node :lines)
      (g/connect resource-node :cursor-ranges view-node :cursor-ranges)
      (g/connect resource-node :syntax-info view-node :syntax-info)))
  view-node)

(defn- setup-find-bar! [^GridPane find-bar]
  ;; TODO: Bind controls.
  (doto find-bar
    (.setMaxWidth Double/MAX_VALUE)))

(defn- setup-replace-bar! [^GridPane replace-bar]
  ;; TODO: Bind controls.
  (doto replace-bar
    (.setMaxWidth Double/MAX_VALUE)))

(defn- repaint-view! [view-node]
  (when-some [resource-node (g/node-value view-node :resource-node)]
    (when-some [resource (g/node-value resource-node :resource)]
      (let [lines (g/node-value resource-node :lines)
            syntax-info (g/node-value resource-node :syntax-info)
            layout (g/node-value view-node :layout)
            grammar (:grammar (:view-opts (resource/resource-type resource)))
            fresh-syntax-info (data/highlight-visible-syntax lines syntax-info layout grammar)]
        (when-not (identical? syntax-info fresh-syntax-info)
          (g/set-property! resource-node :syntax-info fresh-syntax-info))
        (g/node-value view-node :repaint)))))

(defn- make-view [graph parent resource-node opts]
  (let [grid (GridPane.)
        canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        view-node (setup-view! (g/make-node! graph CodeEditorView :canvas canvas) resource-node)
        find-bar (setup-find-bar! (ui/load-fxml "find.fxml"))
        replace-bar (setup-replace-bar! (ui/load-fxml "replace.fxml"))
        repainter (ui/->timer 60 "repaint-code-editor-view" (fn [_ _] (repaint-view! view-node)))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :find-bar find-bar
                     :replace-bar replace-bar}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.setCursor javafx.scene.Cursor/TEXT)
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (handle-scroll! view-node event)))
      (.addEventHandler KeyEvent/KEY_TYPED (ui/event-handler event (handle-key-typed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (handle-mouse-dragged! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (handle-mouse-released! event))))

    (ui/context! grid :new-console context-env nil)
    (ui/bind-keys! grid {KeyCode/ESCAPE :hide-bars})

    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints canvas-pane 0 0)
    (GridPane/setVgrow canvas-pane Priority/ALWAYS)

    (ui/children! grid [canvas-pane])
    (ui/children! parent [grid])
    (ui/fill-control grid)
    (ui/context! canvas :new-code-view context-env nil)

    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! (:tab opts) repainter)
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :new-code
                                :label "Code"
                                :make-view-fn make-view))
