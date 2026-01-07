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

(ns editor.debug-view
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code.data]
            [editor.console :as console]
            [editor.core :as core]
            [editor.debugging.mobdebug :as mobdebug]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.engine :as engine]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.lua :as lua]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [com.dynamo.lua.proto Lua$LuaModule]
           [editor.debugging.mobdebug LuaStructure]
           [java.nio.file Files]
           [java.util Collection]
           [javafx.scene Parent]
           [javafx.scene.control Button Label ListView TextField TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout HBox Pane Priority]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private break-label (localization/message "command.debugger.break"))
(def ^:private continue-label (localization/message "command.debugger.continue"))
(def ^:private step-into-label (localization/message "command.debugger.step-into"))
(def ^:private step-out-label (localization/message "command.debugger.step-out"))
(def ^:private step-over-label (localization/message "command.debugger.step-over"))
(def ^:private stop-debugger-label (localization/message "command.debugger.stop"))

(defn- single [coll]
  (when (nil? (next coll)) (first coll)))

(defn- set-debugger-data-visible!
  [^Parent right-pane visible?]
  (ui/with-controls right-pane [^Parent right-split ^Parent debugger-data-split]
    (let [was-visible? (.isVisible debugger-data-split)]
      (when (not= was-visible? visible?)
        (ui/visible! right-split (not visible?))
        (ui/visible! debugger-data-split visible?)))))

(defn- debuggable-resource? [resource]
  (boolean (some-> resource resource/resource-type :tags (contains? :debuggable))))

(g/defnk update-available-controls!
  [active-resource ^Parent console-grid-pane debug-session right-pane suspension-state]
  (let [frames (:stack suspension-state)
        suspended? (some? frames)
        resource-debuggable? (debuggable-resource? active-resource)
        debug-session? (some? debug-session)]
    (set-debugger-data-visible! right-pane (and debug-session? resource-debuggable? suspended?))
    (ui/with-controls console-grid-pane [debugger-prompt debugger-tool-bar]
      (ui/visible! debugger-prompt (and debug-session? suspended?))
      (ui/visible! debugger-tool-bar debug-session?))
    (when debug-session?
      (ui/with-controls console-grid-pane [debugger-prompt-field pause-debugger-button play-debugger-button step-in-debugger-button step-out-debugger-button step-over-debugger-button]
        (ui/visible! pause-debugger-button (not suspended?))
        (ui/visible! play-debugger-button suspended?)
        (ui/enable! pause-debugger-button (not suspended?))
        (ui/enable! play-debugger-button suspended?)
        (ui/enable! step-in-debugger-button suspended?)
        (ui/enable! step-out-debugger-button suspended?)
        (ui/enable! step-over-debugger-button suspended?)
        (ui/enable! debugger-prompt-field suspended?)))
    (.layout console-grid-pane)))

(g/defnk update-call-stack!
  [debug-session suspension-state right-pane]
  ;; NOTE: This should only depend upon stuff that changes due to a state change
  ;; in the debugger, since selecting the top-frame below will open the suspended
  ;; file and line in the editor.
  (when (some? debug-session)
    (ui/with-controls right-pane [^ListView debugger-call-stack]
      (let [frames (:stack suspension-state)
            suspended? (some? frames)
            items (.getItems debugger-call-stack)]
        (if suspended?
          (do
            (.setAll items ^Collection frames)
            (when-some [top-frame (first frames)]
              (ui/select! debugger-call-stack top-frame)))
          (.clear items))))))

(g/defnk produce-execution-locations
  [suspension-state]
  (when-some [frames (-> suspension-state :stack)]
    (into []
          (map-indexed (fn [i frame]
                         (-> frame
                             (select-keys [:file :line])
                             (assoc :type (if (zero? i) :current-line :current-frame)))))
          frames)))

(g/deftype History [String])

