(ns editor.ui.fuzzy-combo-box
  (:require [editor.ui :as ui]
            [editor.ui.fuzzy-choices-popup :as popup]
            [internal.util :as util])
  (:import (com.sun.javafx.util Utils)
           (javafx.beans.value ChangeListener ObservableValue)
           (javafx.event Event)
           (javafx.geometry HPos Point2D VPos)
           (javafx.scene Parent)
           (javafx.scene.control Button TextField)
           (javafx.scene.input KeyCode KeyEvent)
           (javafx.scene.layout ColumnConstraints GridPane Priority StackPane)
           (javafx.geometry Point2D HPos VPos)))

(set! *warn-on-reflection* true)

(def ^:private item-height 27.0)
(def ^:private all-available 5000)

(def option->text second)
(def option->value first)

(defn- selected-value-atom [container]
  (ui/user-data container ::selected-value-atom))

(defn- pref-popup-position
  ^Point2D [^Parent container width height]
  (Utils/pointRelativeTo container width height HPos/CENTER VPos/BOTTOM 0.0 -1.0 true))

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
        choices-list-view (popup/make-choices-list-view item-height (partial popup/make-choices-list-view-cell option->text))
        choices-popup (popup/make-choices-popup container choices-list-view)
        foldout-button (make-foldout-button)
        filter-field (make-filter-field)
        filter-change-listener (reify ChangeListener
                                 (changed [_ _ _ filter-text]
                                   (let [filtered-options (popup/fuzzy-option-filter-fn option->text option->text filter-text options)
                                         any-options? (boolean (seq filtered-options))
                                         selected-index (when any-options? 0)]
                                     (popup/update-list-view! choices-list-view (.getWidth container) filtered-options selected-index)
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
                        [width height] (popup/update-list-view! choices-list-view (.getWidth container) options selected-option-index)
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
                                           KeyCode/HOME   (do (.consume event) (ui/send-event! filter-field event))
                                           KeyCode/END    (do (.consume event) (ui/send-event! filter-field event))
                                           KeyCode/TAB    (do (.consume event) (accept! (selected-option)) (ui/send-event! filter-field event))
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

(defn value [container]
  @(selected-value-atom container))
