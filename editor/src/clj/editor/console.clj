(ns editor.console
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.util :refer [split-lines]]
            [editor.code.view :as view]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [internal.util :as util])
  (:import (javafx.scene Cursor Node Parent)
           (javafx.scene.canvas Canvas)
           (javafx.scene.control Button CheckBox Tab TabPane TextArea TextField)
           (javafx.scene.input Clipboard ClipboardContent KeyCode KeyEvent MouseEvent ScrollEvent)
           (javafx.scene.layout ColumnConstraints GridPane Pane Priority)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleBooleanProperty SimpleStringProperty)))

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
;; Find Bar
;; -----------------------------------------------------------------------------

(defonce ^SimpleStringProperty find-term-property (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleBooleanProperty find-whole-word-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-case-sensitive-property (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-wrap-property (doto (SimpleBooleanProperty.) (.setValue false)))

(defn- setup-find-bar! [^GridPane find-bar view-node]
  (doto find-bar
    (ui/context! :console-find-bar {:find-bar find-bar :view-node view-node} nil)
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

(defn- focus-term-field! [^Parent bar]
  (ui/with-controls bar [^TextField term]
    (.requestFocus term)
    (.selectAll term)))

(defn- set-find-term! [^String term-text]
  (.setValue find-term-property (or term-text "")))

(handler/defhandler :find-text :console-view
  (run [find-bar view-node]
       (when-some [selected-text (view/non-empty-single-selection-text view-node)]
         (set-find-term! selected-text))
       (focus-term-field! find-bar)))

(handler/defhandler :find-next :console-view
  (run [view-node] (view/find-next! view-node)))

(handler/defhandler :find-next :console-find-bar
  (run [view-node] (view/find-next! view-node)))

(handler/defhandler :find-prev :console-view
  (run [view-node] (view/find-prev! view-node)))

(handler/defhandler :find-prev :console-find-bar
  (run [view-node] (view/find-prev! view-node)))

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

(handler/defhandler :select-next-occurrence :console-view
  (run [view-node] (view/select-next-occurrence! view-node)))

(handler/defhandler :select-next-occurrence :console-find-bar
  (run [view-node] (view/select-next-occurrence! view-node)))

(handler/defhandler :split-selection-into-lines :console-view
  (run [view-node] (view/split-selection-into-lines! view-node)))

;; -----------------------------------------------------------------------------
;; Setup
;; -----------------------------------------------------------------------------

(g/defnode ConsoleNode
  (property cursor-ranges r/CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows r/InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property lines r/Lines (default [""]) (dynamic visible (g/constantly false)))
  (property regions r/Regions (default []) (dynamic visible (g/constantly false))))

(defn- setup-view! [console-node view-node]
  (g/transact
    (concat
      (g/connect console-node :_node-id view-node :resource-node)
      (g/connect console-node :cursor-ranges view-node :cursor-ranges)
      (g/connect console-node :invalidated-rows view-node :invalidated-rows)
      (g/connect console-node :lines view-node :lines)
      (g/connect console-node :regions view-node :regions)))
  view-node)

(defn make-console! [graph ^Tab console-tab ^GridPane console-grid-pane]
  (let [canvas (Canvas.)
        canvas-pane (Pane. (into-array Node [canvas]))
        view-node (setup-view! (g/make-node! graph ConsoleNode)
                               (g/make-node! graph view/CodeEditorView
                                             :canvas canvas
                                             :highlighted-find-term (.getValue find-term-property)))
        find-bar (setup-find-bar! (ui/load-fxml "find.fxml") view-node)
        repainter (ui/->timer "repaint-console-view"
                              (fn [_ elapsed-time]
                                (when (.isSelected console-tab)
                                  (let [{:keys [clear? lines]} (flip-pending!)
                                        prev-lines (if clear? [""] (g/node-value view-node :lines))
                                        prev-regions (if clear? [] (g/node-value view-node :regions))
                                        prev-layout (g/node-value view-node :layout)]
                                    (view/set-properties! view-node nil
                                                          (data/append-distinct-lines prev-lines prev-regions prev-layout lines))
                                    (view/repaint-view! view-node elapsed-time)))))
        context-env {:clipboard (Clipboard/getSystemClipboard)
                     :find-bar find-bar
                     :view-node view-node}]

    ;; Canvas stretches to fit view, and updates properties in view node.
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))
    (ui/observe (.widthProperty canvas) (fn [_ _ width] (g/set-property! view-node :canvas-width width)))
    (ui/observe (.heightProperty canvas) (fn [_ _ height] (g/set-property! view-node :canvas-height height)))

    ;; Highlight occurrences of search term.
    (ui/observe find-term-property (fn [_ _ find-term] (g/set-property! view-node :highlighted-find-term find-term)))
    (ui/observe find-case-sensitive-property (fn [_ _ find-case-sensitive?] (g/set-property! view-node :find-case-sensitive? find-case-sensitive?)))
    (ui/observe find-whole-word-property (fn [_ _ find-whole-word?] (g/set-property! view-node :find-whole-word? find-whole-word?)))

    ;; Configure canvas.
    (doto canvas
      (.setFocusTraversable true)
      (.setCursor Cursor/TEXT)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (view/handle-key-pressed! view-node event)))
      #_(.addEventHandler KeyEvent/KEY_TYPED (ui/event-handler event (view/handle-key-typed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (view/handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (view/handle-mouse-pressed! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_DRAGGED (ui/event-handler event (view/handle-mouse-moved! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_RELEASED (ui/event-handler event (view/handle-mouse-released! view-node event)))
      (.addEventHandler ScrollEvent/SCROLL (ui/event-handler event (view/handle-scroll! view-node event))))

    (ui/context! console-grid-pane :console-grid-pane context-env nil)

    (doto (.getColumnConstraints console-grid-pane)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints canvas-pane 0 0)
    (GridPane/setVgrow canvas-pane Priority/ALWAYS)

    ;; Build view hierarchy.
    (ui/children! console-grid-pane [find-bar canvas-pane])
    (ui/fill-control console-grid-pane)
    (ui/context! canvas :console-view context-env nil)

    ;; Start repaint timer.
    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! console-tab repainter)
    view-node))