(g/defnode DebugView
  (inherits core/Scope)

  (property open-resource-fn g/Any)
  (property state-changed-fn g/Any)

  (property debug-session g/Any)
  (property suspension-state g/Any)

  (property console-grid-pane Parent)
  (property right-pane Parent)

  (property evaluation-history History (default []))
  (property evaluation-history-index g/Int)
  (property evaluation-stashed-entry-text g/Str)

  (input active-resource resource/Resource)

  (output update-available-controls g/Any :cached update-available-controls!)
  (output update-call-stack g/Any :cached update-call-stack!)
  (output execution-locations g/Any :cached produce-execution-locations))

(defn- current-stack-frame
  [debug-view]
  (let [right-pane (g/node-value debug-view :right-pane)]
    (ui/with-controls right-pane [^ListView debugger-call-stack]
      (first (.. debugger-call-stack getSelectionModel getSelectedIndices)))))

(defn- sanitize-eval-error [error-string]
  (let [error-line-pattern ":1: "
        i (string/index-of error-string error-line-pattern)
        displayed-error (if (some? i)
                          (subs error-string (+ i (count error-line-pattern)))
                          error-string)]
    (str "ERROR:EVAL: " displayed-error)))

(defn- eval-result->lines [result]
  (->> result
       vals
       (map mobdebug/lua-value->structure-string)
       (string/join "\n")
       string/split-lines))

(defn- on-eval-input
  [debug-view code]
  (when-some [debug-session (g/node-value debug-view :debug-session)]
    (let [frame (current-stack-frame debug-view)]
      (assert (= :suspended (mobdebug/state debug-session)))
      (console/append-console-entry! :eval-expression code)
      (future
        (let [ret (mobdebug/exec debug-session code frame)]
          (cond
            (= :bad-request (:error ret))
            (console/append-console-entry! :eval-error "Bad request")

            (string? (:error ret))
            (console/append-console-entry! :eval-error (sanitize-eval-error (:error ret)))

            (:result ret)
            (doseq [line (eval-result->lines (:result ret))]
              (console/append-console-entry! :eval-result line))

            :else
            (console/append-console-entry! :eval-error (str ret))))))))

(defn- setup-tool-bar!
  [^Parent console-tool-bar localization]
  (ui/with-controls console-tool-bar [^Parent debugger-tool-bar ^Button pause-debugger-button ^Button play-debugger-button step-in-debugger-button step-out-debugger-button step-over-debugger-button stop-debugger-button]
    (.bind (.managedProperty debugger-tool-bar) (.visibleProperty debugger-tool-bar))
    (.bind (.managedProperty pause-debugger-button) (.visibleProperty pause-debugger-button))
    (.bind (.managedProperty play-debugger-button) (.visibleProperty play-debugger-button))
    (ui/tooltip! pause-debugger-button break-label localization)
    (ui/tooltip! play-debugger-button continue-label localization)
    (ui/tooltip! step-in-debugger-button step-into-label localization)
    (ui/tooltip! step-out-debugger-button step-out-label localization)
    (ui/tooltip! step-over-debugger-button step-over-label localization)
    (ui/tooltip! stop-debugger-button stop-debugger-label localization)
    (ui/bind-action! pause-debugger-button :debugger.break)
    (ui/bind-action! play-debugger-button :debugger.continue)
    (ui/bind-action! step-in-debugger-button :debugger.step-into)
    (ui/bind-action! step-out-debugger-button :debugger.step-out)
    (ui/bind-action! step-over-debugger-button :debugger.step-over)
    (ui/bind-action! stop-debugger-button :debugger.stop)))

(defn- setup-call-stack-view!
  [^ListView debugger-call-stack]
  (doto debugger-call-stack
    (ui/cell-factory! (fn [{:keys [function file line]}]
                        (let [path (FilenameUtils/getFullPath file)
                              name (FilenameUtils/getName file)]
                          {:graphic (doto (HBox.)
                                      (ui/children! [(doto (Label. (str function))
                                                       (ui/add-style! "call-stack-function"))
                                                     (doto (Pane.)
                                                       (HBox/setHgrow Priority/ALWAYS)
                                                       (ui/fill-control))
                                                     (doto (Label. path)
                                                       (ui/add-style! "call-stack-path"))
                                                     (doto (Label. name)
                                                       (ui/add-style! "call-stack-file"))
                                                     (doto (HBox.)
                                                       (ui/add-style! "call-stack-line-container")
                                                       (ui/children! [(doto (Label. (str line))
                                                                        (ui/add-style! "call-stack-line"))]))]))})))))

