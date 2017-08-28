(ns editor.keymap
  (:require
   [editor.ui :as ui])
  (:import
   (com.defold.editor Platform)
   (javafx.scene Scene)
   (javafx.scene.input KeyCombination KeyCombination$ModifierValue)))

(set! *warn-on-reflection* true)

(def host-platform-shortcut-key
  (case (.. Platform getHostPlatform getOs)
    "darwin" :meta-down?
    "win32"  :control-down?
    "linux"  :control-down?))

(def default-key-bindings
  [["A"                     :add]
   ["Alt+Backspace"         :delete-prev-word]
   ["Alt+Delete"            :delete-next-word]
   ["Alt+Down"              :end-of-line]
   ["Alt+Down"              :move-down]
   ["Alt+Left"              :prev-word]
   ["Alt+Right"             :next-word]
   ["Alt+Shortcut+E"        :replace-next]
   ["Alt+Shortcut+X"        :profile]
   ["Alt+Up"                :beginning-of-line]
   ["Alt+Up"                :move-up]
   ["Backspace"             :delete-backward]
   ["Ctrl+A"                :beginning-of-line]
   ["Ctrl+D"                :delete-line]
   ["Ctrl+E"                :end-of-line]
   ["Ctrl+K"                :cut-to-end-of-line]
   ["Ctrl+Left"             :prev-word]
   ["Ctrl+Right"            :next-word]
   ["Ctrl+Space"            :proposals]
   ["Delete"                :delete]
   ["Down"                  :down]
   ["E"                     :rotate-tool]
   ["End"                   :end-of-line]
   ["Enter"                 :enter]
   ["Esc"                   :escape]
   ["F"                     :frame-selection]
   ["F1"                    :documentation]
   ["F10"                   :step-over]
   ["F11"                   :step-into]
   ["Shift+F11"             :step-out]
   ["F5"                    :start-debugger]
   ["F5"                    :break]
   ["F5"                    :continue]
   ["Shift+F5"              :stop-debugger]
   ["Ctrl+R"                :reload-stylesheet]
   ["F9"                    :toggle-breakpoint]
   ["Home"                  :beginning-of-line-text]
   ["Left"                  :left]
   ["Page Down"             :page-down]
   ["Page Up"               :page-up]
   ["Period"                :realign-camera]
   ["R"                     :scale-tool]
   ["Right"                 :right]
   ["Shift+A"               :add-secondary]
   ["Shift+Alt+Down"        :select-end-of-line]
   ["Shift+Alt+Left"        :select-prev-word]
   ["Shift+Alt+Right"       :select-next-word]
   ["Shift+Alt+Up"          :select-beginning-of-line]
   ["Shift+Alt+Shortcut+X"  :profile-show]
   ["Shift+Ctrl+A"          :select-beginning-of-line]
   ["Shift+Ctrl+E"          :select-end-of-line]
   ["Shift+Ctrl+Left"       :select-prev-word]
   ["Shift+Ctrl+Right"      :select-next-word]
   ["Shift+Down"            :down-major]
   ["Shift+Down"            :select-down]
   ["Shift+E"               :erase-tool]
   ["Shift+End"             :select-end-of-line]
   ["Shift+Home"            :select-beginning-of-line-text]
   ["Shift+Left"            :left-major]
   ["Shift+Left"            :select-left]
   ["Shift+Page Down"       :select-page-down]
   ["Shift+Page Up"         :select-page-up]
   ["Shift+Right"           :right-major]
   ["Shift+Right"           :select-right]
   ["Shift+Shortcut+B"      :rebuild]
   ["Shift+Shortcut+Delete" :delete-to-end-of-line]
   ["Shift+Shortcut+Down"   :select-end-of-file]
   ["Shift+Shortcut+End"    :select-end-of-file]
   ["Shift+Shortcut+F"      :search-in-files]
   ["Shift+Shortcut+G"      :find-prev]
   ["Shift+Shortcut+Home"   :select-beginning-of-file]
   ["Shift+Shortcut+L"      :split-selection-into-lines]
   ["Shift+Shortcut+Left"   :select-beginning-of-line-text]
   ["Shift+Shortcut+R"      :open-asset]
   ["Shift+Shortcut+Right"  :select-end-of-line]
   ["Shift+Shortcut+Up"     :select-beginning-of-file]
   ["Shift+Shortcut+W"      :close-all]
   ["Shift+Shortcut+Z"      :redo]
   ["Shift+Tab"             :backwards-tab-trigger]
   ["Shift+Up"              :up-major]
   ["Shift+Up"              :select-up]
   ["Shortcut+A"            :select-all]
   ["Shortcut+B"            :build]
   ["Shortcut+C"            :copy]
   ["Shortcut+Comma"        :preferences]
   ["Shortcut+D"            :select-next-occurrence]
   ["Shortcut+Delete"       :delete-to-beginning-of-line]
   ["Shortcut+Down"         :end-of-file]
   ["Shortcut+E"            :replace-text]
   ["Shortcut+End"          :end-of-file]
   ["Shortcut+F"            :find-text]
   ["Shortcut+G"            :find-next]
   ["Shortcut+Home"         :beginning-of-file]
   ["Shortcut+I"            :reindent]
   ["Shortcut+L"            :goto-line]
   ["Shortcut+Left"         :beginning-of-line-text]
   ["Shortcut+N"            :new-file]
   ["Shortcut+O"            :open]
   ["Shortcut+P"            :open-asset]
   ["Shortcut+Q"            :quit]
   ["Shortcut+R"            :hot-reload]
   ["Shortcut+Right"        :end-of-line]
   ["Shortcut+S"            :save-all]
   ["Shortcut+Slash"        :toggle-comment]
   ["Shortcut+T"            :scene-stop]
   ["Shortcut+U"            :synchronize]
   ["Shortcut+Up"           :beginning-of-file]
   ["Shortcut+V"            :paste]
   ["Shortcut+W"            :close]
   ["Shortcut+X"            :cut]
   ["Shortcut+Z"            :undo]
   ["Space"                 :scene-play]
   ["Space"                 :show-palette]
   ["Tab"                   :tab]
   ["Up"                    :up]
   ["W"                     :move-tool]])

