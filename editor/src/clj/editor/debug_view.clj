(ns editor.debug-view
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
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
           [javafx.beans.value ChangeListener]
           [javafx.event ActionEvent]
           [javafx.scene Parent]
           [javafx.scene.control Button Label ListView SplitPane TreeItem TreeView TextField]
           [javafx.scene.layout HBox Pane Priority]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private attach-debugger-label "Attach Debugger")
(def ^:private break-label "Break")
(def ^:private continue-label "Continue")
(def ^:private detach-debugger-label "Detach Debugger")
(def ^:private start-debugger-label "Run with Debugger")
(def ^:private step-into-label "Step Into")
(def ^:private step-out-label "Step Out")
(def ^:private step-over-label "Step Over")
(def ^:private stop-debugger-label "Stop Debugger")
(def ^:private open-web-profiler-label "Open Web Profiler")

(defn- single [coll]
  (when (nil? (next coll)) (first coll)))

(defn- set-debugger-data-visible!
  [^Parent root visible?]
  (ui/with-controls root [^Parent right-split ^Parent debugger-data-split]
    (let [was-visible? (.isVisible debugger-data-split)]
      (when (not= was-visible? visible?)
        (ui/visible! right-split (not visible?))
        (ui/visible! debugger-data-split visible?)))))

(defn- debuggable-resource? [resource]
  (boolean (some-> resource resource/resource-type :tags (contains? :debuggable))))

(g/defnk update-available-controls!
  [active-resource debug-session suspension-state root]
  (let [frames (:stack suspension-state)
        suspended? (some? frames)
        resource-debuggable? (debuggable-resource? active-resource)
        debug-session? (some? debug-session)]
    (set-debugger-data-visible! root (and debug-session? resource-debuggable? suspended?))
    (ui/with-controls root [debugger-prompt debugger-tool-bar]
      (ui/visible! debugger-prompt (and debug-session? suspended?))
      (ui/visible! debugger-tool-bar debug-session?))
    (when debug-session?
      (ui/with-controls root [debugger-prompt-field pause-debugger-button play-debugger-button step-in-debugger-button step-out-debugger-button step-over-debugger-button]
        (ui/visible! pause-debugger-button (not suspended?))
        (ui/visible! play-debugger-button suspended?)
        (ui/enable! pause-debugger-button (not suspended?))
        (ui/enable! play-debugger-button suspended?)
        (ui/enable! step-in-debugger-button suspended?)
        (ui/enable! step-out-debugger-button suspended?)
        (ui/enable! step-over-debugger-button suspended?)
        (ui/enable! debugger-prompt-field suspended?)))))

