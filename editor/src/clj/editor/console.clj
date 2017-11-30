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
           (javafx.scene Node Parent)
           (javafx.scene.canvas Canvas GraphicsContext)
           (javafx.scene.control Button Tab TabPane TextField)
           (javafx.scene.input Clipboard KeyCode KeyEvent MouseEvent ScrollEvent)
           (javafx.scene.layout GridPane Pane Region)
           (javafx.scene.paint Color)
           (javafx.scene.text TextAlignment)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private *pending (atom {:clear? false :lines []}))

(defn append-console-line!
  "Append a line to the console. Callable from a background thread."
  [line]
  (swap! *pending update :lines conj line))

(defn clear-console!
  "Clear the console. Callable from a background thread."
  []
  (reset! *pending {:clear? true :lines []}))

(defn- flip-pending! []
  (let [*unconsumed (volatile! nil)]
    (swap! *pending (fn [pending]
                      (vreset! *unconsumed pending)
                      {:clear? false :lines []}))
    @*unconsumed))

(defn show! [view-node]
  (let [canvas (g/node-value view-node :canvas)
        ^TabPane tab-pane (ui/closest-node-of-type TabPane canvas)]
    (.select (.getSelectionModel tab-pane) 0)))

;; -----------------------------------------------------------------------------
;; Tool Bar
;; -----------------------------------------------------------------------------

(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))

(defn- refresh-tool-bar! [view-node ^Parent tool-bar]
  ;; TODO: Bind to debugger.
  (let [debugger-feature-enabled? false
        debugger-buttons-visible? false]
    (ui/with-controls tool-bar [^Parent debugger-tool-bar ^Node debugger-separator ^Button toggle-debugger]
      (.setVisible debugger-tool-bar (and debugger-feature-enabled? debugger-buttons-visible?))
      (.setVisible debugger-separator debugger-feature-enabled?)
      (.setVisible toggle-debugger debugger-feature-enabled?))))

