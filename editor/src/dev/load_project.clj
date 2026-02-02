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

(ns load-project
  (:require [dev :as dev]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types]
            [internal.system :as is]
            [internal.transaction :as it]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.eduction :as e])
  (:import [java.util List]))

(set! *warn-on-reflection* true)

;; Set to the path of the project directory you want to load.
(defonce project-path "test/resources/save_data_project")

;; When true, generate tx-data for loaded nodes in a separate step before
;; applying it in a transaction. Allows us to profile the two phases in
;; isolation at the cost of increased peak memory usage.
(defonce separate-load-tx-data-generation true)

;; Set to one of the task-phases below to skip the rest of the tasks.
(def final-task :cache-save-data)

;; You can use this to start and stop your own profiling tool for certain tasks.
(defn- user-profiling-hook! [task-key task-fn]
  {:pre [(keyword? task-key) ; This is one of the task-phases below.
         (fn? task-fn)]} ; This is a zero-argument function. You are responsible for calling it and returning its return value.
  ;; You will receive a call for every task listed in task-phases below.
  ;; You are responsible for calling (task-fn) and returning its return value.
  (case task-key
    ;; We can selectively activate and deactivate profiling around these tasks.
    (:read-resources :load-nodes)
    (do
      ;; Start profiling.
      (let [result (task-fn)]
        ;; Stop profiling.
        result))

    ;; A task we do not care about. Just invoke the task-fn.
    (task-fn)))

(def ^:private ^List task-phases
  [:setup-workspace
   :fetch-libraries
   :resource-sync
   :list-resources
   :make-project
   :read-resources
   :generate-load-tx-data
   :apply-load-tx-data
   :update-overrides
   :update-successors
   :cache-save-data
   :store-post-load-system
   :calculate-scene-deps])

(def ^:private final-task-index
  (let [task-index (.indexOf task-phases final-task)]
    (assert (nat-int? task-index) (str "Invalid final-task: " final-task))
    task-index))

(defn- run-task? [task-key]
  {:pre [(keyword? task-key)]}
  (let [task-index (.indexOf task-phases task-key)]
    (assert (nat-int? task-index) (str "Invalid task-key: " task-key))
    (<= task-index (int final-task-index))))

(defonce ^:private task-metrics (du/make-metrics-collector))
(defonce ^:private resource-metrics (du/make-metrics-collector))
(defonce ^:private transaction-metrics (du/make-metrics-collector))

(defonce ^:private change-tracked-transact false)

(defonce ^:private transact-opts
  {:metrics transaction-metrics
   :track-changes change-tracked-transact})

(defn- measure-task-impl! [task-key task-fn]
  (let [task-label (name task-key)]
    (du/log-time-and-memory task-label
      (du/measuring task-metrics task-key
        (user-profiling-hook! task-key task-fn)))))

