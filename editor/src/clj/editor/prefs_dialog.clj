;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.prefs-dialog
  (:require [camel-snake-kebab :as camel]
            [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.popup :as fx.popup]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.tab :as fx.tab]
            [cljfx.fx.tab-pane :as fx.tab-pane]
            [cljfx.fx.table-cell :as fx.table-cell]
            [cljfx.fx.table-column :as fx.table-column]
            [cljfx.fx.table-view :as fx.table-view]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.keymap :as keymap]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.util :as eutil]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.text-util :as text-util])
  (:import [javafx.event ActionEvent]
           [javafx.scene Scene]
           [javafx.scene.control MenuItem TableView]
           [javafx.scene.input ContextMenuEvent KeyCode KeyCodeCombination KeyCombination KeyCombination$ModifierValue KeyEvent MouseEvent]
           [javafx.stage Window]))

(set! *warn-on-reflection* true)

(def pages
  (delay
    (cond->
      [{:name "General"
        :paths [[:workflow :load-external-changes-on-app-focus]
                [:bundle :open-output-directory]
                [:build :open-html5-build]
                [:build :texture-compression]
                [:run :quit-on-escape]
                [:asset-browser :track-active-tab]
                [:build :lint-code]
                [:run :engine-arguments]]}
       {:name "Code"
        :paths [[:code :custom-editor]
                [:code :open-file]
                [:code :open-file-at-line]
                [:code :font :name]
                [:code :zoom-on-scroll]
                [:code :hover]]}
       {:name "Extensions"
        :paths [[:extensions :build-server]
                [:extensions :build-server-username]
                [:extensions :build-server-password]
                [:extensions :build-server-headers]]}
       {:name "Tools"
        :paths [[:tools :adb-path]
                [:tools :ios-deploy-path]]}]

      (system/defold-dev?)
      (conj {:name "Dev"
             :paths [[:dev :custom-engine]]}))))

(defmulti form-input (fn [schema _value _on-value-changed]
                       (or (:type (:ui schema))
                           (:type schema))))

(defmethod form-input :default [_ value _]
  {:fx/type fxui/label
   :text (str value)})

(defmethod form-input :boolean [_ value on-value-changed]
  {:fx/type fx.h-box/lifecycle
   :children [{:fx/type fxui/check-box
               :selected value
               :on-selected-changed on-value-changed}]})

(defmethod form-input :string [schema value on-value-changed]
  (let [{:keys [prompt multiline]} (:ui schema)]
    (cond-> {:fx/type (if multiline fxui/value-area fxui/value-field)
             :value value
             :on-value-changed on-value-changed}
            prompt (assoc :prompt-text prompt))))

(defmethod form-input :password [schema value on-value-changed]
  (let [prompt (-> schema :ui :prompt)]
    (cond-> {:fx/type fxui/password-value-field
             :value value
             :on-value-changed on-value-changed}
            prompt (assoc :prompt-text prompt))))

