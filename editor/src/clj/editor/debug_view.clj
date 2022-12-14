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

(ns editor.debug-view
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :refer [line-number->CursorRange]]
            [editor.console :as console]
            [editor.core :as core]
            [editor.debugging.mobdebug :as mobdebug]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.engine :as engine]
            [editor.handler :as handler]
            [editor.lua :as lua]
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

(def ^:private break-label "Break")
(def ^:private continue-label "Continue")
(def ^:private detach-debugger-label "Detach Debugger")
(def ^:private start-debugger-label "Start / Attach")
(def ^:private step-into-label "Step Into")
(def ^:private step-out-label "Step Out")
(def ^:private step-over-label "Step Over")
(def ^:private stop-debugger-label "Stop Debugger")
(def ^:private open-engine-profiler-label "Open Web Profiler")

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
        (let [ret (mobdebug/eval debug-session code frame)]
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
  [^Parent console-tool-bar]
  (ui/with-controls console-tool-bar [^Parent debugger-tool-bar ^Button pause-debugger-button ^Button play-debugger-button step-in-debugger-button step-out-debugger-button step-over-debugger-button stop-debugger-button]
    (.bind (.managedProperty debugger-tool-bar) (.visibleProperty debugger-tool-bar))
    (.bind (.managedProperty pause-debugger-button) (.visibleProperty pause-debugger-button))
    (.bind (.managedProperty play-debugger-button) (.visibleProperty play-debugger-button))
    (ui/tooltip! pause-debugger-button break-label)
    (ui/tooltip! play-debugger-button continue-label)
    (ui/tooltip! step-in-debugger-button step-into-label)
    (ui/tooltip! step-out-debugger-button step-out-label)
    (ui/tooltip! step-over-debugger-button step-over-label)
    (ui/tooltip! stop-debugger-button stop-debugger-label)
    (ui/bind-action! pause-debugger-button :break)
    (ui/bind-action! play-debugger-button :continue)
    (ui/bind-action! step-in-debugger-button :step-into)
    (ui/bind-action! step-out-debugger-button :step-out)
    (ui/bind-action! step-over-debugger-button :step-over)
    (ui/bind-action! stop-debugger-button :stop-debugger)))

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
  [debug-view ^Parent console-grid-pane ^Parent right-pane]
  (ui/with-controls console-grid-pane [console-tool-bar
                                       ^Parent debugger-prompt
                                       debugger-prompt-field]
    ;; tool bar
    (setup-tool-bar! console-tool-bar)

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
  (g/set-property! debug-view :console-grid-pane console-grid-pane :right-pane right-pane)
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
          (open-resource-fn resource {:cursor-range (line-number->CursorRange line)})))
      nil)))