(def ^:private allowed-duplicate-shortcuts #{"Alt+Down"
                                             "Alt+Up"
                                             "Shift+Down"
                                             "Shift+Left"
                                             "Shift+Right"
                                             "Shift+Up"
                                             "Space"
                                             "F5"})

(defprotocol KeyComboData
  (key-combo->map* [this] "returns a data representation of a KeyCombination."))

(extend-protocol KeyComboData
  javafx.scene.input.KeyCodeCombination
  (key-combo->map* [key-combo]
    {:key            (.. key-combo getCode getName)
     :alt-down?      (= KeyCombination$ModifierValue/DOWN (.getAlt key-combo))
     :control-down?  (= KeyCombination$ModifierValue/DOWN (.getControl key-combo))
     :meta-down?     (= KeyCombination$ModifierValue/DOWN (.getMeta key-combo))
     :shift-down?    (= KeyCombination$ModifierValue/DOWN (.getShift key-combo))
     :shortcut-down? (= KeyCombination$ModifierValue/DOWN (.getShortcut key-combo))})

  javafx.scene.input.KeyCharacterCombination
  (key-combo->map* [key-combo]
    {:key            (.getCharacter key-combo)
     :alt-down?      (= KeyCombination$ModifierValue/DOWN (.getAlt key-combo))
     :control-down?  (= KeyCombination$ModifierValue/DOWN (.getControl key-combo))
     :meta-down?     (= KeyCombination$ModifierValue/DOWN (.getMeta key-combo))
     :shift-down?    (= KeyCombination$ModifierValue/DOWN (.getShift key-combo))
     :shortcut-down? (= KeyCombination$ModifierValue/DOWN (.getShortcut key-combo))})  )