(defn- setup-variables-view!
  [^TreeView debugger-variables]
  (doto debugger-variables
    (ui/customize-tree-view! {:double-click-expand? true})
    (.setShowRoot false)
    (ui/cell-factory! (fn [{:keys [display-name display-value]}]
                        {:graphic (doto (HBox.)
                                    (ui/fill-control)
                                    (ui/children! [(Label. display-name)
                                                   (doto (Label. " = ")
                                                     (ui/add-style! "equals"))
                                                   (Label. display-value)]))}))))

(defn- make-variable-tree-item
  [[name value]]
  (let [variable {:name name
                  :display-name (mobdebug/lua-value->identity-string name)
                  :value value
                  :display-value (mobdebug/lua-value->identity-string value)}
        tree-item (TreeItem. variable)
        children (.getChildren tree-item)]
    (when (and (instance? LuaStructure value)
               (pos? (count value)))
      (.add children (TreeItem.))
      (ui/observe-once (.expandedProperty tree-item)
                       (fn [_ _ _]
                         (.setAll children ^Collection (map make-variable-tree-item value)))))
    tree-item))

(defn- make-variables-tree-item
  [locals upvalues]
  (let [ret (TreeItem.)
        children (.getChildren ret)]
    (.setAll children ^Collection (map make-variable-tree-item (concat locals upvalues)))
    ret))

(defn- switch-text! [^TextField text-field text]
  (doto text-field
    (.setText text)
    (.end)))

(defn- conj-history-entry [history text]
  (if (or (string/blank? text)
          (= (peek history) text))
    history
    (conj history text)))

(defn- eval-input! [^TextField text-field debug-view ^KeyEvent e]
  (let [text (.getText text-field)]
    (.consume e)
    (switch-text! text-field "")
    (g/transact
      [(g/set-property debug-view :evaluation-history-index nil)
       (g/set-property debug-view :evaluation-stashed-entry-text nil)
       (g/update-property debug-view :evaluation-history conj-history-entry text)])
    (on-eval-input debug-view text)))

(defn- prev-history-entry! [^TextField text-field debug-view ^KeyEvent e]
  (g/with-auto-evaluation-context ec
    (let [history (g/node-value debug-view :evaluation-history ec)
          history-index (g/node-value debug-view :evaluation-history-index ec)]
      (cond
        (empty? history)
        nil

        (nil? history-index)
        (let [index (dec (count history))
              text (.getText text-field)]
          (.consume e)
          (switch-text! text-field (history index))
          (g/transact
            [(g/set-property debug-view :evaluation-stashed-entry-text text)
             (g/set-property debug-view :evaluation-history-index index)]))

        (zero? history-index)
        nil

        :else
        (let [index (dec history-index)]
          (.consume e)
          (switch-text! text-field (history index))
          (g/transact
            (g/set-property debug-view :evaluation-history-index index)))))))

(defn- next-history-entry! [^TextField text-field debug-view ^KeyEvent e]
  (g/with-auto-evaluation-context ec
    (let [history (g/node-value debug-view :evaluation-history ec)
          history-index (g/node-value debug-view :evaluation-history-index ec)
          stashed-entry-text (g/node-value debug-view :evaluation-stashed-entry-text ec)]
      (cond
        (or (empty? history) (nil? history-index))
        nil

        (= history-index (dec (count history)))
        (do
          (.consume e)
          (switch-text! text-field stashed-entry-text)
          (g/transact
            [(g/set-property debug-view :evaluation-history-index nil)
             (g/set-property debug-view :evaluation-stashed-entry-text nil)]))

        :else
        (let [index (inc history-index)]
          (.consume e)
          (switch-text! text-field (history index))
          (g/transact
            (g/set-property debug-view :evaluation-history-index index)))))))

