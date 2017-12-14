(ns editor.console
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.integration :as code-integration]
            [editor.code.resource :as r]
            [editor.code.util :refer [split-lines]]
            [editor.code.view :as view]
            [editor.handler :as handler]
            [editor.ui :as ui])
  (:import (editor.code.data Cursor CursorRange LayoutInfo Rect)
           (javafx.beans.property SimpleStringProperty)
           (javafx.scene Parent)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.control Button Tab TabPane TextField)
           (javafx.scene.input Clipboard KeyCode KeyEvent MouseEvent ScrollEvent)
           (javafx.scene.layout GridPane Pane Region)
           (javafx.scene.paint Color)
           (javafx.scene.text Font FontSmoothingType TextAlignment)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private *pending (atom {:clear? false :entries []}))
(def ^:private gutter-bubble-font (Font. "Source Sans Pro", 10.5))
(def ^:private gutter-bubble-glyph-metrics (view/make-glyph-metrics gutter-bubble-font 1.0))

(defn append-console-entry!
  "Append to the console. Callable from a background thread. If type is
  non-nil, a region of the specified type will encompass the line."
  [type line]
  (assert (or (nil? type) (keyword? type)))
  (assert (string? line))
  (swap! *pending update :entries conj [type line]))

(def append-console-line! (partial append-console-entry! nil))

(defn clear-console!
  "Clear the console. Callable from a background thread."
  []
  (reset! *pending {:clear? true :entries []}))

(defn- flip-pending! []
  (let [*unconsumed (volatile! nil)]
    (swap! *pending (fn [pending]
                      (vreset! *unconsumed pending)
                      {:clear? false :entries []}))
    @*unconsumed))

(defn show! [view-node]
  (let [canvas (g/node-value view-node :canvas)
        ^TabPane tab-pane (ui/closest-node-of-type TabPane canvas)]
    (.select (.getSelectionModel tab-pane) 0)))

;; -----------------------------------------------------------------------------
;; Tool Bar
;; -----------------------------------------------------------------------------

(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))

(defn- setup-tool-bar!
  ^Parent [^Parent tool-bar view-node]
  (ui/with-controls tool-bar [^TextField search-console ^Button prev-console ^Button next-console ^Button clear-console]
    (ui/context! tool-bar :console-tool-bar {:term-field search-console :view-node view-node} nil)
    (.bindBidirectional (.textProperty search-console) find-term-property)
    (ui/bind-keys! search-console {KeyCode/ENTER :find-next})
    (ui/bind-action! prev-console :find-prev)
    (ui/bind-action! next-console :find-next)
    (ui/bind-action! clear-console :clear-console))
  tool-bar)

(defn- dispose-tool-bar! [^Parent tool-bar]
  (ui/with-controls tool-bar [^TextField search-console]
    (.unbindBidirectional (.textProperty search-console) find-term-property)))

(defn- focus-term-field! [^Parent bar]
  (ui/with-controls bar [^TextField search-console]
    (.requestFocus search-console)
    (.selectAll search-console)))

(defn- set-find-term! [^String term-text]
  (.setValue find-term-property (or term-text "")))

(defn- find-next! [view-node]
  (view/set-properties! view-node :selection
                        (data/find-next (view/get-property view-node :lines)
                                        (view/get-property view-node :cursor-ranges)
                                        (view/get-property view-node :layout)
                                        (split-lines (.getValue find-term-property))
                                        false
                                        false
                                        true)))

(defn- find-prev! [view-node]
  (view/set-properties! view-node :selection
                        (data/find-prev (view/get-property view-node :lines)
                                        (view/get-property view-node :cursor-ranges)
                                        (view/get-property view-node :layout)
                                        (split-lines (.getValue find-term-property))
                                        false
                                        false
                                        true)))

