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

(ns editor.keymap
  (:require [editor.ui :as ui]
            [editor.util :as util])
  (:import [javafx.scene Scene]
           [javafx.scene.input KeyCharacterCombination KeyCodeCombination KeyCombination KeyCombination$ModifierValue KeyEvent]))

(set! *warn-on-reflection* true)

(def platform->default-key-bindings
  ;; We should generally avoid adding shortcuts that can also be
  ;; interpreted as typable text. This includes "A", "Shift+E" but
  ;; also combinations like Alt+Ctrl+2 because on windows Alt+Ctrl is
  ;; an alternative way to enter AltGR and AltGR+2 is @ on some layouts -
  ;; something you may want to type in the code editor.
  ;; The function key-binding-data->keymap attempts to catch this type
  ;; of mistake.
  ;; Note that specifying Shortcut key (which is Meta on mac and Ctrl on windows
  ;; and linux) is not allowed.
  {:macos   [["A" :add]
            ["Alt+Backspace" :delete-prev-word]
            ["Alt+Delete" :delete-next-word]
            ["Alt+Down" :end-of-line]
            ["Alt+Down" :move-down]
            ["Alt+Left" :prev-word]
            ["Alt+Meta+E" :select-next-occurrence]
            ["Alt+Meta+F" :replace-text]
            ["Alt+Meta+G" :replace-next]
            ["Alt+Right" :next-word]
            ["Alt+Up" :beginning-of-line]
            ["Alt+Up" :move-up]
            ["Backspace" :delete-backward]
            ["Ctrl+A" :beginning-of-line]
            ["Ctrl+D" :delete-line]
            ["Ctrl+E" :end-of-line]
            ["Ctrl+I" :reindent]
            ["Ctrl+K" :delete-to-end-of-line]
            ["Ctrl+Meta+H" :toggle-component-guides]
            ["Ctrl+R" :reload-stylesheet]
            ["Ctrl+Space" :proposals]
            ["Delete" :delete]
            ["Down" :down]
            ["E" :rotate-tool]
            ["End" :end-of-line]
            ["Enter" :enter]
            ["Esc" :escape]
            ["F" :frame-selection]
            ["F1" :documentation]
            ["F10" :step-over]
            ["F11" :step-into]
            ["F2" :rename]
            ["F5" :start-debugger]
            ["F5" :continue]
            ["F6" :toggle-pane-left]
            ["F7" :toggle-pane-bottom]
            ["F8" :toggle-pane-right]
            ["F9" :toggle-breakpoint]
            ["Home" :beginning-of-line-text]
            ["Left" :left]
            ["Meta+'+'" :zoom-in]
            ["Meta+'-'" :zoom-out]
            ["Meta+A" :select-all]
            ["Meta+B" :build]
            ["Meta+C" :copy]
            ["Meta+Comma" :preferences]
            ["Meta+D" :select-next-occurrence]
            ["Meta+Delete" :delete-to-end-of-line]
            ["Meta+Down" :end-of-file]
            ["Meta+E" :hide-selected]
            ["Meta+F" :filter-form]
            ["Meta+F" :find-text]
            ["Meta+G" :find-next]
            ["Meta+L" :goto-line]
            ["Meta+Left" :beginning-of-line-text]
            ["Meta+N" :new-file]
            ["Meta+O" :open]
            ["Meta+P" :open-asset]
            ["Meta+Q" :quit]
            ["Meta+R" :hot-reload]
            ["Meta+Right" :end-of-line]
            ["Meta+S" :save-all]
            ["Meta+Slash" :toggle-comment]
            ["Meta+T" :scene-stop]
            ["Meta+U" :rebundle]
            ["Meta+Up" :beginning-of-file]
            ["Meta+V" :paste]
            ["Meta+W" :close]
            ["Meta+X" :cut]
            ["Meta+Z" :undo]
            ["Page Down" :page-down]
            ["Page Up" :page-up]
            ["Period" :realign-camera]
            ["R" :scale-tool]
            ["Right" :right]
            ["Shift+A" :add-secondary]
            ["Shift+Alt+Down" :select-end-of-line]
            ["Shift+Alt+Left" :select-prev-word]
            ["Shift+Alt+Right" :select-next-word]
            ["Shift+Alt+Up" :select-beginning-of-line]
            ["Shift+Backspace" :delete-backward]
            ["Shift+Ctrl+A" :select-beginning-of-line]
            ["Shift+Ctrl+E" :select-end-of-line]
            ["Shift+Ctrl+Left" :select-prev-word]
            ["Shift+Ctrl+Right" :select-next-word]
            ["Shift+Down" :down-major]
            ["Shift+Down" :select-down]
            ["Shift+E" :erase-tool]
            ["Shift+End" :select-end-of-line]
            ["Shift+F11" :step-out]
            ["Shift+Home" :select-beginning-of-line-text]
            ["Shift+Left" :left-major]
            ["Shift+Left" :select-left]
            ["Shift+Meta+B" :rebuild]
            ["Shift+Meta+Delete" :delete-to-end-of-line]
            ["Shift+Meta+Down" :select-end-of-file]
            ["Shift+Meta+E" :show-last-hidden]
            ["Shift+Meta+F" :search-in-files]
            ["Shift+Meta+G" :find-prev]
            ["Shift+Meta+I" :toggle-visibility-filters]
            ["Shift+Meta+L" :split-selection-into-lines]
            ["Shift+Meta+Left" :select-beginning-of-line-text]
            ["Shift+Meta+R" :open-asset]
            ["Shift+Meta+Right" :select-end-of-line]
            ["Shift+Meta+Up" :select-beginning-of-file]
            ["Shift+Meta+W" :close-all]
            ["Shift+Meta+Z" :redo]
            ["Shift+Page Down" :select-page-down]
            ["Shift+Page Up" :select-page-up]
            ["Shift+Right" :right-major]
            ["Shift+Right" :select-right]
            ["Shift+Tab" :backwards-tab-trigger]
            ["Shift+Up" :select-up]
            ["Shift+Up" :up-major]
            ["Space" :scene-play]
            ["Space" :show-palette]
            ["Tab" :tab]
            ["Up" :up]
            ["W" :move-tool]]
   :win32 [["A" :add]
           ["Alt+Down" :move-down]
           ["Alt+Up" :move-up]
           ["Backspace" :delete-backward]
           ["Ctrl+'+'" :zoom-in]
           ["Ctrl+'-'" :zoom-out]
           ["Ctrl+A" :select-all]
           ["Ctrl+B" :build]
           ["Ctrl+Backspace" :delete-prev-word]
           ["Ctrl+C" :copy]
           ["Ctrl+Comma" :preferences]
           ["Ctrl+D" :select-next-occurrence]
           ["Ctrl+Delete" :delete-next-word]
           ["Ctrl+E" :hide-selected]
           ["Ctrl+End" :end-of-file]
           ["Ctrl+F" :filter-form]
           ["Ctrl+F" :find-text]
           ["Ctrl+G" :find-next]
           ["Ctrl+H" :replace-text]
           ["Ctrl+H" :toggle-component-guides]
           ["Ctrl+Home" :beginning-of-file]
           ["Ctrl+I" :reindent]
           ["Ctrl+K" :delete-to-end-of-line]
           ["Ctrl+L" :goto-line]
           ["Ctrl+Left" :prev-word]
           ["Ctrl+N" :new-file]
           ["Ctrl+O" :open]
           ["Ctrl+P" :open-asset]
           ["Ctrl+Q" :quit]
           ["Ctrl+R" :hot-reload]
           ["Ctrl+Right" :next-word]
           ["Ctrl+S" :save-all]
           ["Ctrl+Slash" :toggle-comment]
           ["Ctrl+Space" :proposals]
           ["Ctrl+T" :scene-stop]
           ["Ctrl+U" :rebundle]
           ["Ctrl+V" :paste]
           ["Ctrl+W" :close]
           ["Ctrl+X" :cut]
           ["Ctrl+Z" :undo]
           ["Delete" :delete]
           ["Down" :down]
           ["E" :rotate-tool]
           ["End" :end-of-line]
           ["Enter" :enter]
           ["Esc" :escape]
           ["F" :frame-selection]
           ["F1" :documentation]
           ["F10" :step-over]
           ["F11" :step-into]
           ["F2" :rename]
           ["F5" :start-debugger]
           ["F5" :continue]
           ["F6" :toggle-pane-left]
           ["F7" :toggle-pane-bottom]
           ["F8" :toggle-pane-right]
           ["F9" :toggle-breakpoint]
           ["Home" :beginning-of-line-text]
           ["Left" :left]
           ["Page Down" :page-down]
           ["Page Up" :page-up]
           ["Period" :realign-camera]
           ["R" :scale-tool]
           ["Right" :right]
           ["Shift+A" :add-secondary]
           ["Shift+Backspace" :delete-backward]
           ["Shift+Ctrl+B" :rebuild]
           ["Shift+Ctrl+Delete" :delete-to-end-of-line]
           ["Shift+Ctrl+E" :show-last-hidden]
           ["Shift+Ctrl+End" :select-end-of-file]
           ["Shift+Ctrl+F" :search-in-files]
           ["Shift+Ctrl+G" :find-prev]
           ["Shift+Ctrl+H" :replace-next]
           ["Shift+Ctrl+Home" :select-beginning-of-file]
           ["Shift+Ctrl+I" :toggle-visibility-filters]
           ["Shift+Ctrl+L" :split-selection-into-lines]
           ["Shift+Ctrl+Left" :select-prev-word]
           ["Shift+Ctrl+R" :open-asset]
           ["Shift+Ctrl+Right" :select-next-word]
           ["Shift+Ctrl+W" :close-all]
           ["Shift+Ctrl+Z" :redo]
           ["Shift+Down" :down-major]
           ["Shift+Down" :select-down]
           ["Shift+E" :erase-tool]
           ["Shift+End" :select-end-of-line]
           ["Shift+F11" :step-out]
           ["Shift+F5" :stop-debugger]
           ["Shift+Home" :select-beginning-of-line-text]
           ["Shift+Left" :left-major]
           ["Shift+Left" :select-left]
           ["Shift+Page Down" :select-page-down]
           ["Shift+Page Up" :select-page-up]
           ["Shift+Right" :right-major]
           ["Shift+Right" :select-right]
           ["Shift+Tab" :backwards-tab-trigger]
           ["Shift+Up" :select-up]
           ["Shift+Up" :up-major]
           ["Space" :scene-play]
           ["Space" :show-palette]
           ["Tab" :tab]
           ["Up" :up]
           ["W" :move-tool]]
   :linux [["A" :add]
           ["Alt+Down" :move-down]
           ["Alt+Up" :move-up]
           ["Backspace" :delete-backward]
           ["Ctrl+'+'" :zoom-in]
           ["Ctrl+'-'" :zoom-out]
           ["Ctrl+A" :select-all]
           ["Ctrl+B" :build]
           ["Ctrl+Backspace" :delete-prev-word]
           ["Ctrl+C" :copy]
           ["Ctrl+Comma" :preferences]
           ["Ctrl+D" :select-next-occurrence]
           ["Ctrl+Delete" :delete-next-word]
           ["Ctrl+E" :hide-selected]
           ["Ctrl+End" :end-of-file]
           ["Ctrl+F" :filter-form]
           ["Ctrl+F" :find-text]
           ["Ctrl+G" :find-next]
           ["Ctrl+H" :replace-text]
           ["Ctrl+H" :toggle-component-guides]
           ["Ctrl+Home" :beginning-of-file]
           ["Ctrl+I" :reindent]
           ["Ctrl+K" :delete-to-end-of-line]
           ["Ctrl+L" :goto-line]
           ["Ctrl+Left" :prev-word]
           ["Ctrl+N" :new-file]
           ["Ctrl+O" :open]
           ["Ctrl+P" :open-asset]
           ["Ctrl+Q" :quit]
           ["Ctrl+R" :hot-reload]
           ["Ctrl+Right" :next-word]
           ["Ctrl+S" :save-all]
           ["Ctrl+Slash" :toggle-comment]
           ["Ctrl+Space" :proposals]
           ["Ctrl+T" :scene-stop]
           ["Ctrl+U" :rebundle]
           ["Ctrl+V" :paste]
           ["Ctrl+W" :close]
           ["Ctrl+X" :cut]
           ["Ctrl+Z" :undo]
           ["Delete" :delete]
           ["Down" :down]
           ["E" :rotate-tool]
           ["End" :end-of-line]
           ["Enter" :enter]
           ["Esc" :escape]
           ["F" :frame-selection]
           ["F1" :documentation]
           ["F10" :step-over]
           ["F11" :step-into]
           ["F2" :rename]
           ["F5" :start-debugger]
           ["F5" :continue]
           ["F6" :toggle-pane-left]
           ["F7" :toggle-pane-bottom]
           ["F8" :toggle-pane-right]
           ["F9" :toggle-breakpoint]
           ["Home" :beginning-of-line-text]
           ["Left" :left]
           ["Page Down" :page-down]
           ["Page Up" :page-up]
           ["Period" :realign-camera]
           ["R" :scale-tool]
           ["Right" :right]
           ["Shift+A" :add-secondary]
           ["Shift+Backspace" :delete-backward]
           ["Shift+Ctrl+B" :rebuild]
           ["Shift+Ctrl+Delete" :delete-to-end-of-line]
           ["Shift+Ctrl+E" :show-last-hidden]
           ["Shift+Ctrl+End" :select-end-of-file]
           ["Shift+Ctrl+F" :search-in-files]
           ["Shift+Ctrl+G" :find-prev]
           ["Shift+Ctrl+H" :replace-next]
           ["Shift+Ctrl+Home" :select-beginning-of-file]
           ["Shift+Ctrl+I" :toggle-visibility-filters]
           ["Shift+Ctrl+L" :split-selection-into-lines]
           ["Shift+Ctrl+Left" :select-prev-word]
           ["Shift+Ctrl+R" :open-asset]
           ["Shift+Ctrl+Right" :select-next-word]
           ["Shift+Ctrl+W" :close-all]
           ["Shift+Ctrl+Z" :redo]
           ["Shift+Down" :down-major]
           ["Shift+Down" :select-down]
           ["Shift+E" :erase-tool]
           ["Shift+End" :select-end-of-line]
           ["Shift+F11" :step-out]
           ["Shift+F5" :stop-debugger]
           ["Shift+Home" :select-beginning-of-line-text]
           ["Shift+Left" :left-major]
           ["Shift+Left" :select-left]
           ["Shift+Page Down" :select-page-down]
           ["Shift+Page Up" :select-page-up]
           ["Shift+Right" :right-major]
           ["Shift+Right" :select-right]
           ["Shift+Tab" :backwards-tab-trigger]
           ["Shift+Up" :select-up]
           ["Shift+Up" :up-major]
           ["Space" :scene-play]
           ["Space" :show-palette]
           ["Tab" :tab]
           ["Up" :up]
           ["W" :move-tool]]})