(defn setup-prompt-field! [debug-view ^TextField text-field]
  (.addEventFilter text-field KeyEvent/KEY_PRESSED
                   (ui/event-handler e
                     (condp = (.getCode ^KeyEvent e)
                       KeyCode/ENTER (eval-input! text-field debug-view e)
                       KeyCode/UP (prev-history-entry! text-field debug-view e)
                       KeyCode/DOWN (next-history-entry! text-field debug-view e)
                       nil))))

(defn- setup-controls!
  [debug-view ^Parent console-grid-pane ^Parent right-pane localization]
  (ui/with-controls console-grid-pane [console-tool-bar
                                       ^Parent debugger-prompt
                                       debugger-prompt-field]
    ;; tool bar
    (setup-tool-bar! console-tool-bar localization)

    ;; debugger prompt
    (.bind (.managedProperty debugger-prompt) (.visibleProperty debugger-prompt))
    (setup-prompt-field! debug-view debugger-prompt-field))

  (ui/with-controls right-pane [debugger-call-stack
                                ^Parent debugger-data-split
                                ^TreeView debugger-variables
                                ^Parent right-split]
    ;; debugger data views
    (.bind (.managedProperty debugger-data-split) (.visibleProperty debugger-data-split))
    (.bind (.managedProperty right-split) (.visibleProperty right-split))

    ;; call stack
    (setup-call-stack-view! debugger-call-stack)

    (ui/observe-selection debugger-call-stack
                          (fn [node selected-frames]
                            (let [selected-frame (single selected-frames)]
                              (let [{:keys [file line]} selected-frame]
                                (when (and file line)
                                  (let [open-resource-fn (g/node-value debug-view :open-resource-fn)]
                                    (open-resource-fn file line))))
                              (.setRoot debugger-variables (make-variables-tree-item
                                                             (:locals selected-frame)
                                                             (:upvalues selected-frame))))))

    ;; variables
    (setup-variables-view! debugger-variables))

  ;; expose to view node
  (g/set-properties! debug-view :console-grid-pane console-grid-pane :right-pane right-pane)
  nil)

(defn- file-or-module->resource
  [workspace file]
  (some (partial workspace/find-resource workspace) [file (lua/lua-module->path file)]))

(defn- make-open-resource-fn
  [project open-resource-fn]
  (let [workspace (project/workspace project)]
    (fn [file-or-module line]
      (let [resource (file-or-module->resource workspace file-or-module)]
        (when resource
          (open-resource-fn resource {:cursor-range (code.data/line-number->CursorRange line)})))
      nil)))

(defn- collect-enabled-breakpoints [project]
  (into #{}
        (filter :enabled)
        (g/node-value project :breakpoints)))

(defn- set-breakpoint!
  [debug-session {:keys [resource row condition] :as _breakpoint}]
  (when-some [path (resource/proj-path resource)]
    (mobdebug/set-breakpoint! debug-session path (inc row) condition)))

(defn- remove-breakpoint!
  [debug-session {:keys [resource row] :as _breakpoint}]
  (when-some [path (resource/proj-path resource)]
    (mobdebug/remove-breakpoint! debug-session path (inc row))))