(defn- describe-command-cell [keymap command]
  (if command
    (let [changed (not= (keymap/shortcuts keymap command)
                        (keymap/shortcuts (keymap/default) command))]
      {:graphic {:fx/type fxui/horizontal
                 :style-class "keymap-command"
                 :pseudo-classes (if changed #{:overridden} #{})
                 :spacing :small
                 :children (->> (string/split (name command) #"\.")
                                (e/map (fn [part]
                                         {:fx/type fxui/label
                                          :text (camel/->TitleCase part)}))
                                (e/interpose {:fx/type fxui/label
                                              :style-class "keymap-command-arrow"
                                              :text "→"})
                                vec)}})
    {}))

(defn- command-label [command]
  (->> (string/split (name command) #"\.")
       (e/map camel/->TitleCase)
       (coll/join-to-string " → ")))

(defn- filterable-command-label [command]
  (->> (string/split (name command) #"\.")
       (e/map camel/->TitleCase)
       (coll/join-to-string " ")))

(defn- warnings-messages [warnings]
  (e/map (fn [[type warnings]]
           (case type
             :typable "Typable (may interfere with text inputs)"
             :conflict (str "Conflicts with "
                            (->> warnings
                                 (mapv #(-> % :command command-label))
                                 sort
                                 (eutil/join-words ", " " and ")))
             (string/capitalize (name type))))
         (group-by :type warnings)))

(defn- describe-shortcut-cell [keymap command]
  (if command
    (let [shortcuts (keymap/shortcuts keymap command)]
      {:graphic
       {:fx/type fxui/horizontal
        :spacing :small
        :style-class "keymap-shortcuts"
        :children
        (->> shortcuts
             (mapv (coll/pair-fn keymap/shortcut-distinct-display-text))
             (sort-by key)
             (mapv
               (fn [[text shortcut]]
                 (let [warnings (keymap/warnings keymap command shortcut)]
                   (cond-> {:fx/type fxui/label
                            :style-class "keymap-shortcut"
                            :text text}
                           warnings
                           (assoc :pseudo-classes #{:warning}
                                  :tooltip (->> warnings
                                                warnings-messages
                                                (coll/join-to-string "\n"))))))))}})
    {}))

(defn filtered-command? [command shortcuts filter-text]
  (or (text-util/includes-ignore-case? (filterable-command-label command) filter-text)
      (coll/some #(text-util/includes-ignore-case? (keymap/shortcut-filterable-text %) filter-text) shortcuts)))

(defn- handle-table-view-context-menu-requested-event [swap-state ^ContextMenuEvent e]
  (let [^TableView table-view (.getSource e)]
    (when-let [command (.getSelectedItem (.getSelectionModel table-view))]
      (swap-state assoc :context-menu {:command command :x (.getScreenX e) :y (.getScreenY e)}))))

(defn- handle-reset-shortcuts-action [update-keymap command _]
  (update-keymap dissoc command))

(defn- handle-add-shortcut-action [update-keymap command shortcut _]
  (let [shortcut-str (str shortcut)]
    (update-keymap update command (fn [old-changes]
                                    (if (and (contains? (keymap/shortcuts (keymap/default) command) shortcut)
                                             (contains? (:remove old-changes) shortcut-str))
                                      (update old-changes :remove (fnil disj #{}) shortcut-str)
                                      (update old-changes :add (fnil conj #{}) shortcut-str))))))

(defn handle-remove-shortcut-action [update-keymap command shortcut _]
  (let [shortcut-str (str shortcut)]
    (update-keymap
      update
      command
      (fn [old-changes]
        (cond-> old-changes
                (contains? (:add old-changes) shortcut-str)
                (update :add (fnil disj #{}) shortcut-str)

                (contains? (keymap/shortcuts (keymap/default) command) shortcut)
                (update :remove (fnil conj #{}) shortcut-str))))))

(def ^:private ^:const new-shortcut-popup-width 400)
(def ^:private ^:const new-shortcut-popup-height 200)
(def ^:private enter-shortcut (KeyCombination/valueOf "Enter"))
(def ^:private escape-shortcut (KeyCombination/valueOf "Esc"))

(defn- filter-new-shortcut-text-field-events [command displayed-shortcut swap-shortcut swap-state update-keymap e]
  (condp instance? e
    KeyEvent
    (let [^KeyEvent e e]
      (.consume e)
      (when (and (= KeyEvent/KEY_PRESSED (.getEventType e))
                 (not= KeyCode/ALT (.getCode e))
                 (not= KeyCode/CONTROL (.getCode e))
                 (not= KeyCode/META (.getCode e))
                 (not= KeyCode/SHIFT (.getCode e))
                 (not= KeyCode/COMMAND (.getCode e))
                 (not= KeyCode/UNDEFINED (.getCode e)))
        (let [shift (if (.isShiftDown e) KeyCombination$ModifierValue/DOWN KeyCombination$ModifierValue/UP)
              control (if (.isControlDown e) KeyCombination$ModifierValue/DOWN KeyCombination$ModifierValue/UP)
              alt (if (.isAltDown e) KeyCombination$ModifierValue/DOWN KeyCombination$ModifierValue/UP)
              meta (if (.isMetaDown e) KeyCombination$ModifierValue/DOWN KeyCombination$ModifierValue/UP)
              shortcut (KeyCodeCombination. (.getCode e) shift control alt meta KeyCombination$ModifierValue/UP)]
          (cond
            (= displayed-shortcut shortcut escape-shortcut)
            (swap-state dissoc :new-shortcut-popup)

            (and displayed-shortcut (= shortcut enter-shortcut))
            (let [shortcut-str (str displayed-shortcut)]
              (swap-state dissoc :new-shortcut-popup)
              (update-keymap update command (fn [old-changes]
                                              (-> old-changes
                                                  (update :remove (fnil disj #{}) shortcut-str)
                                                  (update :add (fnil conj #{}) shortcut-str)))))
            :else
            (swap-shortcut (constantly shortcut))))))

    ContextMenuEvent
    (let [^ContextMenuEvent e e]
      (.consume e))

    nil))

(fxui/defc new-shortcut-view
  {:compose [{:fx/type fx/ext-state
              :initial-state nil
              :key :shortcut
              :swap-key :swap-shortcut}]}
  [{:keys [keymap command shortcut swap-shortcut swap-state update-keymap]}]
  (let [warnings (when shortcut
                   (keymap/warnings (keymap/from keymap {command {:add #{(str shortcut)}}}) command shortcut))]
    {:fx/type fxui/vertical
     :padding :small
     :spacing :small
     :children
     (cond->
       [{:fx/type fxui/label
         :alignment :center
         :text (str "New '" (command-label command) "' Shortcut")}
        {:fx/type fxui/text-field
         :event-filter #(filter-new-shortcut-text-field-events command shortcut swap-shortcut swap-state update-keymap %)
         :alignment :center
         :text (some-> shortcut keymap/shortcut-distinct-display-text)}
        {:fx/type fxui/label
         :alignment :center
         :text-alignment :center
         :color :hint
         :text (if (not shortcut)
                 "Press the desired key combination"
                 (str
                   (str "Press " (keymap/shortcut-display-text enter-shortcut) " to commit")
                   (when (= shortcut escape-shortcut)
                     (str ", " (keymap/shortcut-display-text escape-shortcut) " again to cancel"))))}]
       warnings
       (conj {:fx/type fxui/paragraph
              :text (str "Warnings:\n• " (coll/join-to-string "\n• " (warnings-messages warnings)))}))}))

(defn- show-new-shortcut-dialog! [swap-state command ^Window window]
  (let [root (.getRoot (.getScene window))
        screen-bounds (.localToScreen root (.getBoundsInLocal root))
        x (- (.getCenterX screen-bounds) (* 0.5 new-shortcut-popup-width))
        y (- (.getCenterY screen-bounds) (* 0.5 new-shortcut-popup-height))]
    (swap-state assoc :new-shortcut-popup {:command command :x x :y y})))

(defn- handle-new-shortcut-action [swap-state command ^ActionEvent e]
  (show-new-shortcut-dialog! swap-state command (.getOwnerWindow (.getParentPopup ^MenuItem (.getSource e)))))

(defn- handle-table-view-key-pressed [swap-state ^KeyEvent e]
  (when (or (= KeyCode/SPACE (.getCode e)) (= KeyCode/ENTER (.getCode e)))
    (let [^TableView table-view (.getSource e)]
      (when-let [command (.getSelectedItem (.getSelectionModel table-view))]
        (show-new-shortcut-dialog! swap-state command (.getWindow (.getScene table-view)))))))

(defn- handle-table-view-mouse-clicked [swap-state ^MouseEvent e]
  (when (= 2 (.getClickCount e))
    (let [^TableView table-view (.getSource e)]
      (when-let [command (.getSelectedItem (.getSelectionModel table-view))]
        (show-new-shortcut-dialog! swap-state command (.getWindow (.getScene table-view)))))))

(fxui/defc keymap-view
  {:compose [{:fx/type fx/ext-watcher
              :ref handler/state-atom
              :key :handler-state}
             {:fx/type fx/ext-state
              :initial-state {:filter-text ""}}]}
  [{:keys [update-keymap state swap-state keymap handler-state]}]
  (let [{:keys [filter-text context-menu new-shortcut-popup]} state
        commands (-> (handler/public-commands handler-state)
                     (into (keymap/commands keymap))
                     (cond->> (not= filter-text "")
                              (filterv #(filtered-command? % (keymap/shortcuts keymap %) filter-text)))
                     (sort)
                     (vec))]
    {:fx/type fxui/vertical
     :padding :medium
     :spacing :medium
     :children
     [{:fx/type fxui/text-field
       :prompt-text "Filter keymap..."
       :text filter-text
       :on-text-changed #(swap-state assoc :filter-text %)}
      {:fx/type fxui/with-popup-window
       :v-box/vgrow :always
       :popup
       (cond
         new-shortcut-popup
         (let [{:keys [command x y]} new-shortcut-popup]
           {:fx/type fx.popup/lifecycle
            :on-window true
            :showing true
            :anchor-x x
            :anchor-y y
            :auto-hide true
            :on-hiding (fn [_] (swap-state dissoc :new-shortcut-popup))
            :content [{:fx/type fx.stack-pane/lifecycle
                       :pref-width new-shortcut-popup-width
                       :children
                       [{:fx/type fx.region/lifecycle
                         :style-class "keymap-new-shortcut-background"}
                        {:fx/type new-shortcut-view
                         :keymap keymap
                         :command command
                         :swap-state swap-state
                         :update-keymap update-keymap}]}]})

         context-menu
         (let [{:keys [command x y]} context-menu
               shortcuts (keymap/shortcuts keymap command)
               default-shortcuts (keymap/shortcuts (keymap/default) command)]
           {:fx/type fx.context-menu/lifecycle
            :on-window true
            :showing true
            :anchor-x x
            :anchor-y y
            :on-hiding (fn [_] (swap-state dissoc :context-menu))
            :items (vec
                     (e/cons
                       {:fx/type fx.menu-item/lifecycle
                        :text "New Shortcut..."
                        :on-action #(handle-new-shortcut-action swap-state command %)}
                       (e/concat
                         (when (not= shortcuts default-shortcuts)
                           [{:fx/type fx.menu-item/lifecycle
                             :text "Reset to Default"
                             :on-action #(handle-reset-shortcuts-action update-keymap command %)}])
                         (->> shortcuts
                              (mapv (coll/pair-fn keymap/shortcut-distinct-display-text))
                              (sort-by key)
                              (e/map (fn [[text shortcut]]
                                       {:fx/type fx.menu-item/lifecycle
                                        :text (str "Remove " text)
                                        :on-action #(handle-remove-shortcut-action update-keymap command shortcut %)})))
                         (when default-shortcuts
                           (let [removed-built-in-shortcuts (set/difference default-shortcuts (or shortcuts #{}))]
                             (->> removed-built-in-shortcuts
                                  (mapv (coll/pair-fn keymap/shortcut-distinct-display-text))
                                  (sort-by key)
                                  (e/map (fn [[text shortcut]]
                                           {:fx/type fx.menu-item/lifecycle
                                            :text (str "Add " text)
                                            :on-action #(handle-add-shortcut-action update-keymap command shortcut %)}))))))))}))
       :desc
       {:fx/type fx.table-view/lifecycle
        :style-class ["table-view" "ext-table-view" "keymap-table"]
        :column-resize-policy TableView/CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS
        :placeholder {:fx/type fx.region/lifecycle}
        :columns [{:fx/type fx.table-column/lifecycle
                   :reorderable false
                   :sortable false
                   :text "Command"
                   :cell-value-factory identity
                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                  :describe (fn/partial #'describe-command-cell keymap)}}
                  {:fx/type fx.table-column/lifecycle
                   :reorderable false
                   :sortable false
                   :text "Shortcuts"
                   :cell-value-factory identity
                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                  :describe (fn/partial #'describe-shortcut-cell keymap)}}]
        :on-context-menu-requested #(handle-table-view-context-menu-requested-event swap-state %)
        :on-key-pressed #(handle-table-view-key-pressed swap-state %)
        :on-mouse-clicked #(handle-table-view-mouse-clicked swap-state %)
        :items commands}}]}))

(defn- prefs-tab-pane [{:keys [prefs prefs-state]}]
  {:fx/type fx.tab-pane/lifecycle
   :tab-closing-policy :unavailable
   :tabs (vec
           (e/conj
             (e/map
               (fn [{:keys [name paths]}]
                 {:fx/type fx.tab/lifecycle
                  :text name
                  :content {:fx/type fxui/grid
                            :padding :medium
                            :spacing :medium
                            :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                                                 {:fx/type fx.column-constraints/lifecycle
                                                  :hgrow :always}]
                            :children (->> paths
                                           (e/mapcat-indexed
                                             (fn [row path]
                                               (let [schema (prefs/schema prefs-state prefs path)
                                                     tooltip (:description (:ui schema))]
                                                 [(cond-> {:fx/type fxui/label
                                                           :grid-pane/row row
                                                           :grid-pane/column 0
                                                           :grid-pane/fill-width false
                                                           :grid-pane/fill-height false
                                                           :grid-pane/valignment :top
                                                           :text (prefs/label prefs-state prefs path)}
                                                          tooltip
                                                          (assoc :tooltip tooltip))
                                                  (assoc
                                                    (form-input schema
                                                                (prefs/get prefs-state prefs path)
                                                                (fn/partial prefs/set! prefs path))
                                                    :grid-pane/row row
                                                    :grid-pane/column 1)])))
                                           vec)}})
               @pages)
             {:fx/type fx.tab/lifecycle
              :text "Keymap"
              :content {:fx/type keymap-view
                        :keymap (keymap/from-prefs prefs-state prefs)
                        :update-keymap (fn/partial prefs/update! prefs [:window :keymap])}}))})

(defn- handle-scene-key-pressed [^KeyEvent e]
  (when (= KeyCode/ESCAPE (.getCode e))
    (.consume e)
    (.hide (.getWindow ^Scene (.getSource e)))))

(defn open!
  "Show the prefs dialog and block the thread until the dialog is closed"
  [prefs]
  (fxui/show-stateless-dialog-and-await-result!
    (fn [result-fn]
      {:fx/type fxui/dialog-stage
       :showing true
       :on-hidden result-fn
       :title "Preferences"
       :resizable true
       :min-width 650
       :min-height 500
       :scene {:fx/type fx.scene/lifecycle
               :stylesheets [(str (io/resource "dialogs.css"))]
               :on-key-pressed handle-scene-key-pressed
               :root {:fx/type fx/ext-watcher
                      :ref prefs/global-state
                      :key :prefs-state
                      :desc {:fx/type prefs-tab-pane
                             :prefs prefs}}}}))
  nil)