(defn- set-breakpoint!
  [debug-session {:keys [resource row] :as _breakpoint}]
  (when-some [path (resource/proj-path resource)]
    (mobdebug/set-breakpoint! debug-session path (inc row))))

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
           (run! #(set-breakpoint! debug-session %) added)
           (run! #(remove-breakpoint! debug-session %) removed)))))))

(defn- make-update-timer
  [project debug-view]
  (let [state   (volatile! {})
        tick-fn (fn [timer _]
                  (when-not (ui/ui-disabled?)
                    ;; if we don't have a debug session going on, there is no point in pulling
                    ;; project/breakpoints or updating the "last breakpoints" state.
                    (when-some [debug-session (g/node-value debug-view :debug-session)]
                      (let [breakpoints (set (g/node-value project :breakpoints))]
                        (update-breakpoints! debug-session (:breakpoints @state) breakpoints)
                        (vreset! state {:breakpoints breakpoints})))))]
    (ui/->timer 4 "debugger-update-timer" tick-fn)))

(defn- setup-view! [debug-view app-view]
  (g/transact
    [(g/connect app-view :active-resource debug-view :active-resource)
     (g/connect debug-view :execution-locations app-view :debugger-execution-locations)])
  debug-view)

(defn make-view!
  [app-view view-graph project ^Parent root open-resource-fn state-changed-fn]
  (let [console-grid-pane (.lookup root "#console-grid-pane")
        right-pane (.lookup root "#right-pane")
        view-id (setup-view! (g/make-node! view-graph DebugView
                                           :open-resource-fn (make-open-resource-fn project open-resource-fn)
                                           :state-changed-fn state-changed-fn)
                             app-view)
        timer (make-update-timer project view-id)]
    (setup-controls! view-id console-grid-pane right-pane)
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

(defn- show-connect-failed-dialog! [target-address ^Exception exception]
  (let [msg (str "Failed to attach debugger to " target-address ":" mobdebug-port ".\n"
                 "Check that the game is running and is reachable over the network.\n")]
    (log/error :msg msg :exception exception)
    (dialogs/make-info-dialog
      {:title "Attach Debugger Failed"
       :icon :icon/triangle-error
       :header msg
       :content (.getMessage exception)})))

(defn start-debugger!
  [debug-view project target-address]
  (try
    (mobdebug/connect! target-address mobdebug-port
                       (fn [debug-session]
                         (ui/run-now
                           (g/update-property! debug-view :debug-session
                                               (fn [old new]
                                                 (when old (mobdebug/close! old))
                                                 new)
                                               debug-session)
                           (update-breakpoints! debug-session (g/node-value project :breakpoints))
                           (mobdebug/run! debug-session (make-debugger-callbacks debug-view))
                           (state-changed! debug-view true)))
                       (fn [_debug-session]
                         (ui/run-now
                           (g/set-property! debug-view
                                            :debug-session nil
                                            :suspension-state nil)
                           (state-changed! debug-view false))))
    (catch Exception exception
      (show-connect-failed-dialog! target-address exception))))

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

(defn can-attach? [prefs]
  (if-some [target (targets/selected-target prefs)]
    (targets/controllable-target? target)
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
             (protobuf/bytes->map Lua$LuaModule))))

(defn attach!
  [debug-view project target build-artifacts]
  (let [target-address (:address target "localhost")
        lua-module (built-lua-module build-artifacts debugger-init-script)]
    (assert lua-module)
    (let [attach-successful? (try
                               (engine/run-script! target lua-module)
                               true
                               (catch Exception exception
                                 (show-connect-failed-dialog! target-address exception)
                                 false))]
      (when attach-successful?
        (start-debugger! debug-view project target-address)))))

(defn detach!
  [debug-view]
  (when-some [debug-session (current-session debug-view)]
    (mobdebug/done! debug-session)))

(handler/defhandler :break :global
  (enabled? [debug-view evaluation-context]
            (= :running (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/suspend! (current-session debug-view))))

(handler/defhandler :continue :global
  ;; NOTE: Shares a shortcut with :app-view/start-debugger.
  ;; Only one of them can be active at a time. This creates the impression that
  ;; there is a single menu item whose label changes in various states.
  (active? [debug-view evaluation-context]
           (debugging? debug-view evaluation-context))
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/run! (current-session debug-view)
                                   (make-debugger-callbacks debug-view))))

(handler/defhandler :step-over :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-over! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :step-into :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-into! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :step-out :global
  (enabled? [debug-view evaluation-context]
            (= :suspended (some-> (current-session debug-view evaluation-context) mobdebug/state)))
  (run [debug-view] (mobdebug/step-out! (current-session debug-view)
                                        (make-debugger-callbacks debug-view))))

(handler/defhandler :detach-debugger :global
  (enabled? [debug-view evaluation-context]
            (current-session debug-view evaluation-context))
  (run [debug-view] (mobdebug/done! (current-session debug-view))))

(handler/defhandler :stop-debugger :global
  (enabled? [debug-view evaluation-context]
            (current-session debug-view evaluation-context))
  (run [debug-view] (mobdebug/exit! (current-session debug-view))))

(defn- can-change-resolution? [debug-view prefs evaluation-context]
  (and (targets/controllable-target? (targets/selected-target prefs))
       (not (suspended? debug-view evaluation-context))))

(def should-rotate-device?
  (atom false))

(handler/defhandler :set-resolution :global
  (enabled? [debug-view prefs evaluation-context]
            (can-change-resolution? debug-view prefs evaluation-context))
  (run [project app-view prefs build-errors-view selection user-data]
       (engine/change-resolution! (targets/selected-target prefs) (:width user-data) (:height user-data) @should-rotate-device?)))

(handler/defhandler :set-custom-resolution :global
  (enabled? [debug-view prefs evaluation-context]
            (can-change-resolution? debug-view prefs evaluation-context))
  (run [project app-view prefs build-errors-view selection user-data]
       (when-let [{:keys [width height]} (dialogs/make-resolution-dialog)]
         (engine/change-resolution! (targets/selected-target prefs) width height @should-rotate-device?))))

(handler/defhandler :set-rotate-device :global
  (enabled? [] true)
  (run [project app-view prefs build-errors-view selection user-data]
       (swap! should-rotate-device? not))
  (state [app-view user-data] @should-rotate-device?))

(handler/defhandler :disabled-menu-label :global
  (enabled? [] false))

(handler/register-menu! ::menubar :editor.defold-project/project
  [{:label "Debug"
    :id ::debug
    :children [{:label start-debugger-label
                :command :start-debugger}
               {:label continue-label
                :command :continue}
               {:label break-label
                :command :break}
               {:label step-over-label
                :command :step-over}
               {:label step-into-label
                :command :step-into}
               {:label step-out-label
                :command :step-out}
               {:label :separator}
               {:label detach-debugger-label
                :command :detach-debugger}
               {:label stop-debugger-label
                :command :stop-debugger}
               {:label :separator}
               {:label open-engine-profiler-label
                :command :engine-profile-show}
               {:label :separator}
               {:label "Simulate Resolution"
                :children [{:label "Custom Resolution..."
                            :command :set-custom-resolution}
                           {:label "Apple"
                            :children [{:label "iPhone"
                                        :children [{:label "Viewport Resolutions"
                                                    :command :disabled-menu-label}
                                                   {:label "iPhone 4 (320x480)"
                                                    :command :set-resolution
                                                    :user-data {:width 320
                                                                :height 480}}
                                                   {:label "iPhone 5/SE (320x568)"
                                                    :command :set-resolution
                                                    :user-data {:width 320
                                                                :height 568}}
                                                   {:label "iPhone 6/7/8 (375x667)"
                                                    :command :set-resolution
                                                    :user-data {:width 375
                                                                :height 667}}
                                                   {:label "iPhone 6/7/8 Plus (414x736)"
                                                    :command :set-resolution
                                                    :user-data {:width 414
                                                                :height 736}}
                                                   {:label "iPhone X/XS (375x812)"
                                                    :command :set-resolution
                                                    :user-data {:width 375
                                                                :height 812}}
                                                   {:label "iPhone XR/XS Max (414x896)"
                                                    :command :set-resolution
                                                    :user-data {:width 414
                                                                :height 896}}
                                                   {:label :separator}
                                                   {:label "Native Resolutions"
                                                    :command :disabled-menu-label}
                                                   {:label "iPhone 4 (640x960)"
                                                    :command :set-resolution
                                                    :user-data {:width 640
                                                                :height 960}}
                                                   {:label "iPhone 5/SE (640x1136)"
                                                    :command :set-resolution
                                                    :user-data {:width 640
                                                                :height 1136}}
                                                   {:label "iPhone 6/7/8 (750x1334)"
                                                    :command :set-resolution
                                                    :user-data {:width 750
                                                                :height 1334}}
                                                   {:label "iPhone 6/7/8 Plus (1242x2208)"
                                                    :command :set-resolution
                                                    :user-data {:width 1242
                                                                :height 2208}}
                                                   {:label "iPhone X/XS (1125x2436)"
                                                    :command :set-resolution
                                                    :user-data {:width 1125
                                                                :height 2436}}
                                                   {:label "iPhone XS Max (1242x2688)"
                                                    :command :set-resolution
                                                    :user-data {:width 1242
                                                                :height 2688}}
                                                   {:label "iPhone XR (828x1792)"
                                                    :command :set-resolution
                                                    :user-data {:width 828
                                                                :height 1792}}]}
                                       {:label "iPad"
                                        :children [{:label "Viewport Resolutions"
                                                    :command :disabled-menu-label}
                                                   {:label "iPad Pro (1024x1366)"
                                                    :command :set-resolution
                                                    :user-data {:width 1024
                                                                :height 1366}}
                                                   {:label "iPad (768x1024)"
                                                    :command :set-resolution
                                                    :user-data {:width 768
                                                                :height 1024}}
                                                   {:label :separator}
                                                   {:label "Native Resolutions"
                                                    :command :disabled-menu-label}
                                                   {:label "iPad Pro (2048x2732)"
                                                    :command :set-resolution
                                                    :user-data {:width 2048
                                                                :height 2732}}
                                                   {:label "iPad (1536x2048)"
                                                    :command :set-resolution
                                                    :user-data {:width 1536
                                                                :height 2048}}
                                                   {:label "iPad Mini (768x1024)"
                                                    :command :set-resolution
                                                    :user-data {:width 768
                                                                :height 1024}}]}]}
                           {:label "Android Devices"
                            :children [{:label "Viewport Resolutions"
                                        :command :disabled-menu-label}
                                       {:label "Samsung Galaxy S7 (360x640)"
                                        :command :set-resolution
                                        :user-data {:width 360
                                                    :height 640}}
                                       {:label "Samsung Galaxy S8/S9/Note 9 (360x740)"
                                        :command :set-resolution
                                        :user-data {:width 360
                                                    :height 740}}
                                       {:label "Google Pixel 1/2/XL (412x732)"
                                        :command :set-resolution
                                        :user-data {:width 412
                                                    :height 732}}
                                       {:label "Google Pixel 3 (412x824)"
                                        :command :set-resolution
                                        :user-data {:width 412
                                                    :height 824}}
                                       {:label "Google Pixel 3 XL (412x847)"
                                        :command :set-resolution
                                        :user-data {:width 412
                                                    :height 847}}
                                       {:label :separator}
                                       {:label "Native Resolutions"
                                        :command :disabled-menu-label}
                                       {:label "Samsung Galaxy S7 (1440x2560)"
                                        :command :set-resolution
                                        :user-data {:width 1440
                                                    :height 2560}}
                                       {:label "Samsung Galaxy S8/S9/Note 9 (1440x2960)"
                                        :command :set-resolution
                                        :user-data {:width 1440
                                                    :height 2960}}
                                       {:label "Google Pixel (1080x1920)"
                                        :command :set-resolution
                                        :user-data {:width 1080
                                                    :height 1920}}
                                       {:label "Google Pixel 2/XL (1440x2560)"
                                        :command :set-resolution
                                        :user-data {:width 1440
                                                    :height 2560}}
                                       {:label "Google Pixel 3 (1080x2160)"
                                        :command :set-resolution
                                        :user-data {:width 1080
                                                    :height 2160}}
                                       {:label "Google Pixel 3 XL (1440x2960)"
                                        :command :set-resolution
                                        :user-data {:width 1440
                                                    :height 2960}}]}
                           {:label "Rotated Device"
                            :command :set-rotate-device
                            :check true}]}]}])
