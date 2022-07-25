;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.ui.select-list-dialog
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.prop :as fx.prop]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.fxui :as fxui]
            [editor.resource :as resource]
            [editor.ui.fuzzy-choices :as fuzzy-choices])
  (:import [javafx.collections ListChangeListener]
           [javafx.event Event]
           [javafx.scene.control ListView TextField]
           [javafx.scene.input KeyCode KeyEvent MouseEvent MouseButton]))


(defn- dialog-stage
  "Dialog `:stage` that manages scene graph itself and provides layout common
  for many dialogs.

  Required keys:

    :header    cljfx description, header of a dialog, already padded
    :footer    cljfx description, footer of a dialog, already padded

  Optional keys:

    :size          dialog width, either :small, :default or :large
    :content       a content of a dialog, not padded; you can use
                   \"dialog-content-padding\" style class to set desired padding
                   (or \"text-area-with-dialog-content-padding\" for text areas)
    :root-props    extra props to scene root's v-box lifecycle (sans :children)"
  [{:keys [size header content footer root-props]
    :or {size :default root-props {}}
    :as props}]
  (-> props
      (dissoc :size :header :content :footer :root-props)
      (assoc :fx/type fxui/dialog-stage
             :scene {:fx/type fx.scene/lifecycle
                     :stylesheets [(str (io/resource "dialogs.css"))]
                     :root (-> root-props
                               (fxui/add-style-classes
                                 "dialog-body"
                                 (case size
                                   :small "dialog-body-small"
                                   :default "dialog-body-default"
                                   :large "dialog-body-large"))
                               (assoc :fx/type fx.v-box/lifecycle
                                      :children (if (some? content)
                                                  [{:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-with-content-header"
                                                    :children [header]}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-content"
                                                    :children [content]}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-with-content-footer"
                                                    :children [footer]}]
                                                  [{:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-without-content-header"
                                                    :children [header]}
                                                   {:fx/type fx.region/lifecycle :style-class "dialog-no-content"}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-without-content-footer"
                                                    :children [footer]}])))})))

(defn- dialog-buttons [props]
  (-> props
      (assoc :fx/type fx.h-box/lifecycle)
      (fxui/provide-defaults :alignment :center-right)
      (fxui/add-style-classes "spacing-smaller")))

(defn- default-filter-fn [filter-on text items]
  (let [text (string/lower-case text)]
    (filterv (fn [item]
               (string/starts-with? (string/lower-case (filter-on item)) text))
             items)))

(def ext-with-identity-items-props
  (fx/make-ext-with-props
    {:items (fx.prop/make (fxui/identity-aware-observable-list-mutator #(.getItems ^ListView %))
                          fx.lifecycle/scalar)}))

(defn- select-first-list-item-on-items-changed! [^ListView view]
  (let [items (.getItems view)
        selection-model (.getSelectionModel view)]
    (when (pos? (count items))
      (.select selection-model 0))
    (.addListener items (reify ListChangeListener
                          (onChanged [_ _]
                            (when (pos? (count items))
                              (.select selection-model 0)))))))

(defn- select-list-dialog
  [{:keys [filter-term filtered-items title ok-label prompt cell-fn selection]
    :as props}]
  {:fx/type dialog-stage
   :title title
   :showing (fxui/dialog-showing? props)
   :on-close-request {:event-type :cancel}
   :size :large
   :header {:fx/type fxui/text-field
            :prompt-text prompt
            :text filter-term
            :on-text-changed {:event-type :set-filter-term}}
   :root-props {:event-filter {:event-type :filter-root-events}}
   :content {:fx/type fx.ext.list-view/with-selection-props
             :props {:selection-mode selection
                     :on-selected-indices-changed {:event-type :set-selected-indices}}
             :desc {:fx/type fx/ext-on-instance-lifecycle
                    :on-created select-first-list-item-on-items-changed!
                    :desc {:fx/type ext-with-identity-items-props
                           :props {:items filtered-items}
                           :desc {:fx/type fx.list-view/lifecycle
                                  :fixed-cell-size 27
                                  :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                                 :describe cell-fn}}}}}
   :footer {:fx/type dialog-buttons
            :children [{:fx/type fxui/button
                        :text ok-label
                        :variant :primary
                        :disable (zero? (count filtered-items))
                        :on-action {:event-type :confirm}
                        :default-button true}]}})

(defn- wrap-cell-fn [f]
  (fn [item]
    (when (some? item)
      (assoc (f item) :on-mouse-clicked {:event-type :select-item-on-double-click}))))