(handler/defhandler :find-text :console-view
  (run [term-field view-node]
       (when-some [selected-text (view/non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (focus-term-field! term-field)))

(handler/defhandler :find-next :console-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :console-tool-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-prev :console-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :console-tool-bar
  (run [view-node] (find-prev! view-node)))

;; -----------------------------------------------------------------------------
;; Read-only code view action handlers
;; -----------------------------------------------------------------------------

(handler/defhandler :select-up :console-view
  (run [view-node] (view/move! view-node :selection :up)))

(handler/defhandler :select-down :console-view
  (run [view-node] (view/move! view-node :selection :down)))

(handler/defhandler :select-left :console-view
  (run [view-node] (view/move! view-node :selection :left)))

(handler/defhandler :select-right :console-view
  (run [view-node] (view/move! view-node :selection :right)))

(handler/defhandler :prev-word :console-view
  (run [view-node] (view/move! view-node :navigation :prev-word)))

(handler/defhandler :select-prev-word :console-view
  (run [view-node] (view/move! view-node :selection :prev-word)))

(handler/defhandler :next-word :console-view
  (run [view-node] (view/move! view-node :navigation :next-word)))

(handler/defhandler :select-next-word :console-view
  (run [view-node] (view/move! view-node :selection :next-word)))

(handler/defhandler :beginning-of-line :console-view
  (run [view-node] (view/move! view-node :navigation :line-start)))

(handler/defhandler :select-beginning-of-line :console-view
  (run [view-node] (view/move! view-node :selection :line-start)))

(handler/defhandler :beginning-of-line-text :console-view
  (run [view-node] (view/move! view-node :navigation :home)))

(handler/defhandler :select-beginning-of-line-text :console-view
  (run [view-node] (view/move! view-node :selection :home)))

(handler/defhandler :end-of-line :console-view
  (run [view-node] (view/move! view-node :navigation :end)))

(handler/defhandler :select-end-of-line :console-view
  (run [view-node] (view/move! view-node :selection :end)))

(handler/defhandler :page-up :console-view
  (run [view-node] (view/page-up! view-node :navigation)))

(handler/defhandler :select-page-up :console-view
  (run [view-node] (view/page-up! view-node :selection)))

(handler/defhandler :page-down :console-view
  (run [view-node] (view/page-down! view-node :navigation)))

(handler/defhandler :select-page-down :console-view
  (run [view-node] (view/page-down! view-node :selection)))

(handler/defhandler :beginning-of-file :console-view
  (run [view-node] (view/move! view-node :navigation :file-start)))

(handler/defhandler :select-beginning-of-file :console-view
  (run [view-node] (view/move! view-node :selection :file-start)))

(handler/defhandler :end-of-file :console-view
  (run [view-node] (view/move! view-node :navigation :file-end)))

(handler/defhandler :select-end-of-file :console-view
  (run [view-node] (view/move! view-node :selection :file-end)))

(handler/defhandler :copy :console-view
  (enabled? [view-node] (view/has-selection? view-node))
  (run [view-node clipboard] (view/copy! view-node clipboard)))

(handler/defhandler :select-all :console-view
  (run [view-node] (view/select-all! view-node)))

(when code-integration/use-new-code-editor?
  (handler/defhandler :select-next-occurrence :console-view
    (run [view-node] (view/select-next-occurrence! view-node)))

  (handler/defhandler :select-next-occurrence :console-tool-bar
    (run [view-node] (view/select-next-occurrence! view-node)))

  (handler/defhandler :split-selection-into-lines :console-view
    (run [view-node] (view/split-selection-into-lines! view-node))))

;; -----------------------------------------------------------------------------
;; Console view action handlers
;; -----------------------------------------------------------------------------

(handler/defhandler :clear-console :console-tool-bar
  (run [view-node] (clear-console!)))

;; -----------------------------------------------------------------------------
;; Setup
;; -----------------------------------------------------------------------------

(g/defnode ConsoleNode
  (property cursor-ranges r/CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows r/InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property lines r/Lines (default [""]) (dynamic visible (g/constantly false)))
  (property regions r/Regions (default []) (dynamic visible (g/constantly false))))

(defn- gutter-metrics []
  [44.0 0.0])

(defn- draw-gutter! [^GraphicsContext gc ^Rect gutter-rect ^LayoutInfo layout ^Font font color-scheme lines regions]
  (let [glyph-metrics (.glyph layout)
        ^double line-height (data/line-height glyph-metrics)
        ^double ascent (data/ascent glyph-metrics)
        ^double gutter-bubble-ascent (data/ascent gutter-bubble-glyph-metrics)
        visible-regions (data/visible-cursor-ranges lines layout regions)
        text-right (Math/floor (- (+ (.x gutter-rect) (.w gutter-rect)) (/ line-height 2.0) 3.0))
        bubble-background-color (Color/valueOf "rgba(255, 255, 255, 0.1)")
        ^Color gutter-foreground-color (view/color-lookup color-scheme "editor.gutter.foreground")
        gutter-background-color (view/color-lookup color-scheme "editor.gutter.background")
        gutter-shadow-color (view/color-lookup color-scheme "editor.gutter.shadow")
        gutter-eval-expression-color (view/color-lookup color-scheme "editor.gutter.eval.expression")
        gutter-eval-result-color (view/color-lookup color-scheme "editor.gutter.eval.result")]

    ;; Draw gutter background and shadow when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-background-color)
      (.fillRect gc (.x gutter-rect) (.y gutter-rect) (.w gutter-rect) (.h gutter-rect))
      (.setFill gc gutter-shadow-color)
      (.fillRect gc (+ (.x gutter-rect) (.w gutter-rect)) 0.0 8.0 (.h gutter-rect)))

    ;; Draw gutter annotations.
    (.setFontSmoothingType gc FontSmoothingType/LCD)
    (.setTextAlign gc TextAlignment/RIGHT)
    (doseq [^CursorRange region visible-regions]
      (let [line-y (data/row->y layout (.row ^Cursor (.from region)))]
        (case (:type region)
          :repeat
          (let [^long repeat-count (:count region)
                overflow? (<= 1000 repeat-count)
                repeat-text (if overflow?
                              (format "+9%02d" (mod repeat-count 100))
                              (str repeat-count))
                text-top (+ 2.0 line-y)
                text-bottom (+ gutter-bubble-ascent text-top)
                text-width (Math/ceil (data/text-width gutter-bubble-glyph-metrics repeat-text))
                ^double text-height (data/line-height gutter-bubble-glyph-metrics)
                text-left (- text-right text-width)
                ^Rect bubble-rect (data/expand-rect (data/->Rect text-left text-top text-width text-height) 3.0 0.0)]
            (.setFill gc bubble-background-color)
            (.fillRoundRect gc (.x bubble-rect) (.y bubble-rect) (.w bubble-rect) (.h bubble-rect) 5.0 5.0)
            (.setFont gc gutter-bubble-font)
            (.setFill gc gutter-foreground-color)
            (.fillText gc repeat-text text-right text-bottom))

          :eval-expression
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-expression-color)
            (.fillText gc ">" text-right text-y))

          :eval-result
          (let [text-y (+ ascent line-y)]
            (.setFont gc font)
            (.setFill gc gutter-eval-result-color)
            (.fillText gc "=" text-right text-y))
          nil)))))

