;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.fxui.combo-box
  (:require [cljfx.api :as fx]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.popup :as fx.popup]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [clojure.java.io :as io]
            [editor.fxui :as fxui]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [java.util Collection]
           [javafx.event EventHandler]
           [javafx.geometry Orientation]
           [javafx.scene Node TraversalDirection]
           [javafx.scene.control ListView ScrollBar]
           [javafx.scene.control.skin DefoldComboBoxListViewSkin]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout Region]
           [javafx.stage Popup]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private ext-with-list-view-props
  (fx/make-ext-with-props fx.list-view/props))

(def ^:private prop-popup
  (fx/make-binding-prop
    (fn [^Node node ^Popup popup]
      (let [bounds (.localToScreen node (.getBoundsInLocal node))
            ^Region root (-> popup .getContent .getFirst)]
        (doto root
          (.setMinWidth (.getWidth bounds))
          (.setMaxWidth (.getWidth bounds)))
        (.show popup node (- (.getMinX bounds) 12.0) (- (.getMaxY bounds) 6.0))
        #(.hide popup)))
    fx.lifecycle/dynamic))

(defn- hide [state]
  (-> state
      (assoc :showing false :filter-text "")
      (dissoc :selected-item)))

(defn- show [state]
  (assoc state :showing true))

(defn- toggle [state]
  ((if (:showing state) hide show) state))

(defn- set-filter-text [state text]
  (assoc state :filter-text text))

(defn- set-selected-item [state item]
  (assoc state :selected-item item))

(defn- handle-mouse-pressed [swap-state ^MouseEvent e]
  (.requestFocus ^Node (.getSource e))
  (.consume e)
  (swap-state toggle))

(defn- handle-key-pressed [showing swap-state ^KeyEvent e]
  (when-not showing
    (let [key-code (.getCode e)]
      (cond
        (or (= KeyCode/ENTER key-code)
            (= KeyCode/SPACE key-code))
        (do
          (.consume e)
          (swap-state show))

        (= KeyCode/TAB key-code)
        (do (.consume e)
            (.requestFocusTraversal ^Node (.getSource e) (if (.isShiftDown e) TraversalDirection/PREVIOUS TraversalDirection/NEXT)))))))

(def ^:private prop-filter-key-pressed
  (fx/make-binding-prop
    (fn [^Node node callback]
      (let [^EventHandler handler callback]
        (.addEventFilter node KeyEvent/KEY_PRESSED handler)
        #(.removeEventFilter node KeyEvent/KEY_PRESSED handler)))
    fx.lifecycle/callback))

(defn- filter-popup-key-pressed [on-value-changed swap-state items item ^KeyEvent e]
  (let [key-code (.getCode e)]
    (if (= KeyCode/ESCAPE key-code)
      (do (.consume e)
          (swap-state hide))
      (when (pos? (count items))
        (cond
          (or (= KeyCode/PAGE_UP key-code)
              (= KeyCode/PAGE_DOWN key-code)
              (= KeyCode/UP key-code)
              (= KeyCode/DOWN key-code))
          (let [direction (if (or (= KeyCode/UP key-code) (= KeyCode/PAGE_UP key-code)) - +)
                magnitude (if (or (= KeyCode/PAGE_UP key-code) (= KeyCode/PAGE_DOWN key-code)) 10 1)
                item (-> (coll/index-of items item)
                         (direction magnitude)
                         long
                         (max 0)
                         (min (dec (count items)))
                         items)]
            (.consume e)
            (swap-state set-selected-item item))

          (= KeyCode/ENTER key-code)
          (do (.consume e)
              (swap-state hide)
              (when on-value-changed (on-value-changed item))))))))

(defn- describe-list-cell [on-value-changed swap-state to-string filtered-item->matching-indices item]
  (when (some? item)
    {:graphic (fuzzy-choices/make-matched-text-flow-cljfx
                (to-string item)
                (filtered-item->matching-indices item))
     :on-mouse-pressed (fn [_]
                         (swap-state hide)
                         (when on-value-changed (on-value-changed item)))}))

(def ^:private prop-list-items+selected-item
  (fx/make-prop
    (fx.mutator/setter
      (fn [^ListView list-view [items selected-item]]
        (.setAll (.getItems list-view) ^Collection items)
        (when (some? selected-item)
          (let [selection-model (.getSelectionModel list-view)]
            (.select selection-model selected-item)
            (fx/run-later
              (-> list-view
                  ^DefoldComboBoxListViewSkin (.getSkin)
                  .getVirtualFlowInstance
                  (.scrollTo (.getSelectedIndex selection-model))))))))
    fx.lifecycle/scalar))

(defn- create-list-view []
  (let [view (ListView.)
        skin (DefoldComboBoxListViewSkin. view)]
    (.setSkin view skin)
    (run!
      (fn [^ScrollBar bar]
        (when (= Orientation/HORIZONTAL (.getOrientation bar))
          (.setVisible bar false)
          (.setMaxHeight bar 0.0)))
      (.lookupAll (.getVirtualFlowInstance skin) ".scroll-bar"))
    view))

(defn- impl
  [{:keys [value on-value-changed items to-string state swap-state filter-prompt-text no-items-text not-found-text color]
    :or {to-string str
         filter-prompt-text "Type to filter"
         no-items-text "No items available"
         not-found-text "No items found"}
    :as props}]
  (let [{:keys [showing filter-text selected-item]} state]
    (-> props
        (dissoc :value :on-value-changed :items :to-string :state :swap-state
                :filter-prompt-text :no-items-text :not-found-text)
        (assoc
          :fx/type fxui/horizontal
          :style-class "ext-combo-box"
          :pseudo-classes (if showing #{:showing} #{})
          :on-focused-changed (fn [_] (swap-state hide))
          :on-mouse-pressed #(handle-mouse-pressed swap-state %)
          :on-key-pressed #(handle-key-pressed showing swap-state %)
          :focus-traversable true
          :children [{:fx/type fx.label/lifecycle
                      :h-box/hgrow :always
                      :text (if (some? value) (to-string value) "")}
                     {:fx/type fx.stack-pane/lifecycle
                      :style-class "ext-combo-box-arrow-button"
                      :children [{:fx/type fx.region/lifecycle
                                  :style-class "ext-combo-box-arrow"}]}]
          fxui/prop-custom-tooltip-node-showing showing)
        fxui/resolve-input-color
        (cond->
          showing
          (assoc
            prop-popup
            (let [option->text #(to-string (% 0))
                  filtered-item+matching-indices (->> items
                                                      (mapv vector)
                                                      (fuzzy-choices/filter-options option->text option->text filter-text)
                                                      (e/map #(coll/pair (% 0) (:matching-indices (meta %))))
                                                      vec)
                  filtered-items (mapv key filtered-item+matching-indices)
                  filtered-item->matching-indices (into {} filtered-item+matching-indices)
                  ;; If no item is selected, default to value
                  selected-item (if (some? selected-item) selected-item value)
                  ;; selected item has to be in items (but value doesn't have to be!)
                  selected-item (when (some? selected-item) (coll/first-where #(= selected-item %) filtered-items))
                  ;; if selected value is not in items, default to first item (items can
                  ;; be empty though, so selected item may still be nil)
                  selected-item (if (nil? selected-item) (first filtered-items) selected-item)
                  show-text-field (< 5 (count items))]
              {:fx/type fx.popup/lifecycle
               :auto-hide true
               :on-hidden (fn [_] (swap-state hide))
               :content
               [{:fx/type fx.stack-pane/lifecycle
                 :stylesheets [(str (io/resource "dialogs.css"))]
                 :children
                 [{:fx/type fx.region/lifecycle
                   :pseudo-classes (if color #{color} #{})
                   :on-mouse-pressed (fn [_] (swap-state hide))
                   :style-class "ext-combo-box-popup-background"}
                  {:fx/type fxui/vertical
                   :padding 1.0
                   prop-filter-key-pressed #(filter-popup-key-pressed on-value-changed swap-state filtered-items selected-item %)
                   :children
                   (-> []
                       (cond-> show-text-field
                               (conj {:fx/type fxui/text-field
                                      :style-class "ext-combo-box-popup-field"
                                      :prompt-text filter-prompt-text
                                      :text filter-text
                                      :on-text-changed #(swap-state set-filter-text %)}))
                       (conj (if (zero? (count filtered-items))
                               {:fx/type fxui/label
                                :alignment :center
                                :color :hint
                                :text (if (pos? (count items)) not-found-text no-items-text)}
                               {:fx/type ext-with-list-view-props
                                :desc {:fx/type fx/ext-instance-factory
                                       :create create-list-view}
                                :props {:focus-traversable (not show-text-field)
                                        :style-class "ext-combo-box-popup-list"
                                        prop-list-items+selected-item [filtered-items selected-item]
                                        :fixed-cell-size 27.0
                                        :max-height (* 27.0 (double (min 10 (count filtered-items))))
                                        :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                                       :describe (fn/partial describe-list-cell on-value-changed swap-state to-string filtered-item->matching-indices)}}})))}]}]}))))))

(defn view
  "Combo box control

  Supports all :horizontal props, plus:
    :value                 current value, doesn't have to be present in items
    :on-value-changed      callback that will receive a new value on change
    :items                 available items
    :to-string             a function that stringifies an item, default str
    :filter-prompt-text    defaults to \"Type to filter\"
    :no-items-text         defaults to \"No items available\"
    :not-found-text        defaults to \"No items found\"
    :color                 either :warning or :error"
  [props]
  {:fx/type fx/ext-state
   :initial-state {:showing false :filter-text ""}
   :desc (assoc props :fx/type impl)})