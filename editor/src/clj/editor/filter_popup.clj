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

(ns editor.filter-popup
  "Shared exclude-patterns filter popup, used by the Open Assets dialog
  (editor.resource-dialog) and the Search in Files dialog
  (editor.search-results-view) so their filtering UI/UX and event handling
  stay identical. Each caller is responsible for the button that opens the
  popup (a cljfx-rendered button for Open Assets, a wrapped legacy FXML
  button for Search in Files) — this namespace only owns the popup content,
  its event handling, and the shared badge/anchor calculations."
  (:require [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.popup :as fx.popup]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.separator :as fx.separator]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [util.coll :as coll])
  (:import [javafx.geometry Point2D]
           [javafx.scene Node]))

(set! *warn-on-reflection* true)

(defn button-graphic [open active-filter-count]
  (let [show-counter (pos? active-filter-count)]
    {:fx/type fx.h-box/lifecycle
     :style-class "filter-popup-button-content"
     :children [{:fx/type fx.label/lifecycle
                 :visible show-counter
                 :managed show-counter
                 :style-class "filter-popup-button-counter"
                 :text (str active-filter-count)}
                {:fx/type fx.region/lifecycle
                 :pseudo-classes (if open #{:open} #{})
                 :h-box/margin {:left 10}
                 :style-class "filter-popup-button-arrow"}]}))

(defn badge-count
  "Number of active exclusions represented in state — used for the filter
  button's counter badge. Enabled patterns only count while
  :filter-popup-filtering-enabled is true, matching when they're actually
  applied (see editor.resource-dialog/editor.search-results-view)."
  [state]
  (+ (if (:filter-popup-filtering-enabled state)
       (count (filter second (:exclude-patterns state)))
       0)
     (count (filter #(get state (:key %)) resource/exclude-filters))))

(defn ^Point2D anchor-point
  "Returns a Point2D suitable for the popup's :anchor-x/:anchor-y, or nil if
  node is nil or not yet attached to a Scene."
  [^Node node]
  (when (and node (some? (.getScene node)))
    (.localToScreen node -12.0 (- (.getMaxY (.getBoundsInLocal node)) 4.0))))

(defn list-cell-view [toggle-event-type remove-event-type [i [pattern selected] :as item]]
  (if-not item
    {}
    {:style-class "filter-popup-list-cell"
     :pref-width 100
     :graphic {:fx/type fx.h-box/lifecycle
               :style-class "filter-popup-list-cell-h-box"
               :alignment :center-left
               :spacing 2
               :children [{:fx/type fx.check-box/lifecycle
                           :h-box/hgrow :always
                           :focus-traversable false
                           :max-width ##Inf
                           :selected (boolean selected)
                           :mnemonic-parsing false
                           :on-selected-changed {:event-type toggle-event-type :index i}
                           :text pattern}
                          {:fx/type fx.button/lifecycle
                           :focus-traversable false
                           :style-class "filter-popup-list-cell-remove-button"
                           :graphic {:fx/type fx.region/lifecycle
                                     :style-class "cross"}
                           :on-action {:event-type remove-event-type :index i}}]}}))

(defn popup-desc
  "Returns the :popup fx description (see editor.fxui/with-popup-window) for
  the shared exclude-patterns filter popup.

  state must contain :filter-popup-open, :filter-popup-text,
  :filter-popup-filtering-enabled, :exclude-patterns, and one entry per
  editor.resource/exclude-filters entry's :key. localization is the dialog's
  localization value, used to resolve message patterns. anchor is the
  Point2D from anchor-point, or nil."
  [{:keys [filter-popup-open filter-popup-text filter-popup-filtering-enabled
           exclude-patterns]
   :as state}
   localization
   ^Point2D anchor]
  {:fx/type fx.popup/lifecycle
   :showing (boolean (and filter-popup-open anchor))
   :anchor-location :window-top-left
   :anchor-x (if anchor (.getX anchor) 0.0)
   :anchor-y (if anchor (.getY anchor) 0.0)
   :auto-hide true
   :auto-fix true
   :hide-on-escape true
   :consume-auto-hiding-events true
   :on-auto-hide {:event-type :filter-popup/hide}
   :content [{:fx/type fx.stack-pane/lifecycle
              :stylesheets [(str (io/resource "editor.css"))]
              :style-class "filter-popup"
              :children [{:fx/type fx.region/lifecycle
                          :mouse-transparent true
                          :style-class "filter-popup-background"}
                         {:fx/type fx.v-box/lifecycle
                          :children
                          (-> [{:fx/type fx.label/lifecycle
                                :v-box/margin {:left 4 :right 4 :top 4}
                                :style-class "grid-menu-group-label"
                                :text (localization (localization/message "dialog.open-assets.filter.section.exclude"))}]
                              (into (map (fn [{:keys [key label]}]
                                           {:fx/type fx.check-box/lifecycle
                                            :v-box/margin 4
                                            :focus-traversable false
                                            :max-width ##Inf
                                            :mnemonic-parsing false
                                            :selected (boolean (get state key))
                                            :on-selected-changed {:event-type :filter-popup/toggle-filter :key key}
                                            :text (localization (localization/message label))}))
                                    resource/exclude-filters)
                              (conj {:fx/type fx.separator/lifecycle
                                     :style-class "filter-popup-separator"}
                                    {:fx/type fx.label/lifecycle
                                     :v-box/margin {:left 4 :right 4 :top 4}
                                     :style-class "grid-menu-group-label"
                                     :text (localization (localization/message "dialog.open-assets.filter.section.patterns"))}
                                    {:fx/type fx.check-box/lifecycle
                                     :v-box/margin 4
                                     :focus-traversable false
                                     :max-width ##Inf
                                     :mnemonic-parsing false
                                     :selected (boolean filter-popup-filtering-enabled)
                                     :on-selected-changed {:event-type :filter-popup/toggle-filtering}
                                     :text (localization (localization/message "dialog.open-assets.filter.enable"))})
                              (cond-> (pos? (count exclude-patterns))
                                (conj {:fx/type fx.list-view/lifecycle
                                       :focus-traversable false
                                       :style-class "filter-popup-list-view"
                                       :items (into [] (map-indexed coll/pair) exclude-patterns)
                                       :fixed-cell-size 27
                                       :max-height (* 27 (min 10 (count exclude-patterns)))
                                       :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                                      :describe (partial list-cell-view
                                                                         :filter-popup/toggle
                                                                         :filter-popup/remove)}}))
                              (conj {:fx/type fx.text-field/lifecycle
                                     :v-box/margin 4
                                     :min-width 220
                                     :text filter-popup-text
                                     :prompt-text (localization (localization/message "dialog.open-assets.filter.prompt"))
                                     :on-text-changed {:event-type :filter-popup/set-text}
                                     :on-action {:event-type :filter-popup/add}}))}]}]})

(defn event-handler
  "Returns an event handler fn of (state event) for the shared exclude-patterns
  filter popup. state must use the same keys as popup-desc. Calls the given
  on-*-changed callbacks whenever the corresponding piece of state changes, so
  callers can persist to prefs and/or trigger a re-filter/re-search.
  on-filter-changed is called with (key new-value) whenever one of the
  editor.resource/exclude-filters toggles changes. Returns nil for events it
  does not handle."
  [on-patterns-changed on-filtering-changed on-filter-changed]
  (fn [state event]
    (case (:event-type event)
      :filter-popup/toggle-open (update state :filter-popup-open not)
      :filter-popup/hide        (assoc state :filter-popup-open false)
      :filter-popup/set-text    (assoc state :filter-popup-text (:fx/event event))
      :filter-popup/toggle-filtering
      (let [new-enabled (not (:filter-popup-filtering-enabled state))]
        (on-filtering-changed new-enabled)
        (assoc state :filter-popup-filtering-enabled new-enabled))
      :filter-popup/toggle-filter
      (let [k (:key event)
            new-value (not (get state k))]
        (on-filter-changed k new-value)
        (assoc state k new-value))
      :filter-popup/toggle
      (let [idx (:index event)
            patterns (vec (:exclude-patterns state))
            new-patterns (update patterns idx (fn [[p e]] [p (not e)]))]
        (on-patterns-changed new-patterns)
        (assoc state :exclude-patterns new-patterns :filter-popup-open true))
      :filter-popup/add
      (let [pattern (string/trim (:filter-popup-text state))
            patterns (:exclude-patterns state)]
        (cond
          (string/blank? pattern)
          state
          (some (fn [[p _]] (= p pattern)) patterns)
          (assoc state :filter-popup-text "")
          :else
          (let [new-patterns (conj (vec patterns) [pattern true])]
            (on-patterns-changed new-patterns)
            (assoc state :exclude-patterns new-patterns :filter-popup-text ""))))
      :filter-popup/remove
      (let [idx (:index event)
            patterns (vec (:exclude-patterns state))
            new-patterns (into (subvec patterns 0 idx) (subvec patterns (inc idx)))]
        (on-patterns-changed new-patterns)
        (assoc state :exclude-patterns new-patterns))
      nil)))
