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
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types]
            [internal.system :as is]
            [util.debug-util :as du]))

(set! *warn-on-reflection* true)

(defonce project-path "test/resources/empty_project")
(defonce system-config (shared-editor-settings/load-project-system-config project-path))
(defonce ^:private -set-system- (do (reset! g/*the-system* (is/make-system system-config)) nil))
(defonce workspace-graph-id (g/last-graph-added))
(defonce workspace (test-util/setup-workspace! workspace-graph-id project-path))

(defonce up-to-date-lib-states
  (dev/run-with-progress "Fetching Libraries..."
    (fn [render-progress!]
      (let [game-project-resource (workspace/find-resource workspace "/game.project")
            dependencies (project/read-dependencies game-project-resource)
            stale-lib-states (workspace/fetch-and-validate-libraries workspace dependencies render-progress!)]
        (workspace/install-validated-libraries! workspace stale-lib-states)))))

(defonce ^:private -initial-resource-sync- (do (workspace/resource-sync! workspace) nil))
(defonce project-graph-id (g/make-graph! :history true :volatility 1))

(defonce project
  (let [extensions (extensions/make project-graph-id)]
    (project/make-project project-graph-id workspace extensions)))

(defonce ^:private process-metrics (du/make-metrics-collector))
(defonce ^:private resource-metrics (du/make-metrics-collector))
(defonce ^:private transaction-metrics (du/make-metrics-collector))
(defonce resources (g/node-value project :resources))

(defonce resource-node-ids
  (du/measuring process-metrics :make-new-nodes
    (#'project/make-nodes! project resources)))

(defonce ^:private -init-project-
  (when-let [game-project (project/get-resource-node project "/game.project")]
    (let [script-intel (project/script-intelligence project)]
      (g/transact
        (concat
          (g/connect script-intel :build-errors game-project :build-errors)
          (g/connect game-project :display-profiles-data project :display-profiles)
          (g/connect game-project :texture-profiles-data project :texture-profiles)
          (g/connect game-project :settings-map project :settings))))))

(defonce load-ordered-node-load-infos
  (du/log-time "Project loading"
    (dev/run-with-progress "Loading Nodes..."
      (fn load-nodes-with-progress [render-progress!]
        (du/measuring process-metrics :load-new-nodes
          (#'project/load-nodes! project resource-node-ids render-progress! {} resource-metrics transaction-metrics))))))

(defonce load-metrics
  (du/when-metrics
    {:new-nodes-by-path (g/node-value project :nodes-by-resource-path)
     :process-metrics @process-metrics
     :resource-metrics @resource-metrics
     :transaction-metrics @transaction-metrics}))

(defonce ^:private -reset-undo- (g/reset-undo! project-graph-id))
