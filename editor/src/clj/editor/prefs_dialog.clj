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
            [cljfx.fx.combo-box :as fx.combo-box]
            [cljfx.fx.combo-box-list-cell :as fx.combo-box-list-cell]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.list-cell :as fx.list-cell]
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
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.system :as system]
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
      [{:pattern (localization/message "prefs.tab.general")
        :paths [[:workflow :load-external-changes-on-app-focus]
                [:workflow :save-on-app-focus-lost]
                [:bundle :open-output-directory]
                [:build :open-html5-build]
                [:build :texture-compression]
                [:run :quit-on-escape]
                [:asset-browser :track-active-tab]
                [:build :lint-code]
                [:window :locale]
                [:run :engine-arguments]]}
       {:pattern (localization/message "prefs.tab.code")
        :paths [[:code :custom-editor]
                [:code :open-file]
                [:code :open-file-at-line]
                [:code :font :name]
                [:code :zoom-on-scroll]
                [:code :hover]
                [:code :auto-closing-parens]]}
       {:pattern (localization/message "prefs.tab.extensions")
        :paths [[:extensions :build-server]
                [:extensions :build-server-username]
                [:extensions :build-server-password]
                [:extensions :build-server-headers]]}
       {:pattern (localization/message "prefs.tab.tools")
        :paths [[:tools :adb-path]
                [:tools :ios-deploy-path]]}]

      (system/defold-dev?)
      (conj {:pattern (localization/message "prefs.tab.dev")
             :paths [[:dev :custom-engine]]}))))

(defmulti form-input (fn [_path schema _value _on-value-changed _localization-state _localization]
                       (or (:type (:ui schema))
                           (:type schema))))

(defmethod form-input :default [_ _ value _ _ _]
  {:fx/type fxui/label
   :text (str value)})

(defmethod form-input :boolean [_ _ value on-value-changed _ _]
  {:fx/type fx.h-box/lifecycle
   :children [{:fx/type fxui/check-box
               :selected value
               :on-selected-changed on-value-changed}]})

(defn- text-input [path value on-value-changed localization-state fx-type unlocalizable-prompt]
  (let [desc {:fx/type fx-type
              :value value
              :on-value-changed on-value-changed}]
    (if unlocalizable-prompt
      (assoc desc :prompt-text unlocalizable-prompt)
      (let [prompt-key (str "prefs.prompt." (coll/join-to-string "." (e/map name path)))]
        (cond-> desc
                (localization/defines-message-key? localization-state prompt-key)
                (assoc :prompt-text (localization-state (localization/message prompt-key))))))))

(defmethod form-input :string [path schema value on-value-changed localization-state _]
  (text-input path value on-value-changed localization-state (if (:multiline (:ui schema)) fxui/value-area fxui/value-field) (:prompt (:ui schema))))

(defmethod form-input :password [path schema value on-value-changed localization-state _]
  (text-input path value on-value-changed localization-state fxui/password-value-field (:prompt (:ui schema))))

(defn- locale-display-name-cell-factory [s]
  {:text (localization/locale-display-name s)})

(defmethod form-input :locale [_ _ _ _ localization-state localization]
  {:fx/type fx.combo-box/lifecycle
   :value (localization/current-locale localization-state)
   :items (localization/available-locales localization-state)
   :button-cell locale-display-name-cell-factory
   :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                  :describe locale-display-name-cell-factory}
   :on-value-changed #(localization/set-locale! localization %)})

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
             :typable (localization/message "prefs.keymap.warning.typable")
             :conflict (localization/message
                         "prefs.keymap.warning.conflicts"
                         {"shortcuts" (->> warnings
                                           (mapv #(-> % :command command-label))
                                           sort
                                           vec
                                           localization/and-list)})
             (string/capitalize (name type))))
         (group-by :type warnings)))

