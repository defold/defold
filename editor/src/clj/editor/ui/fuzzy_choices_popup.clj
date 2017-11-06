(ns editor.ui.fuzzy-choices-popup
  (:require [clojure.core.reducers :as r]
            [clojure.string :as string]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.ui :as ui])
  (:import (com.defold.control ListView)
           (javafx.css Styleable)
           (javafx.scene.text Text TextFlow)
           (javafx.scene.control PopupControl Skin)))

(set! *warn-on-reflection* true)

(def ^:private max-visible-item-count 10)

(defn- list-view-height
  ^double [^double cell-height ^long item-count]
  (+ 2.0 (* cell-height item-count)))

(defn update-list-view! [^ListView list-view width items selected-index]
  (let [item-count (count items)]
    (ui/items! list-view items)
    (ui/visible! list-view (pos? item-count))
    (let [min-width (max 50.0 width)
          pref-width (or (some-> (ui/max-list-view-cell-width list-view) (+ 10.0)) min-width)
          pref-height (list-view-height (.getFixedCellSize list-view) item-count)]
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

(defn- option->fuzzy-matched-option [option->text pattern option]
  (when-some [[score matching-indices] (fuzzy-text/match-path pattern (option->text option))]
    (with-meta option
               {:score score
                :matching-indices matching-indices})))

(defn- option-order [option->text a b]
  (let [score-comparison (compare (:score (meta b)) (:score (meta a)))]
    (if-not (zero? score-comparison)
      score-comparison
      (let [a-text (option->text a)
            b-text (option->text b)
            text-length-comparison (compare (count a-text) (count b-text))]
        (if-not (zero? text-length-comparison)
          text-length-comparison
          (let [text-comparison (compare a-text b-text)]
            (if-not (zero? text-comparison)
              text-comparison
              (if (and (instance? Comparable a) (instance? Comparable b))
                (compare a b)
                0))))))))

(defn fuzzy-option-filter-fn [option->matched-text option->label-text filter-text options]
  (if (empty? filter-text)
    options
    (sort (partial option-order option->label-text)
          (r/foldcat (r/filter some?
                               (r/map (partial option->fuzzy-matched-option option->matched-text filter-text)
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

(defn make-choices-list-view-cell [option->text option]
  (let [text (option->text option)
        matching-indices (:matching-indices (meta option))
        graphic (make-matched-list-item-graphic text matching-indices)]
    {:graphic graphic}))

(defn make-choices-list-view
  ^ListView [cell-height cell-factory]
  (doto (ListView.)
    (.setId "fuzzy-choices-list-view")
    (.setMinHeight (list-view-height cell-height 1)) ; Fixes IndexOutOfBoundsException in ListView
    (.setMaxHeight (list-view-height cell-height max-visible-item-count))
    (.setFixedCellSize cell-height) ; Fixes missing cells in VirtualFlow
    (ui/add-style! "flat-list-view")
    (ui/cell-factory! cell-factory)))

(defn make-choices-popup
  ^PopupControl [^Styleable owner ^ListView choices-list-view]
  (let [popup (proxy [PopupControl] []
                (getStyleableParent [] owner))
        *skinnable (atom popup)
        popup-skin (reify Skin
                     (getSkinnable [_] @*skinnable)
                     (getNode [_] choices-list-view)
                     (dispose [_] (reset! *skinnable nil)))]
    (doto popup
      (.setSkin popup-skin)
      (.setConsumeAutoHidingEvents true)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true))))