(defn- key-combo->map [s]
  (let [key-combo (KeyCombination/keyCombination s)]
    (key-combo->map* key-combo)))

(defn- key-binding-data
  [key-bindings platform-shortcut-key]
  (map (fn [[shortcut command]]
         (let [key-combo (KeyCombination/keyCombination shortcut)
               key-combo-data (key-combo->map shortcut)
               canonical-key-combo-data (-> key-combo-data
                                            (update platform-shortcut-key #(or % (:shortcut-down? key-combo-data)))
                                            (dissoc :shortcut-down?))]
           {:shortcut shortcut
            :command command
            :key-combo key-combo
            :key-combo-data key-combo-data
            :canonical-key-combo-data canonical-key-combo-data}))
       key-bindings))

(defn- remove-shortcut-key-overlaps
  "Given a sequence of key-binding data, find all bindings where the modifiers
  used are in conflict with the `Shortcut` key, and remove them. For example, on
  platforms where the shortcut key is `Ctrl`, the bindings `Ctrl+A` and
  `Shortcut+A` are in conflict as they would resolve to the same binding. In
  such cases, we prefer the binding with the `Shortcut` key."
  [key-bindings-data]
  (->> (group-by :canonical-key-combo-data key-bindings-data)
       (vals)
       (mapcat (fn [overlapping-key-bindings]
                 (or (seq (filter (comp :shortcut-down? :key-combo-data) overlapping-key-bindings))
                     overlapping-key-bindings)))))

(defn- key-binding-data->keymap
  [key-bindings-data valid-command?]
  (reduce (fn [ret {:keys [canonical-key-combo-data shortcut command ^KeyCombination key-combo]}]
            (cond
              (not (valid-command? command))
              (update ret :errors conj {:type :unknown-command
                                        :command command
                                        :shortcut shortcut})

              (not= shortcut (.getName key-combo))
              (update ret :errors conj {:type :unknown-shortcut
                                        :command command
                                        :shortcut shortcut})

              (and (some? (get-in ret [:keymap canonical-key-combo-data]))
                   (not (allowed-duplicate-shortcuts shortcut)))
              (update ret :errors into [{:type :duplicate-shortcut
                                         :command command
                                         :shortcut shortcut}
                                        {:type :duplicate-shortcut
                                         :command (get-in ret [:keymap canonical-key-combo-data])
                                         :shortcut shortcut}])

              :else
              (update-in ret [:keymap canonical-key-combo-data] (fnil conj []) {:command command
                                                                                :shortcut shortcut
                                                                                :key-combo key-combo})))
          {:keymap {}
           :errors #{}}
          key-bindings-data))

(defn- make-keymap*
  [key-bindings platform-shortcut-key valid-command?]
  (-> (key-binding-data key-bindings platform-shortcut-key)
      (remove-shortcut-key-overlaps)
      (key-binding-data->keymap valid-command?)))

(defn make-keymap
  ([key-bindings]
   (make-keymap key-bindings nil))
  ([key-bindings {:keys [valid-command?
                         platform-shortcut-key
                         throw-on-error?]
                  :or   {valid-command?        (constantly true)
                         platform-shortcut-key host-platform-shortcut-key
                         throw-on-error?       false}
                  :as   opts}]
   (let [{:keys [errors keymap]} (make-keymap* key-bindings platform-shortcut-key valid-command?)]
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
                        (if (= ret :editor.ui/not-active)
                          (recur rest)
                          ret)))))))

(defn install-key-bindings!
  [^Scene scene keymap]
  (let [accelerators (.getAccelerators scene)]
    (run! (fn [[_ commands]]
            (let [key-combo (-> commands first :key-combo)]
              (.put accelerators key-combo #(execute-command (map :command commands)))))
          keymap)))
