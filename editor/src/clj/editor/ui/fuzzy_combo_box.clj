(ns editor.ui.fuzzy-combo-box
  (:require [clojure.core.reducers :as r]
            [clojure.string :as string]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.ui :as ui]
            [internal.util :as util])
  (:import (com.defold.control ListView)
           (com.sun.javafx.event DirectEvent)
           (com.sun.javafx.util Utils)
           (javafx.beans.property StringProperty)
           (javafx.beans.value ChangeListener ObservableValue)
           (javafx.event Event EventTarget)
           (javafx.geometry HPos Point2D VPos)
           (javafx.scene AccessibleAttribute Parent)
           (javafx.scene.control Button PopupControl Skin TextField)
           (javafx.scene.input KeyCode KeyEvent MouseEvent)
           (javafx.scene.layout ColumnConstraints GridPane Priority StackPane)
           (javafx.scene.text TextFlow Text)
           (javafx.stage WindowEvent)))

(set! *warn-on-reflection* true)

(def ^:private max-visible-item-count 10)
(def ^:private item-height 27.0)
(def ^:private all-available 5000)

(def ^:private option->text second)
(def ^:private option->value first)
(defn- selected-value-atom [container] (ui/user-data container ::selected-value-atom))

(defn- list-view-height
  ^double [^long item-count]
  (+ 2.0 (* item-height item-count)))

(defn- update-list-view! [^ListView list-view width items selected-index]
  (let [item-count (count items)]
    (ui/items! list-view items)
    (ui/visible! list-view (pos? item-count))
    (let [min-width (max 50.0 width)
          pref-width (or (some-> (ui/max-list-view-cell-width list-view) (+ 10.0)) min-width)
          pref-height (list-view-height item-count)]
      (.setMinWidth list-view min-width)
      (.setPrefWidth list-view pref-width)
      (.setPrefHeight list-view pref-height)
      (if (some? selected-index)
        (do (ui/select-index! list-view selected-index)
            (when (< max-visible-item-count item-count)
              (.scrollTo list-view ^int (int selected-index))))
        (do (ui/select! list-view nil)
            (when (< max-visible-item-count item-count)
              (.scrollTo list-view 0))))
      (let [max-width (.getMaxWidth list-view)
            max-height (.getMaxHeight list-view)
            min-height (.getMinHeight list-view)
            width (max min-width (min pref-width max-width))
            height (max min-height (min pref-height max-height))]
        [width height]))))

(defn- send-event! [^EventTarget event-target ^Event event]
  (Event/fireEvent event-target (DirectEvent. (.copyFor event event-target event-target))))

(defn- modifiers [^KeyEvent key-event]
  (cond-> #{}
          (.isAltDown key-event) (conj :alt)
          (.isShiftDown key-event) (conj :shift)
          (.isShortcutDown key-event) (conj :shortcut)))

(defn- pref-popup-position
  ^Point2D [^Parent container width height]
  (Utils/pointRelativeTo container width height HPos/CENTER VPos/BOTTOM 0.0 -1.0 true))

(defn- option->fuzzy-matched-option [pattern option]
  (when-some [[score matching-indices] (fuzzy-text/match-path pattern (option->text option))]
    (with-meta option
               {:score score
                :matching-indices matching-indices})))

(defn- descending-order [a b]
  (compare b a))

(defn- fuzzy-option-filter-fn [filter-text options]
  (if (empty? filter-text)
    options
    (sort-by (comp :score meta)
             descending-order
             (r/foldcat (r/filter some?
                                  (r/map (partial option->fuzzy-matched-option filter-text)
                                         options))))))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [text matching-indices]
  (let [/ (or (some-> text (string/last-index-of \/) inc) 0)]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run (subs text start /) "diminished")
                       (make-text-run (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run (subs text start end) "diminished")]

                      :else
                      [(make-text-run (subs text start end) nil)])))
          (fuzzy-text/runs (count text) matching-indices))))

(defn- make-matched-list-item-graphic [text matching-indices]
  (TextFlow. (into-array Text (matched-text-runs text matching-indices))))

(defn- make-choices-list-view-cell [option]
  (let [text (option->text option)
        matching-indices (:matching-indices (meta option))
        graphic (make-matched-list-item-graphic text matching-indices)]
    {:graphic graphic}))

(defn- make-choices-list-view
  ^ListView []
  (doto (ListView.)
    (.setId "choices-list-view")
    (.setMinHeight 30.0) ; Fixes IndexOutOfBoundsException in ListView
    (.setMaxHeight (list-view-height max-visible-item-count))
    (.setFixedCellSize item-height) ; Fixes missing cells in VirtualFlow
    (ui/add-style! "flat-list-view")
    (ui/cell-factory! make-choices-list-view-cell)))

(defn- make-choices-popup
  ^PopupControl [^GridPane container ^ListView choices-list-view]
  (let [popup (proxy [PopupControl] []
                (getStyleableParent [] container))
        popup-skin (reify Skin
                     (getSkinnable [_] container)
                     (getNode [_] choices-list-view)
                     (dispose [_]))]
    (doto popup
      (.setSkin popup-skin)
      (.setConsumeAutoHidingEvents true)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true))))

(defn- make-foldout-button
  ^Button []
  (doto (Button.)
    (.setId "foldout-button")
    (.setFocusTraversable false)
    (ui/add-style! "arrow-button")
    (ui/add-style! "button-small")
    (.setGraphic (doto (StackPane.)
                   (ui/add-style! "arrow")))))