(defn- update-breakpoints!
  ([debug-session breakpoints]
   (update-breakpoints! debug-session #{} breakpoints))
  ([debug-session old new]
   (let [added (set/difference new old)
         removed (set/difference old new)]
     (when (or (seq added) (seq removed))
       (mobdebug/with-suspended-session debug-session
         (fn [debug-session]
           (run! #(remove-breakpoint! debug-session %) removed)
           (run! #(set-breakpoint! debug-session %) added)))))))

(defn- make-update-timer
  [project debug-view]
  (let [state   (volatile! {})
        tick-fn (fn [timer _ _]
                  (when-not (ui/ui-disabled?)
                    ;; if we don't have a debug session going on, there is no point in pulling
                    ;; project/breakpoints or updating the "last breakpoints" state.
                    (when-some [debug-session (g/node-value debug-view :debug-session)]
                      (let [breakpoints (collect-enabled-breakpoints project)]
                        (update-breakpoints! debug-session (:breakpoints @state) breakpoints)
                        (vreset! state {:breakpoints breakpoints})))))]
    (ui/->timer 4 "debugger-update-timer" tick-fn)))

(defn- setup-view! [debug-view app-view]
  (g/transact
    [(g/connect app-view :active-resource debug-view :active-resource)
     (g/connect debug-view :execution-locations app-view :debugger-execution-locations)])
  debug-view)

(defn make-view!
  [app-view view-graph project ^Parent root open-resource-fn state-changed-fn localization]
  (let [console-grid-pane (.lookup root "#console-grid-pane")
        right-pane (.lookup root "#right-pane")
        view-id (setup-view! (g/make-node! view-graph DebugView
                                           :open-resource-fn (make-open-resource-fn project open-resource-fn)
                                           :state-changed-fn state-changed-fn)
                             app-view)
        timer (make-update-timer project view-id)]
    (setup-controls! view-id console-grid-pane right-pane localization)
    (ui/timer-start! timer)
    view-id))

(defn- state-changed! [debug-view attention?]
  (let [state-changed-fn (g/node-value debug-view :state-changed-fn)]
    (state-changed-fn attention?)))

(defn- update-suspension-state!
  [debug-view debug-session]
  (let [stack (mobdebug/stack debug-session)]
    (g/set-property! debug-view
                     :suspension-state {:stack (:stack stack)})))

(defn- make-debugger-callbacks
  [debug-view]
  {:on-suspended (fn [debug-session _suspend-event]
                   (ui/run-later
                     (update-suspension-state! debug-view debug-session)
                     (state-changed! debug-view true)))
   :on-resumed   (fn [_debug-session]
                   (ui/run-later
                     (g/set-property! debug-view :suspension-state nil)
                     (state-changed! debug-view false)))})

(def ^:private mobdebug-port 8172)

(defn show-connect-failed-info! [^Exception exception workspace]
  (ui/run-later
    (let [error-text (or (.getMessage exception) (.getSimpleName (class exception)))
          log-text (str error-text "\n\nCheck that the game is running and is reachable over the network.")]
      (log/error :msg log-text :exception exception)
      (notifications/show!
        (workspace/notifications workspace)
        {:type :error
         :id ::debugger-connection-error
         :message (localization/message "notification.debug-view.connect-failed.error" {"error" error-text})}))))

(defn start-debugger!
  [debug-view project target-address instance-index]
  (let [debugger-port (+ mobdebug-port instance-index)]
    (mobdebug/connect! target-address debugger-port
                       (fn [debug-session]
                         (ui/run-now
                           (g/update-property! debug-view :debug-session
                                               (fn [old new]
                                                 (when old (mobdebug/close! old))
                                                 new)
                                               debug-session)
                           (update-breakpoints! debug-session (collect-enabled-breakpoints project))
                           (mobdebug/run! debug-session (make-debugger-callbacks debug-view))
                           (state-changed! debug-view true)))
                       (fn [_debug-session]
                         (ui/run-now
                           (g/set-properties! debug-view
                             :debug-session nil
                             :suspension-state nil)
                           (state-changed! debug-view false)))
                       (fn [exception]
                         (show-connect-failed-info! exception (project/workspace project))))))

(defn current-session
  ([debug-view]
   (g/with-auto-evaluation-context evaluation-context
     (current-session debug-view evaluation-context)))
  ([debug-view evaluation-context]
   (g/node-value debug-view :debug-session evaluation-context)))

(defn suspended?
  [debug-view evaluation-context]
  (and (current-session debug-view evaluation-context)
       (some? (g/node-value debug-view :suspension-state evaluation-context))))

(defn debugging? [debug-view evaluation-context]
  (some? (current-session debug-view evaluation-context)))

(defn- contentless? [state]
  (and state (:connection_mode state)))

(defn can-attach? [prefs]
  (if-some [target (targets/selected-target prefs)]
    (and (targets/controllable-target? target)
         (not (contentless? (engine/get-engine-state! target))))
    false))

(def ^:private debugger-init-script "/_defold/debugger/start.lua")

(defn build-targets
  [project evaluation-context]
  (let [start-script (project/get-resource-node project debugger-init-script evaluation-context)]
    (g/node-value start-script :build-targets evaluation-context)))

(defn- built-lua-module
  [build-artifacts path]
  (let [build-artifact (->> build-artifacts
                            (filter #(= (some-> % :resource :resource resource/proj-path) path))
                            (first))]
    (some->> build-artifact
             :resource
             io/file
             .toPath
             Files/readAllBytes
             (protobuf/bytes->map-with-defaults Lua$LuaModule))))

(defn attach!
  [debug-view project target build-artifacts]
  (let [target-address (:address target "localhost")
        target-port (+ mobdebug-port (:instance-index target 0))
        lua-module (built-lua-module build-artifacts debugger-init-script)]
    (assert lua-module)
    (let [attach-successful? (try
                               (engine/run-script! target lua-module)
                               true
                               (catch Exception exception
                                 (show-connect-failed-info! exception (project/workspace project))
                                 false))]
      (when attach-successful?
        (start-debugger! debug-view project target-address (:instance-index target 0))))))

(defn detach!
  [debug-view]
  (when-some [debug-session (current-session debug-view)]
    (mobdebug/done! debug-session)))

(handler/defhandler :debugger.break :global
  (enabled? [debug-view evaluation-context]
            (= :running (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/suspend! (current-session debug-view))))

(handler/defhandler :debugger.continue :global
  ;; NOTE: Shares a shortcut with :app-view/start-debugger.
  ;; Only one of them can be active at a time. This creates the impression that
  ;; there is a single menu item whose label changes in various states.
  (active? [debug-view evaluation-context]
           (debugging? debug-view evaluation-context))
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/run! (current-session debug-view)
                                   (make-debugger-callbacks debug-view))))

(handler/defhandler :debugger.step-over :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-over! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :debugger.step-into :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-into! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :debugger.step-out :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-out! (current-session debug-view)
                                        (make-debugger-callbacks debug-view))))

(handler/defhandler :debugger.detach :global
  (enabled? [debug-view evaluation-context]
            (current-session debug-view evaluation-context))
  (run [debug-view] (mobdebug/done! (current-session debug-view))))

(handler/defhandler :debugger.stop :global
  (enabled? [debug-view evaluation-context]
            (current-session debug-view evaluation-context))
  (run [debug-view] (mobdebug/exit! (current-session debug-view))))

(defn- simulate-rotated-device? [prefs]
  (prefs/get prefs [:run :simulate-rotated-device]))

(defn- change-resolution! [prefs width height]
  (let [target (targets/selected-target prefs)
        simulate-rotated-device? (simulate-rotated-device? prefs)]
    (when target
      (if (targets/all-launched-targets? target)
        (doseq [launched-target (targets/all-launched-targets)]
          (engine/change-resolution! launched-target width height simulate-rotated-device?))
        (engine/change-resolution! target width height simulate-rotated-device?)))))

(defn- project-settings-screen-data [project]
  (let [project-settings (project/settings project)
        width (get project-settings ["display" "width"])
        height (get project-settings ["display" "height"])]
    {:width width :height height}))

(handler/defhandler :run.set-resolution :global
  (options [user-data]
    (when-not user-data
      [{:label (localization/message "command.run.set-resolution.option.custom-resolution")
        :command :run.set-resolution
        :user-data :custom
        :check true}
       {:label (localization/message "command.run.set-resolution.option.apple")
        :children [{:label (localization/message "command.run.set-resolution.option.iphone")
                    :children [{:label (localization/message "command.run.set-resolution.option.viewport-resolutions")
                                :command :private/disabled-menu-label}
                               {:label "iPhone 4 (320x480)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 320
                                            :height 480}}
                               {:label "iPhone 5/SE (320x568)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 320
                                            :height 568}}
                               {:label "iPhone 6/7/8 (375x667)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 375
                                            :height 667}}
                               {:label "iPhone 6/7/8 Plus (414x736)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 414
                                            :height 736}}
                               {:label "iPhone X/XS (375x812)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 375
                                            :height 812}}
                               {:label "iPhone XR/XS Max (414x896)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 414
                                            :height 896}}
                               {:label "iPhone 12 Pro Max/13 Pro Max/14 Plus (428x926)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 428
                                            :height 926}}
                               {:label "iPhone 14 Pro Max/15 Plus/15 Pro Max (430x932)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 430
                                            :height 932}}
                               {:label :separator}
                               {:label (localization/message "command.run.set-resolution.option.native-resolutions")
                                :command :private/disabled-menu-label}
                               {:label "iPhone 4 (640x960)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 640
                                            :height 960}}
                               {:label "iPhone 5/SE (640x1136)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 640
                                            :height 1136}}
                               {:label "iPhone 6/7/8 (750x1334)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 750
                                            :height 1334}}
                               {:label "iPhone 6/7/8 Plus (1242x2208)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1242
                                            :height 2208}}
                               {:label "iPhone X/XS/11 Pro (1125x2436)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1125
                                            :height 2436}}
                               {:label "iPhone XS Max/11 Pro Max (1242x2688)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1242
                                            :height 2688}}
                               {:label "iPhone XR (828x1792)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 828
                                            :height 1792}}
                               {:label "iPhone 12 Pro Max/13 Pro Max/14 Plus (1284x2778)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1284
                                            :height 2778}}
                               {:label "iPhone 14 Pro Max/15 Plus/15 Pro Max (1290x2796)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1290
                                            :height 2796}}]}
                   {:label (localization/message "command.run.set-resolution.option.ipad")
                    :children [{:label (localization/message "command.run.set-resolution.option.viewport-resolutions")
                                :command :private/disabled-menu-label}
                               {:label "iPad Pro (1024x1366)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1024
                                            :height 1366}}
                               {:label "iPad (768x1024)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 768
                                            :height 1024}}
                               {:label :separator}
                               {:label (localization/message "command.run.set-resolution.option.native-resolutions")
                                :command :private/disabled-menu-label}
                               {:label "iPad Pro (2048x2732)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 2048
                                            :height 2732}}
                               {:label "iPad (1536x2048)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 1536
                                            :height 2048}}
                               {:label "iPad Mini (768x1024)"
                                :command :run.set-resolution
                                :check true
                                :user-data {:width 768
                                            :height 1024}}]}]}
       {:label (localization/message "command.run.set-resolution.option.android-devices")
        :children [{:label (localization/message "command.run.set-resolution.option.viewport-resolutions")
                    :command :private/disabled-menu-label}
                   {:label "Samsung Galaxy S7 (360x640)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 360
                                :height 640}}
                   {:label "Samsung Galaxy S8/S9/Note 9 (360x740)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 360
                                :height 740}}
                   {:label "Google Pixel 1/2/XL (412x732)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 412
                                :height 732}}
                   {:label "Google Pixel 3 (412x824)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 412
                                :height 824}}
                   {:label "Google Pixel 3 XL (412x847)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 412
                                :height 847}}
                   {:label :separator}
                   {:label (localization/message "command.run.set-resolution.option.native-resolutions")
                    :command :private/disabled-menu-label}
                   {:label "Samsung Galaxy S7 (1440x2560)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1440
                                :height 2560}}
                   {:label "Samsung Galaxy S8/S9/Note 9 (1440x2960)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1440
                                :height 2960}}
                   {:label "Google Pixel (1080x1920)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1080
                                :height 1920}}
                   {:label "Google Pixel 2/XL (1440x2560)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1440
                                :height 2560}}
                   {:label "Google Pixel 3 (1080x2160)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1080
                                :height 2160}}
                   {:label "Google Pixel 3 XL (1440x2960)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1440
                                :height 2960}}]}
       {:label (localization/message "command.run.set-resolution.option.handheld")
        :children [{:label (localization/message "command.run.set-resolution.option.native-resolutions")
                    :command :private/disabled-menu-label}
                   {:label "1080p (1920x1080)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1920
                                :height 1080}}
                   {:label "720p (1280x720)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1280
                                :height 720}}
                   {:label "Steam Deck (1280x800)"
                    :command :run.set-resolution
                    :check true
                    :user-data {:width 1280
                                :height 800}}]}]))
  (run [project prefs user-data localization]
    (if (= :custom user-data)
      (let [data (or (prefs/get prefs [:run :simulated-resolution])
                     (project-settings-screen-data project))]
        (when-let [{:keys [width height]} (dialogs/make-resolution-dialog data localization)]
          (prefs/set! prefs [:run :simulated-resolution] {:width width :height height :custom true})
          (change-resolution! prefs width height)))
      (do (prefs/set! prefs [:run :simulated-resolution] user-data)
          (change-resolution! prefs (:width user-data) (:height user-data)))))
  (state [user-data prefs]
    (if (= :custom user-data)
      (:custom (prefs/get prefs [:run :simulated-resolution]))
      (= user-data (prefs/get prefs [:run :simulated-resolution])))))