(defn- setup-tool-bar! [^Parent tool-bar view-node]
  (ui/context! tool-bar :console-tool-bar {:tool-bar tool-bar :view-node view-node} nil)
  (ui/with-controls tool-bar [^TextField search-console ^Button prev-console ^Button next-console ^Button clear-console ^Parent debugger-tool-bar ^Node debugger-separator ^Button toggle-debugger]
    (.bind (.managedProperty debugger-tool-bar) (.visibleProperty debugger-tool-bar))
    (.bind (.managedProperty debugger-separator) (.visibleProperty debugger-separator))
    (.bind (.managedProperty toggle-debugger) (.visibleProperty toggle-debugger))
    (.bindBidirectional (.textProperty search-console) find-term-property)
    (ui/bind-keys! tool-bar {KeyCode/ENTER :find-next})
    (ui/bind-action! prev-console :find-prev)
    (ui/bind-action! next-console :find-next)
    (ui/bind-action! clear-console :clear-console)
    (.setMinWidth clear-console Region/USE_PREF_SIZE))
  (refresh-tool-bar! view-node tool-bar)
  tool-bar)

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
  (run [tool-bar view-node]
       (when-some [selected-text (view/non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (focus-term-field! tool-bar)))

(handler/defhandler :find-next :console-view
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-next :console-tool-bar
  (run [view-node] (find-next! view-node)))

(handler/defhandler :find-prev :console-view
  (run [view-node] (find-prev! view-node)))

(handler/defhandler :find-prev :console-tool-bar
  (run [view-node] (find-prev! view-node)))

;; -----------------------------------------------------------------------------
;; Prompt
;; -----------------------------------------------------------------------------

(defn- refresh-prompt! [view-node ^Parent prompt]
  ;; TODO: Bind to debugger.
  (let [debugger-feature-enabled? false
        debugger-prompt-visible? false]
    (.setVisible prompt (and debugger-feature-enabled? debugger-prompt-visible?))))

(defn- setup-prompt! [^Parent prompt view-node]
  (.bind (.managedProperty prompt) (.visibleProperty prompt))
  (refresh-prompt! view-node prompt)
  prompt)

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

(defn- gutter-metrics [regions glyph-metrics]
  [52.0 0.0])

(defn- draw-gutter! [^GraphicsContext gc ^Rect gutter-rect ^LayoutInfo layout color-scheme lines regions]
  (let [glyph-metrics (.glyph layout)
        ^double line-height (data/line-height glyph-metrics)
        ^double ascent (data/ascent glyph-metrics)
        visible-regions (data/visible-cursor-ranges lines layout regions)
        repeat-x (- (+ (.x gutter-rect) (.w gutter-rect)) (/ line-height 2.0))
        ^Color gutter-foreground-color (view/color-lookup color-scheme "editor.gutter.foreground")
        repeat-count-color (.deriveColor gutter-foreground-color 0 1 1 0.7)
        repeat-sign-color (.deriveColor gutter-foreground-color 0 1 1 0.4)
        gutter-background-color (view/color-lookup color-scheme "editor.gutter.background")
        gutter-shadow-color (view/color-lookup color-scheme "editor.gutter.shadow")]

    ;; Draw gutter background.
    (.setFill gc gutter-background-color)
    (.fillRect gc (.x gutter-rect) (.y gutter-rect) (.w gutter-rect) (.h gutter-rect))

    ;; Draw gutter shadow when scrolled horizontally.
    (when (neg? (.scroll-x layout))
      (.setFill gc gutter-shadow-color)
      (.fillRect gc (+ (.x gutter-rect) (.w gutter-rect)) 0.0 8.0 (.h gutter-rect)))

    (.setTextAlign gc TextAlignment/RIGHT)
    (doseq [^CursorRange region visible-regions]
      (let [y (data/row->y layout (.row ^Cursor (.from region)))]
        (case (:type region)
          :repeat
          (let [^long repeat-count (:count region)
                overflow? (<= 1000 repeat-count)
                repeat-y (+ ascent y)
                repeat-text (if overflow?
                              (format "9%02d  " (mod repeat-count 100))
                              (str repeat-count "  "))
                sign-text (if overflow? "+\u00D7" "\u00D7")] ; "x" (MULTIPLICATION SIGN)
            (.setFill gc repeat-count-color)
            (.fillText gc repeat-text repeat-x repeat-y)
            (.setFill gc repeat-sign-color)
            (.fillText gc sign-text repeat-x repeat-y))
          nil)))))

(deftype ConsoleGutterView []
  view/GutterView

  (gutter-metrics [this lines regions glyph-metrics]
    (gutter-metrics regions glyph-metrics))

  (draw-gutter! [this gc gutter-rect layout color-scheme lines regions visible-cursors]
    (draw-gutter! gc gutter-rect layout color-scheme lines regions)))

(defn- setup-view! [console-node view-node]
  (g/transact
    (concat
      (g/connect console-node :_node-id view-node :resource-node)
      (g/connect console-node :cursor-ranges view-node :cursor-ranges)
      (g/connect console-node :invalidated-rows view-node :invalidated-rows)
      (g/connect console-node :lines view-node :lines)
      (g/connect console-node :regions view-node :regions)))
  view-node)

(defn- repaint-console-view! [view-node elapsed-time]
  (let [{:keys [clear? lines]} (flip-pending!)
        prev-lines (if clear? [""] (g/node-value view-node :lines))
        prev-regions (if clear? [] (g/node-value view-node :regions))
        prev-layout (g/node-value view-node :layout)]
    (view/set-properties! view-node nil
                          (cond-> (data/append-distinct-lines prev-lines prev-regions prev-layout lines)
                                  clear? (assoc :cursor-ranges [data/document-start-cursor-range])
                                  clear? (data/frame-cursor prev-layout)))
    (view/repaint-view! view-node elapsed-time)))

(def ^:private console-grammar
  {:name "Console"
   :scope-name "source.console"
   :patterns [{:match #"^ERROR:.*:"
               :name "console.error"}
              {:match #"^WARNING:.*:"
               :name "console.warning"}
              {:match #"^INFO:.*:"
               :name "console.info"}
              {:match #"^DEBUG:.*:"
               :name "console.debug"}]})

(def ^:private console-color-scheme
  (view/make-color-scheme
    [["console.error" (Color/valueOf "#FF6161")]
     ["console.warning" (Color/valueOf "#FF9A34")]
     ["console.info" (Color/valueOf "#A1B1BF")]
     ["console.debug" (Color/valueOf "#3B8CF8")]
     ["editor.foreground" (Color/valueOf "#A1B1BF")]
     ["editor.background" (Color/valueOf "#27292D")]
     ["editor.cursor" Color/TRANSPARENT]
     ["editor.selection.background" (Color/valueOf "#264A8B")]
     ["editor.selection.background.inactive" (Color/valueOf "#264A8B")]
     ["editor.selection.occurrence.outline" (if code-integration/use-new-code-editor? (Color/valueOf "#A2B0BE") Color/TRANSPARENT)]
     ["editor.gutter.foreground" (Color/valueOf "#A1B1BF")]
     ["editor.gutter.background" (Color/valueOf "#2C2E33")]]))

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
                                             :line-height-factor 1.2))
        tool-bar (setup-tool-bar! (.lookup console-grid-pane "#console-tool-bar") view-node)
        prompt (setup-prompt! (.lookup console-grid-pane "#debugger-prompt") view-node)
        repainter (ui/->timer "repaint-console-view" (fn [_ elapsed-time]
                                                       (when (.isSelected console-tab)
                                                         (repaint-console-view! view-node elapsed-time)
                                                         (refresh-tool-bar! view-node tool-bar)
                                                         (refresh-prompt! view-node prompt))))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :tool-bar tool-bar
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (ui/add-child! canvas-pane canvas)
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))
    (ui/observe (.focusedProperty canvas) (fn [_ _ focused?] (g/set-property! view-node :focused? focused?)))

    ;; Highlight occurrences of search term.
    (ui/observe find-term-property (fn [_ _ find-term] (g/set-property! view-node :highlighted-find-term find-term)))

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

    ;; Start repaint timer.
    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! console-tab repainter)
    view-node))