(defmacro ^:private measure-task! [task-key & body]
  {:pre [(keyword? task-key)
         (nat-int? (.indexOf task-phases task-key))]}
  (let [task-fn-sym (symbol (str "measure-" (name task-key)))]
    `(measure-task-impl! ~task-key (fn ~task-fn-sym [] (do ~@body)))))

(defmacro ^:private run-task! [task-key & body]
  `(when (run-task? ~task-key)
     ~@body))

(defmacro ^:private run-and-measure-task! [task-key & body]
  `(when (run-task? ~task-key)
     (measure-task! ~task-key ~@body)))

(defonce runtime (Runtime/getRuntime))
(defonce start-allocated-bytes (du/allocated-bytes runtime))
(defonce start-time-nanos (System/nanoTime))
(defonce prefs (prefs/project project-path))
(defonce localization (localization/make prefs ::load-project {} ^[] Throwable/.printStackTrace))
(defonce system-config (assoc (shared-editor-settings/load-project-system-config project-path localization) :cache-retain? project/cache-retain?))
(defonce ^:private -set-system- (do (reset! g/*the-system* (is/make-system system-config)) nil))
(defonce workspace-graph-id (g/last-graph-added))

(defonce workspace
  (run-and-measure-task!
    :setup-workspace
    (test-util/setup-workspace! workspace-graph-id project-path)))

(defonce game-project-resource
  (workspace/find-resource workspace "/game.project"))

(defonce up-to-date-lib-states
  (run-and-measure-task!
    :fetch-libraries
    (let [dependencies (project/read-dependencies game-project-resource)
          stale-lib-states (workspace/fetch-and-validate-libraries workspace dependencies progress/null-render-progress!)]
      (workspace/install-validated-libraries! workspace stale-lib-states))))

(defonce ^:private -initial-resource-sync-
  (run-and-measure-task!
    :resource-sync
    (workspace/resource-sync! workspace)
    nil))

(defonce project-graph-id (g/make-graph! :history true :volatility 1))

(defonce node-id+resource-pairs
  (run-and-measure-task!
    :list-resources
    (project/make-node-id+resource-pairs
      project-graph-id
      (g/node-value workspace :resource-list))))

(defonce game-project-node-id
  (some (fn [[node-id resource]]
          (when (identical? game-project-resource resource)
            node-id))
        node-id+resource-pairs))

(defonce project
  (run-and-measure-task!
    :make-project
    (let [extensions (extensions/make project-graph-id)]
      (project/make-project project-graph-id workspace extensions))))

(defonce node-load-infos
  (run-and-measure-task!
    :read-resources
    (project/read-nodes node-id+resource-pairs
      :resource-metrics resource-metrics)))

(defonce migrated-resource-node-ids
  (let [tx-data
        (run-and-measure-task!
          :generate-load-tx-data
          (let [{:keys [disk-sha256s-by-node-id node-id+source-value-pairs]}
                (project/node-load-infos->stored-disk-state node-load-infos)]
            (resource-node/merge-source-values! node-id+source-value-pairs)
            (coll/transfer
              (e/concat
                (project/make-resource-nodes-tx-data project node-id+resource-pairs)
                (project/setup-game-project-tx-data project game-project-node-id)
                (workspace/merge-disk-sha256s workspace disk-sha256s-by-node-id)
                (project/load-nodes-tx-data project node-load-infos progress/null-render-progress! progress/null-render-progress! resource-metrics))
              (if separate-load-tx-data-generation [] :eduction)
              coll/flatten-xf)))

        tx-result
        (as-> (g/make-transaction-context transact-opts) transaction-context

              (run-and-measure-task!
                :apply-load-tx-data
                (it/apply-tx transaction-context tx-data))

              (run-and-measure-task!
                :update-overrides
                (it/update-overrides transaction-context))

              (it/mark-nodes-modified transaction-context)

              (run-and-measure-task!
                :update-successors
                (it/update-successors transaction-context))

              (when transaction-context
                (it/trace-dependencies transaction-context)
                (it/apply-tx-label transaction-context)
                (it/finalize-update transaction-context)))

        _ (when tx-result
            (g/commit-tx-result! tx-result transact-opts))

        migrated-resource-node-ids
        (let [basis (:basis tx-result)]
          (into #{}
                (keep #(resource-node/owner-resource-node-id basis %))
                (g/migrated-node-ids tx-result)))]

    (when-not change-tracked-transact
      (g/clear-system-cache!))
    (run-and-measure-task!
      :cache-save-data
      (project/cache-loaded-save-data! node-load-infos project migrated-resource-node-ids))
    (let [basis (g/now)
          migrated-proj-paths
          (into (sorted-set)
                (map #(resource/proj-path (resource-node/resource basis %)))
                migrated-resource-node-ids)]
      (when (pos? (count migrated-proj-paths))
        (log/info :message "Some files were migrated and will be saved in an updated format." :migrated-proj-paths migrated-proj-paths)))

    migrated-resource-node-ids))

(defonce ^:private -reset-undo- (g/reset-undo! project-graph-id))

(defonce total-duration-nanos
  (let [end-time-nanos (System/nanoTime)]
    (- end-time-nanos (long start-time-nanos))))

(defonce end-allocated-bytes (du/allocated-bytes runtime))

(defonce total-allocated-bytes
  (- end-allocated-bytes (long start-allocated-bytes)))

(defonce ^:private -log-statistics-
  (log/info :message "total"
            :elapsed (du/nanos->string total-duration-nanos)
            :elapsed-sans-gc (du/nanos->string (- total-duration-nanos (du/gc-overhead-ns)))
            :allocated (du/bytes->string total-allocated-bytes)
            :heap (du/bytes->string end-allocated-bytes)))

(defonce load-metrics
  (du/when-metrics
    {:new-nodes-by-path (g/node-value project :nodes-by-resource-path)
     :task-metrics @task-metrics
     :resource-metrics @resource-metrics
     :transaction-metrics @transaction-metrics}))

(defonce post-load-system
  (run-task!
    :store-post-load-system
    @g/*the-system*))

(defn- evaluated-endpoints-by-proj-path [project output-label render-progress!]
  (g/with-auto-evaluation-context evaluation-context
    (let [basis (:basis evaluation-context)

          proj-paths+node-ids
          (->> (g/valid-node-value project :nodes-by-resource-path evaluation-context)
               (filterv (fn [[_proj-path node-id]]
                          (some-> (g/node-type* basis node-id)
                                  (g/has-output? output-label))))
               (sort-by key))

          proj-path-count (count proj-paths+node-ids)]
      (coll/transfer proj-paths+node-ids {}
        (map-indexed
          (fn [proj-path-index [proj-path node-id]]
            (let [message (localization/message nil [] proj-path)
                  progress (progress/make message proj-path-count proj-path-index)]
              (render-progress! progress)
              (let [evaluated-endpoints (dev/recursive-predecessor-endpoints basis node-id output-label)]
                (pair proj-path evaluated-endpoints)))))))))

(defonce evaluated-scene-endpoints-by-proj-path
  (run-and-measure-task!
    :calculate-scene-deps
    (dev/run-with-terminal-progress "Calculating Scene Dependencies..."
      (fn calc-scene-deps-with-progress [render-progress!]
        (evaluated-endpoints-by-proj-path project :scene render-progress!)))))