(handler/defhandler :run.reset-resolution :global
  (enabled? [prefs] (prefs/get prefs [:run :simulated-resolution]))
  (run [project prefs]
       (prefs/set! prefs [:run :simulated-resolution] nil)
       (prefs/set! prefs [:run :simulate-rotated-device] false)
       (let [project-settings-data (project-settings-screen-data project)]
         (change-resolution! prefs (:width project-settings-data) (:height project-settings-data)))))

(handler/defhandler :run.toggle-device-rotated :global
  (run [project app-view prefs build-errors-view selection user-data workspace]
       (prefs/set! prefs [:run :simulate-rotated-device] (not (simulate-rotated-device? prefs)))
       (let [data (prefs/get prefs [:run :simulated-resolution])]
         (when data
           (change-resolution! prefs (:width data) (:height data)))))
  (state [app-view user-data prefs workspace] (simulate-rotated-device? prefs)))

(handler/defhandler :private/disabled-menu-label :global
  (enabled? [] false))

(handler/register-menu! ::menubar :editor.defold-project/project
  [{:label (localization/message "menu.debug")
    :id ::debug
    :children [{:label (localization/message "command.debugger.start")
                :command :debugger.start}
               {:label continue-label
                :command :debugger.continue}
               {:label break-label
                :command :debugger.break}
               {:label step-over-label
                :command :debugger.step-over}
               {:label step-into-label
                :command :debugger.step-into}
               {:label step-out-label
                :command :debugger.step-out}
               {:label :separator}
               {:label (localization/message "command.debugger.detach")
                :command :debugger.detach}
               {:label stop-debugger-label
                :command :debugger.stop}
               {:label :separator}
               {:label (localization/message "command.run.open-profiler")
                :command :run.open-profiler}
               {:label (localization/message "command.run.open-resource-profiler")
                :command :run.open-resource-profiler}
               {:label :separator}
               {:label (localization/message "command.run.reset-resolution")
                :command :run.reset-resolution}
               {:label (localization/message "command.run.set-resolution")
                :command :run.set-resolution
                :expand true}
               {:label (localization/message "command.run.toggle-device-rotated")
                :command :run.toggle-device-rotated
                :check true}
               {:label :separator
                :id ::debug-end}]}])


(comment
  (defn get-all-script-nodes [project]
    (keep (fn [node-id]
            (when (g/node-instance? editor.code.script/ScriptNode node-id)
              (g/node-by-id node-id)))
          (g/node-ids (g/graph (g/node-id->graph-id project)))))

  (->> (g/node-value (dev/project) :breakpoints)
       (group-by #(get-in % [:resource :project-path])))

  (g/targets-of (dev/project) :breakpoints)
  (g/sources-of (dev/project) :breakpoints)
  ,)