(defn- describe-shortcut-cell [keymap localization-state command]
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
                                                (e/map localization-state)
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
  [{:keys [keymap command shortcut swap-shortcut swap-state update-keymap localization-state]}]
  (let [warnings (when shortcut
                   (keymap/warnings (keymap/from keymap {command {:add #{(str shortcut)}}}) command shortcut))]
    {:fx/type fxui/vertical
     :padding :small
     :spacing :small
     :children
     (cond->
       [{:fx/type fxui/label
         :alignment :center
         :text (localization-state (localization/message "prefs.keymap.new-shortcut" {"shortcut" (command-label command)}))}
        {:fx/type fxui/text-field
         :event-filter #(filter-new-shortcut-text-field-events command shortcut swap-shortcut swap-state update-keymap %)
         :alignment :center
         :text (some-> shortcut keymap/shortcut-distinct-display-text)}
        {:fx/type fxui/label
         :alignment :center
         :text-alignment :center
         :color :hint
         :text (localization-state
                 (if (not shortcut)
                   (localization/message "prefs.keymap.new-shortcut.press-desired-combination")
                   (if (= shortcut escape-shortcut)
                     (localization/message "prefs.keymap.new-shortcut.commit-or-cancel" {"enter" (keymap/shortcut-display-text enter-shortcut)
                                                                                         "escape" (keymap/shortcut-display-text escape-shortcut)})
                     (localization/message "prefs.keymap.new-shortcut.commit" {"enter" (keymap/shortcut-display-text enter-shortcut)}))))}]
       warnings
       (conj {:fx/type fxui/paragraph
              :text (localization-state
                      (localization/message
                        "prefs.keymap.warnings"
                        {"warnings" (->> warnings
                                         (warnings-messages)
                                         (e/map #(localization-state (localization/message "prefs.keymap.warning" {"warning" %})))
                                         (coll/join-to-string ""))}))}))}))

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
  [{:keys [update-keymap state swap-state keymap handler-state localization-state]}]
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
       :prompt-text (localization-state (localization/message "prefs.keymap.filter.prompt"))
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
                         :localization-state localization-state
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
                        :text (localization-state (localization/message "prefs.keymap.context-menu.new-shortcut"))
                        :on-action #(handle-new-shortcut-action swap-state command %)}
                       (e/concat
                         (when (not= shortcuts default-shortcuts)
                           [{:fx/type fx.menu-item/lifecycle
                             :text (localization-state (localization/message "prefs.keymap.context-menu.reset-to-default"))
                             :on-action #(handle-reset-shortcuts-action update-keymap command %)}])
                         (->> shortcuts
                              (mapv (coll/pair-fn keymap/shortcut-distinct-display-text))
                              (sort-by key)
                              (e/map (fn [[text shortcut]]
                                       {:fx/type fx.menu-item/lifecycle
                                        :text (localization-state (localization/message "prefs.keymap.context-menu.remove" {"shortcut" text}))
                                        :on-action #(handle-remove-shortcut-action update-keymap command shortcut %)})))
                         (when default-shortcuts
                           (let [removed-built-in-shortcuts (set/difference default-shortcuts (or shortcuts #{}))]
                             (->> removed-built-in-shortcuts
                                  (mapv (coll/pair-fn keymap/shortcut-distinct-display-text))
                                  (sort-by key)
                                  (e/map (fn [[text shortcut]]
                                           {:fx/type fx.menu-item/lifecycle
                                            :text (localization-state (localization/message "prefs.keymap.context-menu.add" {"shortcut" text}))
                                            :on-action #(handle-add-shortcut-action update-keymap command shortcut %)}))))))))}))
       :desc
       {:fx/type fx.table-view/lifecycle
        :style-class ["table-view" "ext-table-view" "keymap-table"]
        :column-resize-policy TableView/CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS
        :placeholder {:fx/type fx.region/lifecycle}
        :columns [{:fx/type fx.table-column/lifecycle
                   :reorderable false
                   :sortable false
                   :text (localization-state (localization/message "prefs.keymap.command"))
                   :cell-value-factory identity
                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                  :describe (fn/partial #'describe-command-cell keymap)}}
                  {:fx/type fx.table-column/lifecycle
                   :reorderable false
                   :sortable false
                   :text (localization-state (localization/message "prefs.keymap.shortcuts"))
                   :cell-value-factory identity
                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                  :describe (fn/partial #'describe-shortcut-cell keymap localization-state)}}]
        :on-context-menu-requested #(handle-table-view-context-menu-requested-event swap-state %)
        :on-key-pressed #(handle-table-view-key-pressed swap-state %)
        :on-mouse-clicked #(handle-table-view-mouse-clicked swap-state %)
        :items commands}}]}))

(defn- handle-scene-key-pressed [^KeyEvent e]
  (when (= KeyCode/ESCAPE (.getCode e))
    (.consume e)
    (.hide (.getWindow ^Scene (.getSource e)))))

(fxui/defc dialog-view
  {:compose [{:fx/type fx/ext-watcher
              :ref prefs/global-state
              :key :prefs-state}
             {:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}]}
  [{:keys [prefs prefs-state localization-state localization result-fn]}]
  {:fx/type fxui/dialog-stage
   :showing true
   :on-hidden result-fn
   :title (localization-state (localization/message "prefs.dialog.title"))
   :resizable true
   :min-width 650
   :min-height 500
   :scene
   {:fx/type fx.scene/lifecycle
    :stylesheets [(str (io/resource "dialogs.css"))]
    :on-key-pressed handle-scene-key-pressed
    :root
    {:fx/type fx.tab-pane/lifecycle
     :tab-closing-policy :unavailable
     :tabs (vec
             (e/conj
               (e/map
                 (fn [{:keys [pattern paths]}]
                   {:fx/type fx.tab/lifecycle
                    :text (localization-state pattern)
                    :content {:fx/type fxui/grid
                              :padding :medium
                              :spacing :medium
                              :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                                                   {:fx/type fx.column-constraints/lifecycle
                                                    :hgrow :always}]
                              :children (->> paths
                                             (e/mapcat-indexed
                                               (fn [row path]
                                                 (let [key-suffix (coll/join-to-string "." (e/map name path))
                                                       label-key (str "prefs.label." key-suffix)
                                                       tooltip-key (str "prefs.help." key-suffix)]
                                                   [(cond->
                                                      {:fx/type fxui/label
                                                       :grid-pane/row row
                                                       :grid-pane/column 0
                                                       :grid-pane/fill-width false
                                                       :grid-pane/fill-height false
                                                       :grid-pane/valignment :top
                                                       :text (localization-state (localization/message label-key))}
                                                      (localization/defines-message-key? localization-state tooltip-key)
                                                      (assoc :tooltip (localization-state (localization/message tooltip-key))))
                                                    (assoc
                                                      (form-input path
                                                                  (prefs/schema prefs-state prefs path)
                                                                  (prefs/get prefs-state prefs path)
                                                                  (fn/partial prefs/set! prefs path)
                                                                  localization-state
                                                                  localization)
                                                      :grid-pane/row row
                                                      :grid-pane/column 1)])))
                                             vec)}})
                 @pages)
               {:fx/type fx.tab/lifecycle
                :text (localization-state (localization/message "prefs.tab.keymap"))
                :content {:fx/type keymap-view
                          :localization-state localization-state
                          :keymap (keymap/from-prefs prefs-state prefs)
                          :update-keymap (fn/partial prefs/update! prefs [:window :keymap])}}))}}})

(defn open!
  "Show the prefs dialog and block the thread until the dialog is closed"
  [prefs localization]
  (fxui/show-stateless-dialog-and-await-result!
    (fn [result-fn]
      {:fx/type dialog-view
       :result-fn result-fn
       :localization localization
       :prefs prefs}))
  nil)
