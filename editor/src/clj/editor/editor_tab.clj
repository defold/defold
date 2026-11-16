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

(ns editor.editor-tab
  (:require [cljfx.api :as fx]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.progress-bar :as fx.progress-bar]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.text-area :as fx.text-area]
            [cljfx.fx.v-box :as fx.v-box]
            [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-loading :as resource-loading]
            [editor.ui :as ui]
            [editor.view :as view])
  (:import [javafx.animation PauseTransition]
           [javafx.scene.control Tab]
           [javafx.util Duration]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn view-type
  "Returns the map describing the view-type of the view associated with the
  editor tab, if any. See workspace/register-view-type."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (and (map? %)
                   (keyword? (:id %))))]}
  (ui/user-data editor-tab ::view-type))

(defn set-view-type!
  "Associate a map describing the view-type of the view with the editor tab. The
  supplied view-type should be either a view-type map registered with the
  workspace or nil. Returns nil. See workspace/register-view-type."
  [^Tab editor-tab view-type]
  {:pre [(instance? Tab editor-tab)
         (or (nil? view-type)
             (and (map? view-type)
                  (keyword? (:id view-type))))]}
  (ui/user-data! editor-tab ::view-type view-type))

(defn view-type-id
  "Returns the keyword identifying the view-type associated with the editor tab,
  if any. See workspace/register-view-type."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (keyword? %))]}
  (:id (view-type editor-tab)))

(defn requested-view-type
  "Returns the view type requested when opening this tab.

  A pending tab initially contains a PendingWorkbenchView node and has no actual
  `view-type`, so editing commands cannot use the resource before it is ready.
  Its `requested-view-type` still identifies the intended editor, allowing
  repeated open requests to find the tab and tab preferences to preserve it.

  Once preparation finishes, the requested editor's view node replaces the
  pending node in the same Tab. The actual `view-type` then matches the
  requested type. Tabs created with their final view immediately fall back to
  `view-type`."
  [^Tab editor-tab]
  (or (ui/user-data editor-tab ::requested-view-type)
      (view-type editor-tab)))

(defn set-requested-view-type! [^Tab editor-tab view-type]
  (ui/user-data! editor-tab ::requested-view-type view-type))

(defn view-node-id
  "Returns the node-id of the view node associated with the editor tab, if any."
  [^Tab editor-tab]
  {:pre [(instance? Tab editor-tab)]
   :post [(or (nil? %)
              (g/node-id? %))]}
  (ui/user-data editor-tab ::view-node-id))

(defn set-view-node-id!
  "Associate a view node-id with the editor tab. The supplied view-node-id
  should be either the node-id of a view node in the graph or nil. Returns nil."
  [^Tab editor-tab view-node-id]
  {:pre [(instance? Tab editor-tab)
         (or (nil? view-node-id)
             (g/node-id? view-node-id))]}
  (ui/user-data! editor-tab ::view-node-id view-node-id))

(defn resource-node-id
  "Returns the node-id of the ResourceNode associated with the editor tab, if
  any."
  [^Tab editor-tab evaluation-context]
  {:post [(or (nil? %)
              (and (g/node-id? %)
                   (g/node-instance? @(:basis-ref evaluation-context) resource/ResourceNode %)))]}
  (when-let [view-node-id (view-node-id editor-tab)]
    (let [[_ {:keys [resource-node pending-resource-node]}] (g/node-value view-node-id :view-data evaluation-context)]
      (or resource-node pending-resource-node))))

(defn resource
  "Returns the Resource associated with the editor tab, if any."
  [^Tab editor-tab evaluation-context]
  {:post [(or (nil? %)
              (resource/resource? %))]}
  (some-> (resource-node-id editor-tab evaluation-context)
          (g/node-value :resource evaluation-context)))

(defn prefs-data
  "Returns the preference data associated with the editor tab, if any."
  [^Tab editor-tab evaluation-context]
  (when-let [view-type-id (:id (requested-view-type editor-tab))]
    (when-let [resource (resource editor-tab evaluation-context)]
      (let [proj-path (resource/proj-path resource)]
        [proj-path
         view-type-id]))))

;; Tab types that show something other than a project resource. Tabs of these
;; types are ephemeral: nothing saves or restores them yet. Subsystems that need
;; to handle a tab by its type look the type up here rather than knowing about
;; the feature that registered it.
(defonce ^:private tab-types-atom (atom {}))

(defn register-type!
  "Registers an editor tab type, replacing any type previously registered under
  the same id. Returns nil.

  `type-id` - qualified keyword identifying the tab type
  `descriptor` - map with:
    `:make-tab-spec-fn` - (fn [opts] tab-spec), where tab-spec describes the tab
                          to app-view/make-editor-tab!"
  [type-id descriptor]
  {:pre [(qualified-keyword? type-id)]}
  (swap! tab-types-atom assoc type-id descriptor)
  nil)

