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

(ns editor.build
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.transpilers :as code.transpilers]
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.pipeline :as pipeline]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.node :as in]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [java.util ArrayDeque HashMap HashSet]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- compiling-progress-message [node-id->resource-path node-id]
  (if (nil? node-id)
    (localization/message "progress.compiling")
    (when-some [resource-path (node-id->resource-path node-id)]
      (localization/message "progress.compiling-resource" {"resource" resource-path}))))

(defn- flatten-build-targets [build-targets]
  (let [seen (HashSet.)
        queue (doto (ArrayDeque.) (.add build-targets))]
    (loop [acc (transient [])]
      (if-let [batch (.pollFirst queue)]
        (recur (transduce
                 (filter
                   (fn [target]
                     (let [content-hash (target :content-hash)
                           not-seen-before (.add seen content-hash)]
                       (when not-seen-before
                         (assert (bt/content-hash? content-hash)
                                 (str "Build target has invalid content-hash: "
                                      (resource/resource->proj-path (target :resource))))
                         (let [deps (target :deps)]
                           (when-not (coll/empty? deps)
                             (.add queue deps))))
                       not-seen-before)))
                 conj!
                 acc
                 batch))
        (persistent! acc)))))

(defn- resolve-deps-impl
  ([build-targets project report-node-id-progress! evaluation-context]
   (let [proj-path->dynamic-build-targets (code.transpilers/build-output
                                            (project/code-transpilers @(:basis-ref evaluation-context))
                                            evaluation-context)]
     (if (g/error-value? proj-path->dynamic-build-targets)
       proj-path->dynamic-build-targets
       (let [ret (resolve-deps-impl (HashMap.) [] build-targets project proj-path->dynamic-build-targets report-node-id-progress! evaluation-context)]
         (cond-> ret (not (g/error-value? ret)) flatten-build-targets)))))
  ([^HashMap seen stack build-targets project proj-path->dynamic-build-targets report-node-id-progress! evaluation-context]
   (let [ret
         (if (g/error-value? build-targets)
           build-targets
           (coll/into-> build-targets []
             coll/flatten-xf
             (map (fn [build-target]
                    (if (g/error-value? build-target)
                      build-target
                      (let [content-hash (build-target :content-hash)]
                        (if-let [resolved (.get seen content-hash)]
                          resolved
                          (let [node-id (build-target :node-id)
                                _ (when report-node-id-progress! (report-node-id-progress! node-id))
                                result
                                (if (coll/some #(identical? build-target %) stack)
                                  (let [cycle (into []
                                                    (comp
                                                      (drop-while #(not (identical? % build-target)))
                                                      (keep (comp resource/proj-path :resource :resource)))
                                                    (conj stack build-target))]
                                    (g/map->error {:_node-id node-id
                                                   :_label :build-targets
                                                   :severity :fatal
                                                   :message (format "Dependency cycle detected: %s."
                                                                    (string/join " -> "
                                                                                 (map #(str \' % \')
                                                                                      cycle)))}))
                                  (let [deps (build-target :deps)
                                        dynamic-deps (build-target :dynamic-deps)
                                        resolved-deps
                                        (resolve-deps-impl
                                          seen
                                          (conj stack build-target)
                                          [deps
                                           (->Eduction
                                             (map (fn [proj-path]
                                                    (if-let [node (project/get-resource-node project proj-path evaluation-context)]
                                                      (g/node-value node :build-targets evaluation-context)
                                                      (or
                                                        (proj-path->dynamic-build-targets proj-path)
                                                        (resource-io/file-not-found-error node-id nil :fatal (project/resolve-path-or-resource project proj-path evaluation-context))))))
                                             dynamic-deps)]
                                          project
                                          proj-path->dynamic-build-targets
                                          report-node-id-progress!
                                          evaluation-context)]
                                    (if (g/error-value? resolved-deps)
                                      resolved-deps
                                      (-> build-target
                                          (assoc :deps resolved-deps)
                                          (dissoc :dynamic-deps)))))]
                            (.put seen content-hash result)
                            result))))))))]
     (or (g/flatten-errors ret) ret))))

(defn resolve-dependencies
  "Fully resolve build target dependencies

  Returns either:
    - a flat vector of all resolved build targets collected with a breadth-first
      order, de-duplicated by content hash
    - error value"
  ([build-targets project]
   (g/with-auto-evaluation-context evaluation-context
     (resolve-deps-impl build-targets project nil evaluation-context)))
  ([build-targets project evaluation-context]
   (resolve-deps-impl build-targets project nil evaluation-context)))

(defn resolve-node-dependencies
  "Fully resolve node's build target dependencies

  Returns either:
    - a flat vector of all resolved build targets collected with a breadth-first
      order, de-duplicated by content hash
    - error value"
  ([node project]
   (g/with-auto-evaluation-context evaluation-context
     (resolve-node-dependencies node project evaluation-context)))
  ([node project evaluation-context]
   (let [build-targets (g/node-value node :build-targets evaluation-context)]
     (resolve-deps-impl build-targets project nil evaluation-context))))

(defn- throw-build-cancelled-exception! []
  (throw (ex-info "Build cancelled." {:ex-type :task-cancelled})))

(defn- throw-if-task-cancelled! [task-cancelled?]
  (when (task-cancelled?)
    (throw-build-cancelled-exception!)))

(defn- cancellable-render-progress [render-progress! task-cancelled?]
  {:pre [(ifn? render-progress!)
         (ifn? task-cancelled?)]}
  (if (= fn/constantly-false task-cancelled?)
    render-progress!
    (fn cancellable-render-progress! [progress]
      (throw-if-task-cancelled! task-cancelled?)
      (render-progress! progress))))

(defn make-node-id->resource-path [project evaluation-context]
  (set/map-invert
    (g/tx-cached-node-value! project :nodes-by-resource-path evaluation-context :tx-cache/reuse)))

(defn- read-build-resource-nodes! [project root-proj-paths render-load-progress! {:keys [basis-ref] :as evaluation-context}]
  (let [workspace (project/workspace project evaluation-context)
        nodes-by-resource-path (g/tx-cached-node-value! project :nodes-by-resource-path evaluation-context :tx-cache/reuse)

        {:keys [editable->type-ext->resource-type] :as read-opts}
        (workspace/evaluation-context-read-opts! workspace evaluation-context :tx-cache/reuse)

        dependency-node-ids
        (fn dependency-node-ids [node-id]
          (let [resource (resource-node/resource @basis-ref node-id)
                resource-type (resource/lookup-resource-type resource editable->type-ext->resource-type)]
            (render-load-progress!
              (progress/make-cancellable-indeterminate
                (localization/message
                  "progress.reading-resource"
                  {"resource" (resource/proj-path resource)})))
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
        build-error-node-ids (g/required-shell-node-ids @basis-ref [(g/endpoint project :build-errors)])
        node-ids (project/resource-node-load-order (into root-node-ids build-error-node-ids) dependency-node-ids)
        basis @basis-ref]
    (coll/into-> node-ids []
      (keep (fn [node-id]
              (when (in/shell-node? (g/node-by-id basis node-id))
                (let [resource (resource-node/resource basis node-id)
                      proj-path (resource/proj-path resource)]
                  (pair node-id proj-path))))))))

(defn load-build-resources!
  "Reads the dependencies of the build roots and project validation resources,
  then materializes their shells in dependency order."
  [project root-proj-paths render-load-progress! task-cancelled?]
  (let [report-progress! (cancellable-render-progress render-load-progress! task-cancelled?)]
    (try
      (report-progress! (progress/make-cancellable-indeterminate (localization/message "progress.loading-project")))
      (let [nodes (g/with-evaluation-context [evaluation-context (g/make-evaluation-context)]
                    (let [nodes (read-build-resource-nodes! project root-proj-paths report-progress! evaluation-context)]
                      ;; Reuse these source values during materialization.
                      (ui/run-now
                        (g/update-system-from-evaluation-context! evaluation-context))
                      nodes))
            node-count (count nodes)]
        (dotimes [index node-count]
          (let [[node-id proj-path] (nodes index)
                message (localization/message "progress.loading-resource" {"resource" proj-path})]
            (report-progress! (progress/make message node-count index true))
            (g/with-evaluation-context [evaluation-context (g/make-evaluation-context)]
              (g/materialize-node! node-id evaluation-context)
              (ui/run-now
                (g/update-system-from-evaluation-context! evaluation-context)))))
        (throw-if-task-cancelled! task-cancelled?))
      (finally
        (render-load-progress! progress/done)))))

(defn node-build-targets [node-id node-id->resource-path render-progress! task-cancelled? evaluation-context]
  {:pre [(map? node-id->resource-path)
         (ifn? render-progress!)
         (ifn? task-cancelled?)]}
  (let [steps (atom [])

        collect-tracer-fn
        (fn collect-tracer-fn [state node output-type label]
          (throw-if-task-cancelled! task-cancelled?)
          (when (and (= :build-targets label)
                     (= :begin state)
                     (= :output output-type))
            (swap! steps conj node)))

        _ (g/node-value node-id :build-targets (g/dry-run-evaluation-context evaluation-context collect-tracer-fn))
        progress-message-fn (partial compiling-progress-message node-id->resource-path)
        step-count (count @steps)
        progress-tracer (project/make-progress-tracer :build-targets step-count progress-message-fn (progress/nest-render-progress render-progress! (progress/make localization/empty-message 10 0 true) 5))
        evaluation-context-with-progress-trace (assoc evaluation-context :tracer progress-tracer)]
    (doseq [node-id (rseq @steps)]
      (try
        (g/node-value node-id :build-targets evaluation-context-with-progress-trace)
        (catch Throwable error
          (throw (pipeline/decorate-build-exception error :compile node-id nil evaluation-context)))))
    (g/node-value node-id :build-targets evaluation-context)))

(defn- resolve-dependencies-with-progress [build-targets project node-id->resource-path render-progress! evaluation-context]
  {:pre [(map? node-id->resource-path)
         (ifn? render-progress!)]}
  (resolve-deps-impl
    build-targets
    project
    (fn report-resolve-deps-node-id-progress! [node-id]
      (when-let [path (node-id->resource-path node-id)]
        (render-progress! (progress/make-indeterminate (localization/message "progress.resolving-resource" {"resource" path})))))
    evaluation-context))

(defn build-build-targets! [build-targets workspace old-artifact-map render-progress! evaluation-context]
  {:pre [(ifn? render-progress!)]}
  (if (g/error? build-targets)
    {:error build-targets}
    (let [build-dir (workspace/build-path workspace)]
      (pipeline/build!
        build-targets
        build-dir
        old-artifact-map
        evaluation-context
        (progress/nest-render-progress
          render-progress!
          (progress/make localization/empty-message 10 5 true)
          5)))))

(defn build-project!
  "Builds node-id in the supplied snapshot. For load progress, run
  load-build-resources! before creating the build evaluation-context."
  [project node-id old-artifact-map opts evaluation-context]
  (let [[project-errors project-warnings]
        (->> (g/node-value project :build-errors evaluation-context)
             (g/unpack-errors)
             (:causes)
             (coll/separate-by g/error-fatal?))

        workspace (project/workspace project evaluation-context)
        extra-build-targets (:extra-build-targets opts)
        task-cancelled? (or (:task-cancelled? opts) fn/constantly-false)
        render-progress! (or (:render-progress! opts) progress/null-render-progress!)
        cancellable-render-progress! (cancellable-render-progress render-progress! task-cancelled?)
        node-id->resource-path (make-node-id->resource-path project evaluation-context)
        node-build-targets (node-build-targets node-id node-id->resource-path cancellable-render-progress! task-cancelled? evaluation-context)
        build-targets (resolve-dependencies-with-progress [node-build-targets extra-build-targets] project node-id->resource-path cancellable-render-progress! evaluation-context)
        result (if (coll/not-empty project-errors)
                 {:error (g/flatten-errors project-errors build-targets)}
                 (build-build-targets! build-targets workspace old-artifact-map cancellable-render-progress! evaluation-context))]

    (cond-> result
      (coll/not-empty project-warnings) (update :warning g/flatten-errors project-warnings))))