(deftype ConsoleGutterView []
  view/GutterView

  (gutter-metrics [_this _lines _regions _glyph-metrics]
    (gutter-metrics))

  (draw-gutter! [_this gc gutter-rect layout font color-scheme lines regions _visible-cursors]
    (draw-gutter! gc gutter-rect layout font color-scheme lines regions)))

(defn- setup-view! [console-node view-node]
  (g/transact
    (concat
      (g/connect console-node :_node-id view-node :resource-node)
      (g/connect console-node :cursor-ranges view-node :cursor-ranges)
      (g/connect console-node :invalidated-rows view-node :invalidated-rows)
      (g/connect console-node :lines view-node :lines)
      (g/connect console-node :regions view-node :regions)))
  view-node)

(defn- make-region
  ^CursorRange [^long row [type line]]
  (assert (keyword? type))
  (assert (string? line))
  (assoc (data/->CursorRange (data/->Cursor row 0)
                             (data/->Cursor row (count line)))
    :type type))

(defn- append-distinct-lines [{:keys [lines regions] :as props} entries]
  (merge props (data/append-distinct-lines lines regions (mapv second entries))))

(defn- append-regioned-lines [{:keys [lines regions] :as props} entries]
  (assoc props
    :lines (into lines (map second) entries)
    :regions (into regions (map make-region (iterate inc (count lines)) entries))))