(defn- select-list-dialog-event-handler [filter-fn items]
  (fn [state event]
    (case (:event-type event)
      :cancel (assoc state ::fxui/result nil)
      :confirm (assoc state ::fxui/result {:filter-term (:filter-term state)
                                           :selected-items (mapv (:filtered-items state) (:selected-indices state))})
      :set-selected-indices (assoc state :selected-indices (:fx/event event))
      :select-item-on-double-click (let [^MouseEvent e (:fx/event event)]
                                     (if (and (= MouseButton/PRIMARY (.getButton e))
                                              (= 2 (.getClickCount e)))
                                       (recur state (assoc event :event-type :confirm))
                                       state))
      :filter-root-events (let [^Event e (:fx/event event)]
                            (if (and (instance? KeyEvent e)
                                     (= KeyEvent/KEY_PRESSED (.getEventType e)))
                              (let [^KeyEvent e e]
                                (condp = (.getCode e)
                                  KeyCode/UP (let [view (.getTarget e)]
                                               (when (instance? ListView (.getTarget e))
                                                 (let [^ListView view view]
                                                   (when (zero? (.getSelectedIndex (.getSelectionModel view)))
                                                     (.consume e)
                                                     (.fireEvent view (KeyEvent.
                                                                        KeyEvent/KEY_PRESSED
                                                                        "\t"
                                                                        "\t"
                                                                        KeyCode/TAB
                                                                        #_shift true
                                                                        #_control false
                                                                        #_alt false
                                                                        #_meta false)))))
                                               state)
                                  KeyCode/DOWN (let [view (.getTarget e)]
                                                 (when (instance? TextField view)
                                                   (let [^TextField view view]
                                                     (.consume e)
                                                     (.fireEvent view (KeyEvent.
                                                                        KeyEvent/KEY_PRESSED
                                                                        "\t"
                                                                        "\t"
                                                                        KeyCode/TAB
                                                                        #_shift false
                                                                        #_control false
                                                                        #_alt false
                                                                        #_meta false))))
                                                 state)
                                  KeyCode/ENTER (if (empty? (:filtered-items state))
                                                  state
                                                  (recur state (assoc event :event-type :confirm)))
                                  KeyCode/ESCAPE (recur state (assoc event :event-type :cancel))
                                  state))
                              state))
      :set-filter-term (let [term (:fx/event event)
                             filtered-items (vec (filter-fn term items))]
                         (assoc state
                           :filter-term term
                           :filtered-items filtered-items
                           :selected-indices (if (seq filtered-items) [0] []))))))

(defn make-select-list-dialog
  "Show dialog that allows the user to select one or many of the suggested items

  Returns a coll of selected items or nil if nothing was selected

  Supported options keys (all optional):

      :title          dialog title, defaults to \"Select Item\"
      :ok-label       label on confirmation button, defaults to \"OK\"
      :filter         initial filter term string, defaults to value in
                      :filter-atom, or, if absent, to empty string
      :filter-atom    atom with initial filter term string; if supplied, its
                      value will be reset to final filter term on confirm
      :cell-fn        cell factory fn, defaults to identity; should return a
                      cljfx prop map for a list cell
      :selection      a selection mode, either :single (default) or :multiple
      :filter-fn      filtering fn of 2 args (filter term and items), should
                      return a filtered coll of items
      :filter-on      if no custom :filter-fn is supplied, use this fn of item
                      to string for default filtering, defaults to str
      :prompt         filter text field's prompt text"
  ([items]
   (make-select-list-dialog items {}))
  ([items options]
   (let [cell-fn (wrap-cell-fn (:cell-fn options identity))
         filter-atom (:filter-atom options)
         filter-fn (or (:filter-fn options)
                       (fn [text items]
                         (default-filter-fn (:filter-on options str) text items)))
         filter-term (or (:filter options)
                         (some-> filter-atom deref)
                         "")
         initial-filtered-items (vec (filter-fn filter-term items))
         result (fxui/show-dialog-and-await-result!
                  :initial-state {:filter-term filter-term
                                  :filtered-items initial-filtered-items
                                  :selected-indices (if (seq initial-filtered-items) [0] [])}
                  :event-handler (select-list-dialog-event-handler filter-fn items)
                  :description {:fx/type select-list-dialog
                                :title (:title options "Select Item")
                                :ok-label (:ok-label options "OK")
                                :prompt (:prompt options "Type to filter")
                                :cell-fn cell-fn
                                :selection (:selection options :single)})]
     (when result
       (let [{:keys [filter-term selected-items]} result]
         (when (and filter-atom selected-items)
           (reset! filter-atom filter-term))
         selected-items)))))