(defn unregister-type!
  "Removes the editor tab type registered under `type-id`. Returns nil."
  [type-id]
  (swap! tab-types-atom dissoc type-id)
  nil)

(defn resolve-type
  "Returns the descriptor registered under `type-id`, or nil."
  [type-id]
  (get @tab-types-atom type-id))

(defn instance-key [^Tab editor-tab]
  (ui/user-data editor-tab ::instance-key))

(defn set-instance-key! [^Tab editor-tab instance-key]
  (ui/user-data! editor-tab ::instance-key instance-key))

(defn- start-pending-workbench-view-progress-delay! [swap-state _completion]
  (let [progress-delay
        (doto (PauseTransition. (Duration/millis 300))
          (.setOnFinished
            (ui/event-handler _event
              (swap-state assoc :show-progress true)))
          (.play))]
    (fn stop-progress-delay! []
      (.stop progress-delay))))

(defn- pending-workbench-view-error-content [{:keys [error-message localization-state proj-path]}]
  {:fx/type fx.v-box/lifecycle

   :children
   [{:fx/type fxui/prominent-header
     :header (localization/message "dialog.open-resource.unrecognized.title")
     :icon :icon/triangle-sad
     :localization localization-state
     :wrap-text false}
    {:fx/type fx.v-box/lifecycle
     :style-class "pending-workbench-view-error-content"
     :v-box/vgrow :always

     :children
     [{:fx/type fx.label/lifecycle
       :min-height :use-pref-size
       :wrap-text true

       :text
       (localization-state
         (localization/message "dialog.open-resource.unrecognized.header" {"resource" proj-path}))}
      {:fx/type fx.text-area/lifecycle
       :min-height 0
       :v-box/vgrow :always
       :editable false
       :focus-traversable false
       :wrap-text true
       :text (localization-state error-message)}]}]})

(ui/defc pending-workbench-view-content
  {:compose
   [{:fx/type fx/ext-watcher
     :ref (:localization props)
     :key :localization-state}
    {:fx/type fx/ext-watcher
     :ref (:load-state props)
     :key :load-state}
    {:fx/type fx/ext-state

     ;; Each new load process, including a retry, gets its own visibility delay.
     :initial-state
     {:completion (:completion (:load-state props))
      :show-progress false}}
    {:fx/type fx/ext-effect
     :fn start-pending-workbench-view-progress-delay!
     :args [(:swap-state props) (:completion (:load-state props))]}]}
  [{:keys [load-state localization-state state]}]
  (let [{:keys [error-message progress proj-path]} load-state]
    {:fx/type fx.stack-pane/lifecycle
     :style-class "pending-workbench-view"
     :background {:fills [{:fill (fxui/vec->color colors/scene-background)}]}

     :children
     [(if error-message
        {:fx/type pending-workbench-view-error-content
         :error-message error-message
         :localization-state localization-state
         :proj-path proj-path}
        {:fx/type fx.v-box/lifecycle
         :style-class "pending-workbench-view-progress"
         :pref-width 540
         :max-width :use-pref-size
         :max-height :use-pref-size
         :visible (:show-progress state)

         :children
         [{:fx/type fx.label/lifecycle
           :text (localization-state (progress/message progress))}
          {:fx/type fx.progress-bar/lifecycle
           :style-class ["progress-bar" "really-small-progress-bar"]
           :max-width Double/MAX_VALUE
           :progress (or (progress/fraction progress) -1.0)}]})]}))

(defn cancel-pending-tab!
  "Cancels the pending tab's load process and disposes its progress UI."
  [tab]
  (when-let [load-state (ui/user-data tab ::workbench-view-load-state)]
    (swap! load-state assoc :cancelled true)
    (ui/advance-ui-user-data-component! tab ::pending-workbench-view nil)))

(defn reopen-pending-tab!
  "Updates a pending tab's open options, retrying a failed load process.

  Returns true if the tab is pending, or nil if it already has its final view.
  Must be called on the JavaFX application thread."
  [tab open-opts]
  (when-let [load-state (ui/user-data tab ::workbench-view-load-state)]
    (let [{:keys [retry!]} (swap! load-state update :open-opts merge open-opts)]
      (when retry!
        (retry!)))
    true))