(def default-host-key-bindings
  (platform->default-key-bindings (util/os)))

(def ^:private default-allowed-duplicate-shortcuts
  #{"Alt+Down"
    "Alt+Up"
    "Ctrl+F"
    "Ctrl+H"
    "F5"
    "Meta+F"
    "Shift+Down"
    "Shift+Left"
    "Shift+Right"
    "Shift+Up"
    "Space"})

;; These are only (?) used in contexts where there is no text field
;; interested in the actual typable input.
(def ^:private default-allowed-typable-shortcuts
  #{"A"         ; :add
    "E"         ; :rotate-tool
    "F"         ; :frame-selection
    "R"         ; :scale-tool
    "W"         ; :move-tool
    "Shift+A"   ; :add-secondary
    "Shift+E"}) ; :erase-tool

(defprotocol KeyComboData
  (key-combo->map [this] "returns a data representation of a KeyCombination."))

(extend-protocol KeyComboData
  KeyCodeCombination
  (key-combo->map [key-combo]
    {:key (.getName (.getCode key-combo))
     :alt-down? (= KeyCombination$ModifierValue/DOWN (.getAlt key-combo))
     :control-down? (= KeyCombination$ModifierValue/DOWN (.getControl key-combo))
     :meta-down? (= KeyCombination$ModifierValue/DOWN (.getMeta key-combo))
     :shift-down? (= KeyCombination$ModifierValue/DOWN (.getShift key-combo))
     :shortcut-down? (= KeyCombination$ModifierValue/DOWN (.getShortcut key-combo))})

  KeyCharacterCombination
  (key-combo->map [key-combo]
    {:key (.getCharacter key-combo)
     :alt-down? (= KeyCombination$ModifierValue/DOWN (.getAlt key-combo))
     :control-down? (= KeyCombination$ModifierValue/DOWN (.getControl key-combo))
     :meta-down? (= KeyCombination$ModifierValue/DOWN (.getMeta key-combo))
     :shift-down? (= KeyCombination$ModifierValue/DOWN (.getShift key-combo))
     :shortcut-down? (= KeyCombination$ModifierValue/DOWN (.getShortcut key-combo))}))

