(ns editor.debug-view
  (:require
   [clojure.set :as set]
   [dynamo.graph :as g]
   [editor.console :as console]
   [editor.core :as core]
   [editor.debugging.mobdebug :as mobdebug]
   [editor.handler :as handler]
   [editor.lua :as lua]
   [editor.defold-project :as project]
   [editor.resource :as resource]
   [editor.ui :as ui]
   [editor.workspace :as workspace]
   [clojure.string :as str])
  (:import
   (javafx.scene Parent)
   (javafx.scene.control Button Label ListView TreeItem TreeView SplitPane)
   (javafx.scene.layout HBox Pane Priority Region)
   (org.apache.commons.io FilenameUtils)))

(set! *warn-on-reflection* false)

(defn- single [coll]
  (when (nil? (next coll)) (first coll)))

(defn- add-item!
  [^ListView list-view item]
  (.. list-view getItems (add item)))

(defn- set-debugger-data-container-visible! [^SplitPane root visible?]
  (let [debugger-data-container (ui/user-data root :debugger-data-container)
        was-visible? (some? (.lookup root "#debugger-data-container"))]
    (assert (instance? Parent debugger-data-container))
    (cond
      (and visible? (not was-visible?))
      (.add (.getItems root) debugger-data-container)

      (and was-visible? (not visible?))
      (.remove (.getItems root) debugger-data-container))))

(g/defnk update-view!
  [debug-session suspension-state root]
  (let [debugger-visible? (some? debug-session)]
    (set-debugger-data-container-visible! root debugger-visible?)
    (ui/with-controls root [^Parent debugger-prompt ^Parent debugger-tool-bar]
      (.setVisible debugger-prompt debugger-visible?)
      (.setVisible debugger-tool-bar debugger-visible?))
    (when debugger-visible?
      (ui/with-controls root [^ListView debugger-call-stack]
        (if-some [frames (-> suspension-state :stack)]
          (do
            (.. debugger-call-stack getItems (setAll frames))
            (when-let [top-frame (first frames)]
              (ui/select! debugger-call-stack top-frame)))
          (do
            (.. debugger-call-stack getItems clear)
            (.. debugger-call-stack getSelectionModel clearSelection)))))))

(g/defnk produce-active-locations
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

  (output update-view g/Any :cached update-view!)
  (output active-locations g/Any :cached produce-active-locations))


(defonce view-state (atom nil))

(defn- current-stack-frame
  [debug-view]
  (let [root (g/node-value debug-view :root)]
    (ui/with-controls root [^ListView debugger-call-stack]
      (first (.. debugger-call-stack getSelectionModel getSelectedIndices)))))

(defn- on-eval-input
  [debug-view ^ListView event]
  (when-some [debug-session (g/node-value debug-view :debug-session)]
    (let [input (.getSource event)
          code  (.getText input)
          frame (current-stack-frame debug-view)]
      (.clear input)
      (assert (= :suspended (mobdebug/state debug-session)))
      (console/append-console-line! (str "❯ " code))
      (let [ret (mobdebug/eval debug-session code frame)
            s (some->> ret :result vals (str/join ", "))]
        (console/append-console-line! (str "❮ " s))))))

(defn- setup-tool-bar!
  [^Parent console-tool-bar]
  (ui/with-controls console-tool-bar [^Parent debugger-tool-bar pause-debugger step-in-debugger step-out-debugger step-over-debugger stop-debugger]
    (.bind (.managedProperty debugger-tool-bar) (.visibleProperty debugger-tool-bar))
    (ui/bind-action! pause-debugger :break)
    (ui/bind-action! step-in-debugger :step-into)
    (ui/bind-action! step-out-debugger :step-out)
    (ui/bind-action! step-over-debugger :step-over)
    (ui/bind-action! stop-debugger :stop-debugger)))

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
    (ui/cell-factory! (fn [[name value]]
                        (let [value (if (map? value)
                                      (-> value meta :tostring)
                                      (str value))]
                          {:graphic (doto (HBox.)
                                      (ui/fill-control)
                                      (ui/children! [(Label. name)
                                                     (doto (Label. "=")
                                                       (ui/add-style! "equals"))
                                                     (Label. value)]))})))))