(defn make-pending-tab!
  "Creates a pending tab and starts its background resource load process.

  Materializes the resource's dependencies, then traces and evaluates
  `evaluated-output-label`. Progress, errors, cancellation, and retries belong
  to the tab. Must be called on the JavaFX application thread.

  The callbacks keep AppView integration with the caller:
    `:make-tab-fn` - (fn [tab-spec]) creates a Tab, adding its title, icon,
                    requested view type, and other presentation details
    `:close-tab-fn` - (fn [tab]) closes a tab whose resource no longer exists
    `:replace-view-fn` - (fn [tab pending-view resource-node open-opts])
                        installs the prepared view and deletes the pending node

  All callbacks run on the JavaFX application thread. Returns the Tab."
  [project localization resource-node evaluated-output-label open-opts
   {:keys [make-tab-fn close-tab-fn replace-view-fn]}]
  (let [completion (future/make)
        loading-message (localization/message "progress.loading")

        load-state
        (atom {:progress (progress/make-indeterminate loading-message)
               :open-opts open-opts
               :completion completion})

        task-cancelled?
        (fn task-cancelled? []
          (boolean (:cancelled @load-state)))

        render-progress!
        (progress/throttle-render-progress
          (fn render-workbench-view-load-progress! [progress]
            (swap! load-state
                   (fn [load-state]
                     (if (:cancelled load-state)
                       load-state
                       (assoc load-state :progress progress))))))

        tab-spec
        {:make-view-fn
         (fn make-pending-view [parent {:keys [tab]}]
           (let [tx-result
                 (g/transact {:undoable false}
                   (g/make-node view/PendingWorkbenchView :load-state load-state))

                 view (first (g/tx-nodes-added tx-result))]

             (ui/advance-ui-user-data-component!
               tab ::pending-workbench-view
               {:fx/type fxui/ext-with-anchor-pane-props
                :desc {:fx/type ui/ext-value :value parent}

                :props
                {:children
                 [{:fx/type pending-workbench-view-content
                   :load-state load-state
                   :localization localization
                   :anchor-pane/top 0
                   :anchor-pane/right 0
                   :anchor-pane/bottom 0
                   :anchor-pane/left 0}]}})
             view))

         :connect-view-fn
         (fn connect-pending-view [view]
           (g/connect resource-node :node-id+resource view :pending-node-id+resource))}

        ^Tab tab (make-tab-fn tab-spec)
        pending-view (view-node-id tab)]
    (ui/user-data! tab ::workbench-view-load-state load-state)
    (ui/on-closed! tab
                   (fn dispose-pending-content [_event]
                     (cancel-pending-tab! tab)))
    (letfn [(fail-loading! [completion error]
              (swap! load-state assoc
                     :error-message (dialogs/exception-messages (Throwable->map error))
                     :retry! retry-loading!)
              (error-reporting/report-exception! error)
              (future/complete! completion :failed))

            (finish-loading! [completion loaded-resource-node invalidate-counters error]
              (if (task-cancelled?)
                (future/complete! completion :cancelled)
                (let [[current-resource-node current-resource]
                      (g/node-value pending-view :pending-node-id+resource)]
                  (cond
                    (or (not current-resource-node)
                        (not (resource/exists? current-resource)))
                    (do
                      (close-tab-fn tab)
                      (future/complete! completion :cancelled))

                    (or (not= loaded-resource-node current-resource-node)
                        (and invalidate-counters
                             (let [endpoint-invalidated? (g/endpoint-invalidated-pred invalidate-counters)
                                   output-endpoint (g/endpoint current-resource-node evaluated-output-label)]
                               (endpoint-invalidated? output-endpoint))))
                    (start-loading! completion)

                    error
                    (fail-loading! completion error)

                    (g/defective? (g/now) current-resource-node)
                    (let [error-value (g/node-value current-resource-node evaluated-output-label)
                          exception (get-in error-value [:user-data :exception])

                          error-message
                          (if exception
                            (dialogs/exception-messages (Throwable->map exception))
                            (g/error-message error-value))]
                      (swap! load-state assoc
                             :error-message error-message
                             :retry! retry-loading!)
                      (future/complete! completion :failed))

                    :else
                    (let [open-opts (:open-opts @load-state)]
                      (ui/advance-ui-user-data-component! tab ::pending-workbench-view nil)
                      (replace-view-fn tab pending-view current-resource-node open-opts)
                      (ui/user-data! tab ::workbench-view-load-state nil)
                      (future/complete! completion :ready))))))

            (retry-loading! []
              ;; The failed completion is final. Reopening starts a new load
              ;; process while retaining the pending view and Tab.
              (let [completion (future/make)]
                (swap! load-state
                       (fn [load-state]
                         (-> load-state
                             (dissoc :error-message :retry!)
                             (assoc :progress (progress/make-indeterminate loading-message)
                                    :completion completion))))
                (start-loading! completion)))

            (start-loading! [completion]
              (let [[resource-node resource] (g/node-value pending-view :pending-node-id+resource)
                    proj-path (resource/proj-path resource)]
                (swap! load-state assoc :proj-path proj-path)
                (future/compute
                  (let [result
                        (try
                          {:invalidate-counters
                           (resource-loading/load-and-evaluate-resource!
                             project resource-node proj-path evaluated-output-label render-progress! task-cancelled?)}
                          (catch Throwable error
                            {:error error}))]
                    (ui/run-now
                      (try
                        (finish-loading! completion resource-node (:invalidate-counters result) (:error result))
                        (catch Throwable error
                          (fail-loading! completion error))))))))]
      (start-loading! completion))
    tab))