(defn key-event->map [^KeyEvent e]
  {:key              (.getCharacter e)
   :alt-down?        (.isAltDown e)
   :control-down?    (.isControlDown e)
   :meta-down?       (.isMetaDown e)
   :shift-down?      (.isShiftDown e)
   :shortcut-down?   (.isShortcutDown e)})

(defn key-combo->display-text [s]
  (.getDisplayText (KeyCombination/keyCombination s)))

(def ^:private typable-truth-table
  "Only act on key pressed events that look like textual input, and
  skip what is likely shortcut combinations.

  Transcribed, and altered, from javafx TextInputControlBehaviour/defaultKeyTyped. See:

  http://grepcode.com/file/repo1.maven.org/maven2/net.java.openjfx.backport/openjfx-78-backport/1.8.0-ea-b96.1/com/sun/javafx/scene/control/behavior/TextInputControlBehavior.java#TextInputControlBehavior.defaultKeyTyped%28javafx.scene.input.KeyEvent%29

  Original:

        if (event.isControlDown() || event.isAltDown() || (isMac() && event.isMetaDown())) {
            if (!((event.isControlDown() || isMac()) && event.isAltDown())) return;
        }

  Transcribed:

        (not (and (or control-down? alt-down? (and mac? meta-down?))
                  (not (and (or control-down? mac?) alt-down?))))

  Simplified:

    (or (and control-down? alt-down?)
        (and mac? alt-down?)
        (and (not mac?) (not control-down?) (not alt-down?))
        (and (not control-down?) (not alt-down?) (not meta-down?)))

  Truth table:

  MAC  CTRL ALT  META    RESULT
  no   no   no   no   => typable
  no   no   no   yes  => typable  -- Haven't been able to repro META on non-mac, assume not typable if even possible
  no   no   yes  no   => shortcut
  no   no   yes  yes  => shortcut
  no   yes  no   no   => shortcut
  no   yes  no   yes  => shortcut
  no   yes  yes  no   => typable
  no   yes  yes  yes  => typable  -- As above
  yes  no   no   no   => typable
  yes  no   no   yes  => shortcut
  yes  no   yes  no   => typable
  yes  no   yes  yes  => typable  -- No other tested mac app reacts like this, but seems you can use cmd (Meta) as modifier in keyboard input sources
  yes  yes  no   no   => shortcut
  yes  yes  no   yes  => shortcut
  yes  yes  yes  no   => typable  -- As above
  yes  yes  yes  yes  => typable  -- As above

  This does not seem right. We probably want something like that:

    MAC   CTRL  ALT   META  RESULT"
  {[:no   :no   :no   :no ] :typable
   [:no   :no   :no   :yes] :shortcut   ;; Now treated as possibly shortcut
   [:no   :no   :yes  :no ] :shortcut
   [:no   :no   :yes  :yes] :shortcut
   [:no   :yes  :no   :no ] :shortcut
   [:no   :yes  :no   :yes] :shortcut
   [:no   :yes  :yes  :no ] :typable
   [:no   :yes  :yes  :yes] :shortcut   ;; As above
   [:yes  :no   :no   :no ] :typable
   [:yes  :no   :no   :yes] :shortcut
   [:yes  :no   :yes  :no ] :typable
   [:yes  :no   :yes  :yes] :shortcut   ;; Now treated as possibly shortcut
   [:yes  :yes  :no   :no ] :shortcut
   [:yes  :yes  :no   :yes] :shortcut
   [:yes  :yes  :yes  :no ] :typable    ;; As above
   [:yes  :yes  :yes  :yes] :shortcut}) ;; Now treated as possibly shortcut

