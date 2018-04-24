(ns editor.ui.fuzzy-choices-popup
  (:require [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices])
  (:import (com.defold.control ListView)
           (javafx.css Styleable)
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

(def fuzzy-option-filter-fn (partial fuzzy-choices/filter-options fuzzy-choices/no-bonus))

(defn make-choices-list-view-cell [option->text option]
  (let [text (option->text option)
        matching-indices (fuzzy-choices/matching-indices option)
        graphic (fuzzy-choices/make-matched-text-flow text matching-indices)]
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
