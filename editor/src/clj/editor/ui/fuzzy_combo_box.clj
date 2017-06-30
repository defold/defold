(ns editor.ui.fuzzy-combo-box
  (:require [clojure.core.reducers :as r]
            [clojure.string :as string]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.ui :as ui]
            [internal.util :as util])
  (:import (com.defold.control ListView)
           (com.sun.javafx.util Utils)
           (javafx.beans.property StringProperty)
           (javafx.beans.value ObservableValue)
           (javafx.event Event)
           (javafx.geometry HPos Point2D VPos)
           (javafx.scene AccessibleAttribute Parent)
           (javafx.scene.control Button PopupControl Skin TextField)
           (javafx.scene.input KeyCode MouseEvent)
           (javafx.scene.layout ColumnConstraints GridPane Priority)
           (javafx.scene.text TextFlow Text)
           (javafx.stage WindowEvent)))

(set! *warn-on-reflection* true)

(def ^{:private true :const true} all-available 5000)

(defn- filter-field ^TextField [^Parent container] (.lookup container "#filter-field"))
#_(defn- foldout-button ^Button [^Parent container] (.lookup container "#foldout-button"))
#_(defn- choices-popup ^PopupControl [container] (ui/user-data container ::choices-popup))
(defn- choices-list-view ^ListView [container] (ui/user-data container ::choices-list-view))
(defn- selected-value-atom [container] (ui/user-data container ::selected-value-atom))

(def ^:private option->text second)
(def ^:private option->value first)

(defn- pref-popup-position ^Point2D [^Parent container]
  (Utils/pointRelativeTo container (choices-list-view container) HPos/CENTER VPos/BOTTOM 0.0 0.0 true))

(defn- option->fuzzy-matched-option [pattern option]
  (when-some [[score matching-indices] (fuzzy-text/match pattern (option->text option))]
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

(defn- make-choices-list-view ^ListView [^StringProperty filter-text-property options]
  (let [list-view (doto (ListView.) (.setId "choices-list-view"))]
    (ui/add-style! list-view "flat-list-view")
    (ui/cell-factory! list-view make-choices-list-view-cell)
    (ui/items! list-view options)
    (ui/observe filter-text-property
                (fn [_ _ ^String filter-text]
                  (let [filtered-items (fuzzy-option-filter-fn filter-text options)]
                    (ui/items! list-view filtered-items)
                    (when (seq filtered-items)
                      (ui/select-index! list-view 0)))))
    list-view))

(defn- make-choices-popup ^PopupControl [^GridPane container ^ListView choices-list-view]
  (let [popup (proxy [PopupControl] []
                (getStyleableParent [] container))
        popup-skin (reify Skin
                     (getSkinnable [_] container)
                     (getNode [_] choices-list-view)
                     (dispose [_]))]
    #_(ui/add-style! popup "combo-box-popup")
    (doto popup
      (.setSkin popup-skin)
      (.setConsumeAutoHidingEvents true)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true)
      #_(.setOnAutoHide (ui/event-handler _ (.onAutoHide (.getBehavior popup))))
      #_(.addEventHandler MouseEvent/MOUSE_CLICKED (ui/event-handler _ (.onAutoHide (.getBehavior popup)))))

    #_(let [layout-pos-listener (ui/invalidation-listener _
                                ())]
      (.addListener (.layoutXProperty container) layout-pos-listener)
      (.addListener (.layoutYProperty container) layout-pos-listener)
      (.addListener (.widthProperty container) layout-pos-listener)
      (.addListener (.heightProperty container) layout-pos-listener))

    popup))

(defn- ^Button make-foldout-button []
  (let [foldout-button (doto (Button.)
                         (.setId "foldout-button")
                         (ui/add-style! "button-small")
                         (ui/add-style! "arrow-button"))]
    foldout-button))

(defn make ^Parent [options]
  (let [container (GridPane.)
        filter-field (doto (TextField.) (.setId "filter-field"))
        foldout-button (make-foldout-button)
        selected-value-atom (atom nil)
        choices-list-view (make-choices-list-view (.textProperty filter-field) options)
        choices-popup (make-choices-popup container choices-list-view)
        reject! (fn []
                  (.hide choices-popup))
        accept! (fn []
                  (reset! selected-value-atom (option->value (first (ui/selection choices-list-view))))
                  (.hide choices-popup))]

    (ui/add-style! container "composite-property-control-container")
    (ui/user-data! container ::choices-list-view choices-list-view)
    (ui/user-data! container ::choices-popup choices-popup)
    (ui/user-data! container ::selected-value-atom selected-value-atom)
    (ui/on-closed! choices-popup (fn [_] (.notifyAccessibleAttributeChanged container AccessibleAttribute/FOCUS_NODE)))

    ;; Open popup foldout button is clicked.
    (ui/on-action! foldout-button (fn [^Event event]
                                    (.consume event)
                                    (if (.isShowing choices-popup)
                                      (.hide choices-popup)
                                      (let [anchor (pref-popup-position container)
                                            selected-value @selected-value-atom]
                                        (if-some [selected-option-index (util/first-index-where #(= selected-value (option->value %)) options)]
                                          (ui/select-index! choices-list-view selected-option-index)
                                          (ui/select! choices-list-view nil))
                                        (.show choices-popup container (.getX anchor) (.getY anchor))))))



    (ui/on-key! choices-list-view (fn [key]
                                    (condp = key
                                      KeyCode/ENTER
                                      (accept!)

                                      KeyCode/ESCAPE
                                      (reject!)

                                      nil)))

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
             ::selected-value-watch
             (fn [_key _reference old-state new-state]
               (listen-fn old-state new-state))))

(defn set-value! [container value]
  (reset! (selected-value-atom container) value))