(defn- boolean->kw [b]
  (if b :yes :no))

(defn typable?
  ([key-combo-data]
   (typable? key-combo-data (util/os)))
  ([{:keys [alt-down? control-down? meta-down?]} os]
   (let [mac? (= os :macos)]
     (-> typable-truth-table
         (get (mapv boolean->kw [mac? control-down? alt-down? meta-down?]))
         (= :typable)))))

(defn- platform-typable-shortcut? [key-combo-data os]
  (and (typable? key-combo-data os)
       (= 1 (count (:key key-combo-data)))))

(defn- key-binding-data [key-bindings]
  (map (fn [[shortcut command]]
         (let [key-combo (KeyCombination/keyCombination shortcut)
               key-combo-data (key-combo->map key-combo)]
           {:shortcut shortcut
            :command command
            :key-combo key-combo
            :key-combo-data key-combo-data}))
       key-bindings))

(defn- key-binding-data->keymap
  [key-bindings-data platform valid-command? allowed-duplicate-shortcuts allowed-typable-shortcuts]
  (reduce (fn [ret {:keys [key-combo-data shortcut command ^KeyCombination key-combo]}]
            (cond
              (not (valid-command? command))
              (update ret :errors conj {:type :unknown-command
                                        :command command
                                        :shortcut shortcut})

              (not= shortcut (.getName key-combo))
              (update ret :errors conj {:type :unknown-shortcut
                                        :command command
                                        :shortcut shortcut})

              (:shortcut-down? key-combo-data)
              (update ret :errors conj {:type :shortcut-key
                                        :command command
                                        :shortcut shortcut})

              (and (platform-typable-shortcut? key-combo-data platform)
                   (not (allowed-typable-shortcuts shortcut)))
              (update ret :errors conj {:type :typable-shortcut
                                        :command command
                                        :shortcut shortcut})

              (and (some? (get-in ret [:keymap key-combo-data]))
                   (not (allowed-duplicate-shortcuts shortcut)))
              (update ret :errors into [{:type :duplicate-shortcut
                                         :command command
                                         :shortcut shortcut}
                                        {:type :duplicate-shortcut
                                         :command (get-in ret [:keymap key-combo-data])
                                         :shortcut shortcut}])

              :else
              (update-in ret [:keymap key-combo-data] (fnil conj []) {:command command
                                                                      :shortcut shortcut
                                                                      :key-combo key-combo})))
          {:keymap {}
           :errors #{}}
          key-bindings-data))

(defn- make-keymap*
  [key-bindings platform valid-command? allowed-duplicate-shortcuts allowed-typable-shortcuts]
  (-> (key-binding-data key-bindings)
      (key-binding-data->keymap platform valid-command? allowed-duplicate-shortcuts allowed-typable-shortcuts)))

(defn make-keymap
  ([key-bindings]
   (make-keymap key-bindings nil))
  ([key-bindings {:keys [valid-command?
                         platform
                         throw-on-error?
                         allowed-duplicate-shortcuts
                         allowed-typable-shortcuts]
                  :or   {valid-command?              (constantly true)
                         platform                    (util/os)
                         throw-on-error?             false
                         allowed-duplicate-shortcuts default-allowed-duplicate-shortcuts
                         allowed-typable-shortcuts   default-allowed-typable-shortcuts}}]
   (let [{:keys [errors keymap]} (make-keymap* key-bindings platform valid-command? allowed-duplicate-shortcuts allowed-typable-shortcuts)]
     (if (and (seq errors) throw-on-error?)
       (throw (ex-info "Keymap has errors"
                       {:errors       errors
                        :key-bindings key-bindings}))
       keymap))))

(defn command->shortcut
  [keymap]
  (into {}
        (comp cat
              (map (fn [{:keys [command shortcut]}]
                     [command shortcut])))
        (vals keymap)))

(defn- execute-command
  [commands]
  ;; It is imperative that the handler is invoked using run-later as
  ;; this avoids a JVM crash on some macOS versions. Prior to macOS
  ;; Sierra, the order in which native menu events are delivered
  ;; triggered a segfault in the native menu implementation when the
  ;; stage is changed during the event dispatch. This happens for
  ;; example when we have a shortcut triggering the opening of a
  ;; dialog.
  (ui/run-later (let [command-contexts (ui/contexts (ui/main-scene))]
                  (loop [[command & rest] commands]
                    (when (some? command)
                      (let [ret (ui/invoke-handler command-contexts command)]
                        (if (or (= ret ::ui/not-active)
                                (= ret ::ui/not-enabled))
                          (recur rest)
                          ret)))))))

(defn install-key-bindings!
  [^Scene scene keymap]
  (let [accelerators (.getAccelerators scene)]
    (run! (fn [[_ commands]]
            (let [key-combo (-> commands first :key-combo)]
              (.put accelerators key-combo #(execute-command (map :command commands)))))
          keymap)))