(defn- repaint-console-view! [view-node elapsed-time]
  (let [{:keys [clear? entries]} (flip-pending!)]
    (when (or clear? (seq entries))
      (let [^LayoutInfo prev-layout (g/node-value view-node :layout)
            prev-lines (g/node-value view-node :lines)
            prev-document-width (if clear? 0.0 (.document-width prev-layout))
            appended-width (data/max-line-width (.glyph prev-layout) (.tab-stops prev-layout) (mapv second entries))
            document-width (max prev-document-width ^double appended-width)
            was-scrolled-to-bottom? (data/scrolled-to-bottom? prev-layout (count prev-lines))
            props (reduce (fn [props entries]
                            (if (nil? (ffirst entries))
                              (append-distinct-lines props entries)
                              (append-regioned-lines props entries)))
                          {:lines (if clear? [""] prev-lines)
                           :regions (if clear? [] (g/node-value view-node :regions))}
                          (partition-by #(nil? (first %)) entries))]
        (view/set-properties! view-node nil
                              (cond-> (assoc props :document-width document-width)
                                      was-scrolled-to-bottom? (assoc :scroll-y (data/scroll-to-bottom prev-layout (count (:lines props))))
                                      clear? (assoc :cursor-ranges [data/document-start-cursor-range])
                                      clear? (data/frame-cursor prev-layout))))))
  (view/repaint-view! view-node elapsed-time))

(def ^:private console-grammar
  {:name "Console"
   :scope-name "source.console"
   :patterns [{:match #"^INFO:RESOURCE: (.+?) was successfully reloaded\."
               :name "console.reload.successful"}
              {:match #"^ERROR:.+?:"
               :name "console.error"}
              {:match #"^WARNING:.+?:"
               :name "console.warning"}
              {:match #"^INFO:.+?:"
               :name "console.info"}
              {:match #"^DEBUG:.+?:"
               :name "console.debug"}]})

(def ^:private console-color-scheme
  (view/make-color-scheme
    [["console.error" (Color/valueOf "#FF6161")]
     ["console.warning" (Color/valueOf "#FF9A34")]
     ["console.info" (Color/valueOf "#CCCFD3")]
     ["console.debug" (Color/valueOf "#3B8CF8")]
     ["console.reload.successful" (Color/valueOf "#33CC33")]
     ["editor.foreground" (Color/valueOf "#A2B0BE")]
     ["editor.background" (Color/valueOf "#27292D")]
     ["editor.cursor" Color/TRANSPARENT]
     ["editor.gutter.eval.expression" (Color/valueOf "#DDDDDD")]
     ["editor.gutter.eval.result" (Color/valueOf "#52575C")]
     ["editor.selection.background" (Color/valueOf "#264A8B")]
     ["editor.selection.background.inactive" (Color/valueOf "#264A8B")]
     ["editor.selection.occurrence.outline" (if code-integration/use-new-code-editor? (Color/valueOf "#A2B0BE") Color/TRANSPARENT)]]))

(defn make-console! [graph ^Tab console-tab ^GridPane console-grid-pane]
  (let [canvas (Canvas.)
        ^Pane canvas-pane (.lookup console-grid-pane "#console-canvas-pane")
        view-node (setup-view! (g/make-node! graph ConsoleNode)
                               (g/make-node! graph view/CodeEditorView
                                             :canvas canvas
                                             :color-scheme console-color-scheme
                                             :grammar console-grammar
                                             :gutter-view (ConsoleGutterView.)
                                             :highlighted-find-term (.getValue find-term-property)
                                             :line-height-factor 1.2
                                             :resize-reference :bottom))
        tool-bar (setup-tool-bar! (.lookup console-grid-pane "#console-tool-bar") view-node)
        repainter (ui/->timer "repaint-console-view" (fn [_ elapsed-time]
                                                       (when (.isSelected console-tab)
                                                         (repaint-console-view! view-node elapsed-time))))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :term-field (.lookup tool-bar "#search-console")
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (ui/add-child! canvas-pane canvas)
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))
    (ui/observe (.focusedProperty canvas) (fn [_ _ focused?] (g/set-property! view-node :focused? focused?)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (view/handle-key-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (view/handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (view/handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (view/handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (view/handle-mouse-released! view-node event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (view/handle-scroll! view-node event))))

    ;; Configure contexts.
    (ui/context! console-grid-pane :console-grid-pane context-env nil)
    (ui/context! canvas :console-view context-env nil)

    ;; Highlight occurrences of search term.
    (let [find-term-setter (view/make-property-change-setter view-node :highlighted-find-term)]
      (.addListener find-term-property find-term-setter)

      ;; Remove callbacks if the console tab is closed.
      (ui/on-closed! console-tab (fn [_]
                                   (ui/timer-stop! repainter)
                                   (dispose-tool-bar! tool-bar)
                                   (.removeListener find-term-property find-term-setter))))

    ;; Start repaint timer.
    (ui/timer-start! repainter)
    view-node))