(g/defnk update-call-stack!
  [debug-session suspension-state root]
  ;; NOTE: This should only depend upon stuff that changes due to a state change
  ;; in the debugger, since selecting the top-frame below will open the suspended
  ;; file and line in the editor.
  (when (some? debug-session)
    (ui/with-controls root [^ListView debugger-call-stack]
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

(g/defnode DebugView
  (inherits core/Scope)

  (property open-resource-fn g/Any)

  (property debug-session g/Any)
  (property suspension-state g/Any)

  (property root Parent)

  (input active-resource resource/Resource)

  (output update-available-controls g/Any :cached update-available-controls!)
  (output update-call-stack g/Any :cached update-call-stack!)
  (output execution-locations g/Any :cached produce-execution-locations))


(defonce view-state (atom nil))

(defn- current-stack-frame
  [debug-view]
  (let [root (g/node-value debug-view :root)]
    (ui/with-controls root [^ListView debugger-call-stack]
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
  [debug-view ^ActionEvent event]
  (when-some [debug-session (g/node-value debug-view :debug-session)]
    (let [input ^TextField (.getSource event)
          code  (.getText input)
          frame (current-stack-frame debug-view)]
      (.clear input)
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
    (.setShowRoot false)
    (ui/cell-factory! (fn [{:keys [display-name display-value]}]
                        {:graphic (doto (HBox.)
                                    (ui/fill-control)
                                    (ui/children! [(Label. display-name)
                                                   (doto (Label. "=")
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

(defn- setup-controls!
  [debug-view ^SplitPane root]
  (ui/with-controls root [console-tool-bar
                          debugger-call-stack
                          ^Parent debugger-data-split
                          ^Parent debugger-prompt
                          debugger-prompt-field
                          ^TreeView debugger-variables
                          ^Parent right-split]
    ;; debugger data views
    (.bind (.managedProperty debugger-data-split) (.visibleProperty debugger-data-split))
    (.bind (.managedProperty right-split) (.visibleProperty right-split))

    ;; tool bar
    (setup-tool-bar! console-tool-bar)

    ;; debugger prompt
    (.bind (.managedProperty debugger-prompt) (.visibleProperty debugger-prompt))
    (ui/on-action! debugger-prompt-field #(on-eval-input debug-view %))

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
    (setup-variables-view! debugger-variables)

    (g/set-property! debug-view :root root)
    nil))

(defn- file-or-module->resource
  [workspace file]
  (some (partial workspace/find-resource workspace) [file (lua/lua-module->path file)]))

(defn- make-open-resource-fn
  [project open-resource-fn]
  (let [workspace (project/workspace project)]
    (fn [file-or-module line]
      (let [resource (file-or-module->resource workspace file-or-module)]
        (when resource
          (open-resource-fn resource {:line line})))
      nil)))

(defn- set-breakpoint!
  [debug-session {:keys [resource line] :as breakpoint}]
  (when-some [path (resource/proj-path resource)]
    ;; NOTE: The filenames returned by debug.getinfo("S").source
    ;; differs between lua/luajit:
    ;;
    ;; - luajit always returns a path for both .script components and
    ;;   .lua modules, ie. /path/to/module.lua
    ;;
    ;; - lua returns a path for .script components, but the dotted
    ;;   module name path.to.module for .lua modules
    ;;
    ;; So, when the breakpoint is being added to a .lua module, we set
    ;; an additional breakpoint on the module name, ensuring we break
    ;; correctly in both cases.
    (mobdebug/set-breakpoint! debug-session path (inc line))
    (when (= "lua" (FilenameUtils/getExtension path))
      (mobdebug/set-breakpoint! debug-session (lua/path->lua-module path) (inc line)))))

(defn- remove-breakpoint!
  [debug-session {:keys [resource line] :as breakpoint}]
  (when-some [path (resource/proj-path resource)]
    (mobdebug/remove-breakpoint! debug-session path (inc line))
    (when (= "lua" (FilenameUtils/getExtension path))
      (mobdebug/remove-breakpoint! debug-session (lua/path->lua-module path) (inc line)))))

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
  [app-view view-graph project ^Parent root open-resource-fn]
  (let [view-id (setup-view! (g/make-node! view-graph DebugView
                                           :open-resource-fn (make-open-resource-fn project open-resource-fn))
                             app-view)
        timer (make-update-timer project view-id)]
    (setup-controls! view-id root)
    (ui/timer-start! timer)
    (reset! view-state {:app-view app-view
                        :view-graph view-graph
                        :project project
                        :root root
                        :open-resource-fn open-resource-fn
                        :view-id view-id
                        :timer timer})
    view-id))

(defn show!
  [debug-view]
  (ui/with-controls (g/node-value debug-view :root) [tool-tabs]
    (ui/select-tab! tool-tabs "console-tab")))

(defn- update-suspension-state!
  [debug-view debug-session]
  (let [stack (mobdebug/stack debug-session)]
    (g/set-property! debug-view
                     :suspension-state {:stack (:stack stack)})))

(defn- make-debugger-callbacks
  [debug-view]
  {:on-suspended (fn [debug-session suspend-event]
                   (ui/run-later
                     (update-suspension-state! debug-view debug-session)
                     (show! debug-view)))
   :on-resumed   (fn [debug-session]
                   (ui/run-later (g/set-property! debug-view :suspension-state nil)))})

(def ^:private mobdebug-port 8172)

(defn start-debugger!
  [debug-view project target-address]
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
                         (show! debug-view)))
                     (fn [debug-session]
                       (ui/run-now
                         (g/set-property! debug-view
                                          :debug-session nil
                                          :suspension-state nil)))))

(defn current-session
  [debug-view]
  (g/node-value debug-view :debug-session))

(defn suspended?
  [debug-view]
  (and (current-session debug-view)
       (some? (g/node-value debug-view :suspension-state))))

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
                                 (let [msg (str "Failed to attach debugger to " target-address ":" mobdebug-port ".\n"
                                                "Check that the game is running and is reachable over the network.\n")]
                                   (log/error :msg msg :exception exception)
                                   (dialogs/make-error-dialog "Attach Debugger Failed" msg (.getMessage exception))
                                   false)))]
      (when attach-successful?
        (start-debugger! debug-view project target-address)))))

(defn detach!
  [debug-view]
  (when-some [debug-session (current-session debug-view)]
    (mobdebug/done! debug-session)))

(handler/defhandler :break :global
  (enabled? [debug-view] (= :running (some-> (current-session debug-view) mobdebug/state)))
  (run [debug-view] (mobdebug/suspend! (current-session debug-view))))

(handler/defhandler :continue :global
  (enabled? [debug-view] (= :suspended (some-> (current-session debug-view) mobdebug/state)))
  (run [debug-view] (mobdebug/run! (current-session debug-view)
                                   (make-debugger-callbacks debug-view))))

(handler/defhandler :step-over :global
  (enabled? [debug-view] (= :suspended (some-> (current-session debug-view) mobdebug/state)))
  (run [debug-view] (mobdebug/step-over! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :step-into :global
  (enabled? [debug-view] (= :suspended (some-> (current-session debug-view) mobdebug/state)))
  (run [debug-view] (mobdebug/step-into! (current-session debug-view)
                                         (make-debugger-callbacks debug-view))))

(handler/defhandler :step-out :global
  (enabled? [debug-view] (= :suspended (some-> (current-session debug-view) mobdebug/state)))
  (run [debug-view] (mobdebug/step-out! (current-session debug-view)
                                        (make-debugger-callbacks debug-view))))

(handler/defhandler :detach-debugger :global
  (enabled? [debug-view] (current-session debug-view))
  (run [debug-view] (mobdebug/done! (current-session debug-view))))

(handler/defhandler :stop-debugger :global
  (enabled? [debug-view] (current-session debug-view))
  (run [debug-view] (mobdebug/exit! (current-session debug-view))))

(handler/defhandler :open-web-profiler :global
  (enabled? [prefs]
    (some? (targets/selected-target prefs)))
  (run [prefs]
    (let [address (:address (targets/selected-target prefs))]
      (ui/open-url (format "http://%s:8002/" address)))))

(ui/extend-menu ::menubar :editor.defold-project/project
                [{:label "Debug"
                  :id ::debug
                  :children [{:label start-debugger-label
                              :command :start-debugger}
                             {:label attach-debugger-label
                              :command :attach-debugger}
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
                             {:label open-web-profiler-label
                              :command :open-web-profiler}]}])