(defn- make-variable-tree-item
  [[name value :as variable]]
  (let [tree-item (TreeItem. variable)]
    (when (and (map? value) (seq value))
      (.. tree-item getChildren (addAll (mapv make-variable-tree-item (sort-by key value)))))
    tree-item))

(defn- make-variables-tree-item
  [locals upvalues]
  (make-variable-tree-item ["root" (into locals upvalues)]))

(defn- setup-controls!
  [debug-view ^SplitPane root]
  (ui/with-controls root [debugger-call-stack debugger-data-container ^Parent debugger-prompt debugger-prompt-field debugger-variables console-tool-bar]
    ;; detach debugger-data-container from SplitPane so we can toggle it
    (ui/user-data! root :debugger-data-container debugger-data-container)
    (.remove (.getItems root) debugger-data-container)

    ;; tool bar
    (setup-tool-bar! console-tool-bar)

    ;; debugger prompt
    (.bind (.managedProperty debugger-prompt) (.visibleProperty debugger-prompt))
    (ui/on-action! debugger-prompt-field (partial on-eval-input debug-view))

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

(when @view-state
  (ui/run-now (setup-controls! (-> view-state deref :view-id)
                               (-> view-state deref :root))))

(defn- try-resolve-resource
  [workspace path]
  (let [resource (workspace/resolve-workspace-resource workspace path)]
    (when (some-> resource resource/exists?) resource)))

(defn- file-or-module->resource
  [workspace file]
  (some (partial try-resolve-resource workspace) [file (lua/lua-module->path file)]))

(defn- make-open-resource-fn
  [project open-resource-fn]
  (let [workspace (project/workspace project)]
    (fn [file-or-module line]
      (let [resource (file-or-module->resource workspace file-or-module)]
        (if resource
          (do
            (prn :opening-resource file-or-module)
            (open-resource-fn resource {:line line}))
          (prn :resource-not-found file-or-module)))
      nil)))

(defn- set-breakpoint!
  [debug-session {:keys [resource line] :as breakpoint}]
  (when-some [file (resource/proj-path resource)]
    (mobdebug/set-breakpoint! debug-session (str "=" file) (inc line))))

(defn- remove-breakpoint!
  [debug-session {:keys [resource line] :as breakpoint}]
  (when-some [file (resource/proj-path resource)]
    (mobdebug/remove-breakpoint! debug-session (str "=" file) (inc line))))

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
                  (let [breakpoints (set (g/node-value project :breakpoints))]
                    (when-some [debug-session (g/node-value debug-view :debug-session)]
                      (update-breakpoints! debug-session (:breakpoints @state) breakpoints))
                    (vreset! state {:breakpoints breakpoints})))]
    (ui/->timer 4 "debugger-update-timer" tick-fn)))

(defn make-view!
  [app-view view-graph project ^Parent root open-resource-fn]
  (let [view-id (g/make-node! view-graph DebugView
                              :open-resource-fn (make-open-resource-fn project open-resource-fn))
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
  (ui/select-tab! (ui/parent-tab-pane (g/node-value debug-view :root))
                  "console-tab"))

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

(defn start-debugger!
  [project debug-view]
  (mobdebug/listen (fn [debug-session]
                     (ui/run-now (g/update-property! debug-view :debug-session
                                                     (fn [old new]
                                                       (when old (mobdebug/close! old))
                                                       new)
                                                     debug-session))
                     (update-breakpoints! debug-session (g/node-value project :breakpoints))
                     (mobdebug/run! debug-session (make-debugger-callbacks debug-view))
                     (show! debug-view))
                   (fn [debug-session]
                     (ui/run-later (g/set-property! debug-view :debug-session nil)))))

(defn current-session
  [debug-view]
  (g/node-value debug-view :debug-session))

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


(ui/extend-menu ::menubar :editor.defold-project/project
                [{:label "Debug"
                  :id ::debug
                  :children [{:label "Run with Debugger"
                              :command :start-debugger}
                             {:label "Attach Debugger"
                              :command :attach-debugger}
                             {:label "Continue"
                              :command :continue}
                             {:label "Break"
                              :command :break}
                             {:label "Step Over"
                              :command :step-over}
                             {:label "Step Into"
                              :command :step-into}
                             {:label "Step Out"
                              :command :step-out}
                             {:label :separator}
                             {:label "Detach Debugger"
                              :command :detach-debugger}
                             {:label "Stop Debugger"
                              :command :stop-debugger}]}])
