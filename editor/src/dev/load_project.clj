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

(ns load-project
  (:require [dev :as dev]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types]
            [internal.system :as is]
            [service.log :as log]
            [util.debug-util :as du])
  (:import [java.util List]))

(set! *warn-on-reflection* true)

;; Set to the path of the project directory you want to load.
(defonce project-path "test/resources/save_data_project")

;; Set to one of the task-phases below to skip the rest of the tasks.
(defonce final-task :cache-save-data)

;; You can use this to start and stop your own profiling tool for certain tasks.
(defn- user-profiling-hook! [task-key task-fn]
  {:pre [(keyword? task-key) ; This is one of the task-phases below.
         (fn? task-fn)]} ; This is a zero-argument function. You are responsible for calling it and returning its return value.
  ;; You will receive a call for every task listed in task-phases below.
  ;; You are responsible for calling (task-fn) and returning its return value.
  (case task-key
    ;; We can selectively activate and deactivate profiling around these tasks.
    (:make-nodes :read-resources :load-nodes)
    (do
      ;; Start profiling.
      (let [result (task-fn)]
        ;; Stop profiling.
        result))

    ;; A task we do not care about. Just invoke the task-fn.
    (task-fn)))

(defonce ^:private ^List task-phases
  [:setup-workspace
   :fetch-libraries
   :resource-sync
   :make-project
   :list-resources
   :make-nodes
   :setup-project
   :read-resources
   :sort-source-values
   :store-source-values
   :load-nodes
   :cache-save-data])

(defonce ^:private final-task-index
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

(defmacro ^:private run-and-measure-task! [task-key & body]
  `(when (run-task? ~task-key)
     (measure-task! ~task-key ~@body)))

(defonce system-config (shared-editor-settings/load-project-system-config project-path))
(defonce ^:private -set-system- (do (reset! g/*the-system* (is/make-system system-config)) nil))
(defonce workspace-graph-id (g/last-graph-added))

(defonce workspace
  (run-and-measure-task!
    :setup-workspace
    (test-util/setup-workspace! workspace-graph-id project-path)))

(defonce up-to-date-lib-states
  (when (run-task? :fetch-libraries)
    (dev/run-with-progress "Fetching Libraries..."
      (fn fetch-libraries-with-progress [render-progress!]
        (measure-task!
          :fetch-libraries
          (let [game-project-resource (workspace/find-resource workspace "/game.project")
                dependencies (project/read-dependencies game-project-resource)
                stale-lib-states (workspace/fetch-and-validate-libraries workspace dependencies render-progress!)]
            (workspace/install-validated-libraries! workspace stale-lib-states)))))))

(defonce ^:private -initial-resource-sync-
  (run-and-measure-task!
    :resource-sync
    (workspace/resource-sync! workspace)
    nil))

(defonce project-graph-id (g/make-graph! :history true :volatility 1))

(defonce project
  (run-and-measure-task!
    :make-project
    (let [extensions (extensions/make project-graph-id)]
      (project/make-project project-graph-id workspace extensions))))

(defonce resources
  (run-and-measure-task!
    :list-resources
    (g/node-value project :resources)))

(defonce resource-node-ids
  (run-and-measure-task!
    :make-nodes
    (#'project/make-nodes! project resources)))

(defn- make-resource-node-progress-fn [^String label render-progress!]
  {:pre [(string? label)
         (fn? render-progress!)]}
  (first
    (#'project/make-progress-fns
      [[1 label]]
      (count resource-node-ids)
      render-progress!)))

(defonce ^:private -setup-project-
  (when (run-task? :setup-project)
    (when-let [game-project (project/get-resource-node project "/game.project")]
      (let [script-intel (project/script-intelligence project)]
        (g/transact
          (concat
            (g/connect script-intel :build-errors game-project :build-errors)
            (g/connect game-project :display-profiles-data project :display-profiles)
            (g/connect game-project :texture-profiles-data project :texture-profiles)
            (g/connect game-project :settings-map project :settings)))))))

(defonce node-load-infos
  (let [node-load-infos
        (when (run-task? :read-resources)
          (dev/run-with-progress "Reading Files..."
            (fn read-files-with-progress [render-progress!]
              (let [progress-reading! (make-resource-node-progress-fn "Reading" render-progress!)]
                (measure-task!
                  :read-resources
                  (#'project/read-node-load-infos resource-node-ids progress-reading! resource-metrics))))))

        node-load-infos
        (run-and-measure-task!
          :sort-source-values
          (#'project/sort-node-load-infos-for-loading node-load-infos {} {}))]

    (run-and-measure-task!
      :store-source-values
      (#'project/store-loaded-disk-sha256-hashes! node-load-infos workspace)
      (#'project/store-loaded-source-values! node-load-infos))

    node-load-infos))

(defonce migrated-resource-node-ids
  (let [migrated-resource-node-ids
        (when (run-task? :load-nodes)
          (dev/run-with-progress "Loading Nodes..."
            (fn load-nodes-with-progress [render-progress!]
              (let [progress-loading! (make-resource-node-progress-fn "Loading" render-progress!)]
                (measure-task!
                  :load-nodes
                  (#'project/load-nodes-into-graph! node-load-infos project progress-loading! resource-metrics transaction-metrics))))))

        _
        (run-and-measure-task!
          :cache-save-data
          (#'project/cache-loaded-save-data! node-load-infos project migrated-resource-node-ids))

        basis (g/now)

        migrated-proj-paths
        (into (sorted-set)
              (map #(resource/proj-path (resource-node/resource basis %)))
              migrated-resource-node-ids)]

    (when (pos? (count migrated-proj-paths))
      (log/info :message "Some files were migrated and will be saved in an updated format." :migrated-proj-paths migrated-proj-paths))

    migrated-resource-node-ids))

(defonce ^:private -reset-undo- (g/reset-undo! project-graph-id))

(defonce load-metrics
  (du/when-metrics
    {:new-nodes-by-path (g/node-value project :nodes-by-resource-path)
     :task-metrics @task-metrics
     :resource-metrics @resource-metrics
     :transaction-metrics @transaction-metrics}))