(defn- make-filter-field
  ^TextField []
  (doto (TextField.)
    (.setId "filter-field")))

(defn make
  ^Parent [options]
  (let [selected-value-atom (atom nil)
        container (GridPane.)
        choices-list-view (make-choices-list-view)
        choices-popup (make-choices-popup container choices-list-view)
        foldout-button (make-foldout-button)
        filter-field (make-filter-field)
        filter-change-listener (reify ChangeListener
                                 (changed [_ _ _ filter-text]
                                   (let [filtered-options (fuzzy-option-filter-fn filter-text options)
                                         any-options? (boolean (seq filtered-options))
                                         selected-index (when any-options? 0)]
                                     (update-list-view! choices-list-view (.getWidth container) filtered-options selected-index)
                                     (if any-options?
                                       (ui/remove-style! filter-field "unmatched")
                                       (ui/add-style! filter-field "unmatched")))))
        selected-option (fn [] (first (ui/selection choices-list-view)))
        refresh-filter-field! (fn [selected-value]
                                (let [selected-option (util/first-where #(= selected-value (option->value %)) options)
                                      text (or (option->text selected-option) (str selected-value))]
                                  (ui/remove-style! filter-field "unmatched")
                                  (ui/text! filter-field text)))
        show! (fn show! []
                (when-not (.isShowing choices-popup)
                  (.addListener (.textProperty filter-field) filter-change-listener)
                  (.setPromptText filter-field "Type to filter")
                  (ui/add-style! container "showing")
                  (let [selected-value @selected-value-atom
                        selected-option-index (util/first-index-where #(= selected-value (option->value %)) options)
                        [width height] (update-list-view! choices-list-view (.getWidth container) options selected-option-index)
                        anchor (pref-popup-position container width height)]
                    (.show choices-popup container (.getX anchor) (.getY anchor)))
                  (ui/run-later
                    (when (and (ui/focus? filter-field)
                               (not (empty? (ui/text filter-field))))
                      (.selectAll filter-field)))))
        hide! (fn hide! [] (.hide choices-popup))
        reject! (fn reject! [] (hide!))
        accept! (fn accept! [option]
                  (hide!)
                  (when (some? option)
                    (reset! selected-value-atom (option->value option))))]

    ;; Configure container.
    (ui/add-style! container "fuzzy-combo-box")
    (ui/add-style! container "composite-property-control-container")
    (ui/user-data! container ::choices-popup choices-popup)
    (ui/user-data! container ::selected-value-atom selected-value-atom)

    ;; When the filter field gains focus, open the popup.
    (ui/on-focus! filter-field (fn [got-focus?]
                                 (if got-focus?
                                   (show!)
                                   (hide!))))

    ;; When the foldout button is clicked, focus on the filter field.
    (ui/on-action! foldout-button (fn [^Event event]
                                    (.consume event)
                                    (if (.isShowing choices-popup)
                                      (hide!)
                                      (if (ui/focus? filter-field)
                                        (show!)
                                        (ui/request-focus! filter-field)))))

    ;; Handle key events.
    ;; The choices list view receives all key events while the popup is open.
    ;; Note that we redirect some key events to the filter field.
    (.addEventFilter choices-list-view
                     KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [^KeyEvent event event]
                                         (condp = (.getCode event)
                                           KeyCode/ENTER  (do (.consume event) (accept! (selected-option)))
                                           KeyCode/ESCAPE (do (.consume event) (reject!))
                                           KeyCode/HOME   (do (.consume event) (send-event! filter-field event))
                                           KeyCode/END    (do (.consume event) (send-event! filter-field event))
                                           KeyCode/TAB    (do (.consume event) (accept! (selected-option)) (send-event! filter-field event))
                                           nil))))

    ;; Clicking an option in the popup changes the value.
    (.setOnMouseClicked choices-list-view
                        (ui/event-handler event
                                          (when-some [option (ui/cell-item-under-mouse event)]
                                            (accept! option))))

    ;; Update filter field from external changes.
    (add-watch selected-value-atom
               ::internal-selected-value-watch
               (fn [_ _ _ selected-value]
                 (when-not (.isShowing choices-popup)
                   (refresh-filter-field! selected-value))))

    ;; Clean up after the popup closes.
    (ui/on-closed! choices-popup (fn [_]
                                   (.removeListener (.textProperty filter-field) filter-change-listener)
                                   (.setPromptText filter-field nil)
                                   (refresh-filter-field! @selected-value-atom)
                                   (ui/remove-style! container "showing")
                                   (ui/request-focus! container)))

    ;; Ensure popup is closed if the container is detached from the scene.
    (.addListener (.sceneProperty container)
                  (ui/invalidation-listener scene-property
                    (when (nil? (.getValue ^ObservableValue scene-property))
                      (.hide choices-popup))))

    ;; Layout children.
    (ui/children! container [filter-field foldout-button])
    (GridPane/setConstraints filter-field 0 0)
    (GridPane/setConstraints foldout-button 1 0)
    (doto (.getColumnConstraints container)
      (.add (doto (ColumnConstraints.) (.setHgrow Priority/ALWAYS) (.setPrefWidth all-available)))
      (.add (doto (ColumnConstraints.) (.setHgrow Priority/NEVER) (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF))))

    container))

(defn observe! [container listen-fn]
  (add-watch (selected-value-atom container)
             ::external-selected-value-watch
             (fn [_key _reference old-state new-state]
               (when (not= old-state new-state)
                 (listen-fn old-state new-state)))))

(defn set-value! [container value]
  (reset! (selected-value-atom container) value))
