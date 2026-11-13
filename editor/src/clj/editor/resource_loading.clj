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

(ns editor.resource-loading
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.node :as in]
            [util.coll :as coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- throw-if-task-cancelled! [task-cancelled?]
  (when (task-cancelled?)
    (throw (ex-info "Resource load process cancelled." {:ex-type :task-cancelled}))))

(defn- read-resource-nodes! [project root-proj-paths required-endpoints render-load-progress! {:keys [basis-ref] :as evaluation-context}]
  (let [workspace (project/workspace project evaluation-context)
        nodes-by-resource-path (g/tx-cached-node-value! project :nodes-by-resource-path evaluation-context :tx-cache/reuse)

        {:keys [editable->type-ext->resource-type] :as read-opts}
        (workspace/evaluation-context-read-opts! workspace evaluation-context :tx-cache/reuse)

        node-id->dependency-node-ids
        (fn node-id->dependency-node-ids [node-id]
          (let [resource (resource-node/resource @basis-ref node-id)
                resource-type (resource/lookup-resource-type resource editable->type-ext->resource-type)
                proj-path (resource/proj-path resource)
                message (localization/message "progress.reading-resource" {"resource" proj-path})]
            (render-load-progress! (progress/make-cancellable-indeterminate message))
            (when (or (:dependencies-fn resource-type)
                      (:prerequisites-fn resource-type))
              (let [save-value (resource-io/evaluate-or-read-save-value! read-opts node-id resource evaluation-context)]
                (when-not (g/error-value? save-value)
                  (let [{:keys [dependency-proj-paths prerequisite-proj-paths]}
                        (project/resource-load-dependencies read-opts node-id resource resource-type save-value)]

                    (coll/into-> [dependency-proj-paths prerequisite-proj-paths] []
                      cat
                      (keep nodes-by-resource-path))))))))

        root-node-ids (coll/into-> root-proj-paths [] (keep nodes-by-resource-path))
        required-node-ids (g/required-shell-node-ids @basis-ref required-endpoints)
        root-node-ids (into root-node-ids required-node-ids)
        node-ids (project/resource-node-load-order root-node-ids node-id->dependency-node-ids)
        basis @basis-ref]

    (coll/into-> node-ids []
      (keep (fn [node-id]
              (when (in/shell-node? (g/node-by-id basis node-id))
                (let [resource (resource-node/resource basis node-id)
                      proj-path (resource/proj-path resource)]
                  (pair node-id proj-path))))))))

(defn load-resources!
  "Reads and materializes dependencies of resource paths and graph endpoints.

  `root-proj-paths` identifies the resources to prepare, including their
  serialized dependencies and prerequisites, in dependency order.

  `required-endpoints` contains graph endpoints created with `g/endpoint`.
  These identify additional outputs the caller will evaluate after the load
  process, whose dependencies might not be reachable from `root-proj-paths`.
  We traverse their value dependencies in the current graph basis, without
  evaluating outputs or consulting cached values, to find resource shells whose
  outputs require materialization. Those shells become additional roots for the
  same dependency discovery and materialization process. Connections that only
  require non-materializing outputs do not force their resources to be loaded.

  For example, a build supplies the project's `:build-errors` endpoint so that
  project validation resources and their dependencies are ready even when the
  built resource does not reference them. Pass an empty collection when only
  the resource paths and their dependencies need to be prepared.

  Run on a background thread; commits are handed off to the UI thread.
  The caller owns progress completion, allowing further preparation phases."
  [project root-proj-paths required-endpoints render-load-progress! task-cancelled?]
  (let [loading-message (localization/message "progress.loading")

        report-progress!
        (fn report-progress! [progress]
          (throw-if-task-cancelled! task-cancelled?)
          (render-load-progress! progress))]

    (report-progress! (progress/make-cancellable-indeterminate loading-message))
    (let [node-id+proj-path-pairs
          (g/with-evaluation-context [evaluation-context (g/make-evaluation-context)]
            ;; Initialize shared settings and profiles before dependent
            ;; resources cache their save values.
            (doseq [label [:settings :display-profiles :texture-profiles :use-font-layout :use-rich-text]]
              (g/node-value project label evaluation-context))
            (let [node-id+proj-path-pairs
                  (read-resource-nodes! project root-proj-paths required-endpoints report-progress! evaluation-context)]

              ;; Reuse these source values during materialization.
              (ui/run-now
                (g/update-system-from-evaluation-context! evaluation-context))
              node-id+proj-path-pairs))

          node-count (count node-id+proj-path-pairs)]

      (dotimes [index node-count]
        (let [[node-id proj-path] (node-id+proj-path-pairs index)
              message (localization/message "progress.loading-resource" {"resource" proj-path})]
          (report-progress! (progress/make message node-count index true))
          (g/with-evaluation-context [evaluation-context (g/make-evaluation-context)]
            (g/materialize-node! node-id evaluation-context)
            ;; Finish committing this resource even if cancellation arrived
            ;; during its load process. A later request can reuse the work.
            (ui/run-now
              (g/update-system-from-evaluation-context! evaluation-context)))))
      (throw-if-task-cancelled! task-cancelled?))
    nil))

(defn load-and-evaluate-resource!
  "Materializes a resource's dependencies, then traces and evaluates its output.

  `evaluated-output-label` identifies both the resource output to evaluate and
  the outputs counted during tracing to report evaluation progress. Tracing
  reports indeterminate progress; evaluation reports the traced work count.

  Run on a background thread; commits are handed off to the UI thread. Returns
  the evaluation's invalidation counters so the caller can reject a stale
  result. Cancelling the load process discards unfinished evaluation, while
  resources already materialized remain available for subsequent requests."
  [project resource-node proj-path evaluated-output-label render-progress! task-cancelled?]
  (load-resources! project [proj-path] [] render-progress! task-cancelled?)
  (g/with-evaluation-context [evaluation-context (g/make-evaluation-context)]
    (let [message (localization/message "progress.working")
          steps (volatile! [])

          collect-tracer-fn
          (fn collect-tracer-fn [state node-id output-type label]
            (throw-if-task-cancelled! task-cancelled?)
            (when (and (= :begin state)
                       (= :output output-type)
                       (= evaluated-output-label label))
              (vswap! steps conj node-id)))

          dry-run-evaluation-context
          (g/dry-run-evaluation-context evaluation-context collect-tracer-fn)]

      (render-progress! (progress/make-indeterminate message))
      (g/node-value resource-node evaluated-output-label dry-run-evaluation-context)
      (let [steps (coll/into-> @steps [] (distinct))
            step-count (count steps)
            progress-message-fn (constantly message)
            progress-tracer (project/make-progress-tracer evaluated-output-label step-count progress-message-fn render-progress!)

            evaluation-context-with-progress-trace
            (assoc evaluation-context
              :tracer
              (fn trace-progress [state node-id output-type label]
                (throw-if-task-cancelled! task-cancelled?)
                (progress-tracer state node-id output-type label)))]

        (render-progress! (progress/make message (max 1 step-count) 0))
        (doseq [node-id (rseq steps)]
          (g/node-value node-id evaluated-output-label evaluation-context-with-progress-trace))
        (g/node-value resource-node evaluated-output-label evaluation-context-with-progress-trace)
        (ui/run-now
          (throw-if-task-cancelled! task-cancelled?)
          (g/update-system-from-evaluation-context! evaluation-context))
        (:initial-invalidate-counters evaluation-context)))))
