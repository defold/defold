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

(ns editor.panes
  (:require [cljfx.api :as fx]
            [cljfx.fx.split-pane :as fx.split-pane]
            [cljfx.fx.tab :as fx.tab]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [editor.fxui :as fxui]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [java.util List]
           [javafx.event Event]
           [javafx.geometry Orientation]
           [javafx.scene.control SplitPane Tab TabPane]))

(set! *warn-on-reflection* true)

(def locations
  {:left-top {:region :left :scope :global}
   :left-bottom {:region :left :scope :global}
   :right-top {:region :right :scope :workbench}
   :right-bottom {:region :right :scope :workbench}
   :bottom {:region :bottom :scope :workbench}})

(defn- select-pane! [prefs location id]
  (when (and id (not= id (get (prefs/get prefs [:window :layout :selected-tabs]) location)))
    (prefs/update! prefs [:window :layout :selected-tabs] assoc location id)))

(ui/defc pane
  {:compose [{:fx/type fxui/ext-with-instances
              :inputs {:content (:content-desc props)}}]}
  [{:keys [id title location instances localization-state prefs]}]
  (when-let [content (:content instances)]
    {:fx/type fx.tab/lifecycle
     :style-class ["tab" (str (name id) "-tab")]
     :text (localization-state title)
     :closable false
     :on-selection-changed (fn pane-selection-changed [^Event event]
                             (when (.isSelected ^Tab (.getSource event))
                               (select-pane! prefs location id)))
     :content {:fx/type ui/ext-value :value content}}))

(def ^:private ext-with-split-pane-props
  (fx/make-ext-with-props fx.split-pane/props))

(def ^:private prop-tab-selection
  (fx/make-prop
    (fx.mutator/setter
      (fn apply-tab-selection! [^TabPane host {:keys [ids selected-id]}]
        (if selected-id
          (.select (.getSelectionModel host) (int (.indexOf ^List ids selected-id)))
          (.clearSelection (.getSelectionModel host)))))
    fx.lifecycle/scalar))

(def ^:private prop-region-visible
  (fx/make-prop
    (fx.mutator/setter
      (fn apply-region-visibility! [^SplitPane split {:keys [index content visible]}]
        (let [items (.getItems split)
              present (.contains items content)]
          (when (not= present (boolean visible))
            (let [divider-index (max 0 (dec index))

                  length
                  (condp = (.getOrientation split)
                    Orientation/HORIZONTAL (.getWidth split)
                    Orientation/VERTICAL (.getHeight split))

                  focus-owner (some-> split .getScene .getFocusOwner)

                  removing-focus-owner
                  (and present focus-owner (ui/closest-node-where (partial identical? content) focus-owner))]
              (if visible
                (let [fraction (/ (ui/user-data split ::hidden-size) length)

                      position
                      (max 0.0 (min 1.0 (if (zero? index) fraction (- 1.0 fraction))))]
                  (.add items (int index) content)
                  (.setDividerPosition split divider-index position))
                (let [position (get (.getDividerPositions split) divider-index)

                      size
                      (if (zero? index)
                        (Math/floor (* position length))
                        (Math/ceil (* (- 1.0 position) length)))]
                  (ui/user-data! split ::hidden-size size)
                  (.remove items content)))
              (.layout split)
              ;; If this action causes the focus owner to be removed from the
              ;; scene, move focus to the SplitPane. This ensures we have a
              ;; valid UI context when refreshing the menus.
              (when removing-focus-owner
                (.requestFocus split)))))))
    fx.lifecycle/scalar))

(defn visible-by-location [panes instances hidden]
  (reduce (fn [acc {:keys [id location]}]
            (if (and (get instances id) (not (contains? hidden id)))
              (update acc location (fnil conj []) id)
              acc))
          {}
          panes))

(defn- location-desc [[location _] {:keys [visible instances prefs prefs-state]}]
  (let [ids (get visible location [])

        saved-id
        (get (prefs/get prefs-state prefs [:window :layout :selected-tabs]) location)

        selected-id
        (if (.contains ^List ids saved-id) saved-id (first ids))]
    {:fx/type fxui/ext-with-extra-props
     :desc {:fx/type fxui/tab-pane
            :style-class ["tab-pane" "pane-tabs"]
            :min-width 170
            :min-height 100
            :tab-closing-policy :unavailable
            :tabs (mapv (fn [id] {:fx/type ui/ext-value :value (get instances id)}) ids)}
     :props {prop-tab-selection {:ids ids :selected-id selected-id}}}))

(ui/defc layout
  {:compose [{:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}
             {:fx/type fxui/ext-with-instances
              :inputs (coll/pair-map-by :id
                                        #(assoc % :fx/type pane
                                                :localization-state (:localization-state props)
                                                :prefs (:prefs props))
                                        (:panes props))}
             {:fx/type fx/ext-watcher :ref prefs/global-state :key :prefs-state}
             {:fx/type ui/ext-memo
              :fn visible-by-location
              :args [(:panes props) (:instances props) (:hidden props)]
              :key :visible}
             {:fx/type fxui/ext-with-instances
              :inputs (coll/pair-map-by key #(location-desc % props) locations)
              :key :location-instances}]}
  [{:keys [hidden-regions prefs prefs-state assets-split right-split bottom-container regions visible location-instances]}]
  {:fx/type fx/ext-many
   :desc (vec
           (e/concat
             [{:fx/type fxui/ext-with-anchor-pane-props
               :desc {:fx/type ui/ext-value :value bottom-container}
               :props {:children [{:fx/type ui/ext-value
                                   :value (:bottom location-instances)
                                   :anchor-pane/top 0
                                   :anchor-pane/right 0
                                   :anchor-pane/bottom 0
                                   :anchor-pane/left 0}]}}]
             (e/map
               (fn [[split id default places]]
                 (let [items
                       (coll/into-> places []
                         (keep (fn [location]
                                 (when (contains? visible location)
                                   {:fx/type ui/ext-value :fx/key location :value (get location-instances location)}))))]
                   {:fx/type ext-with-split-pane-props
                    :desc {:fx/type ext-with-split-pane-props
                           :desc {:fx/type ui/ext-value :value split}
                           :props {:items items}}
                    :props {:divider-positions (if (= 2 (count items))
                                                 (get (prefs/get prefs-state prefs [:window :split-positions]) id [default])
                                                 [])}}))
               [[assets-split :assets-split 0.575 [:left-top :left-bottom]]
                [right-split :right-split 0.4 [:right-top :right-bottom]]])
             (e/map
               (fn [[region definition]]
                 {:fx/type ext-with-split-pane-props
                  :desc {:fx/type ui/ext-value :value (:split definition)}
                  :props {prop-region-visible
                          (assoc (dissoc definition :split)
                            :visible (and (not (contains? hidden-regions region))
                                          (coll/any? #(= region (get-in locations [% :region])) (coll/keys visible))))}})
               regions)))})
