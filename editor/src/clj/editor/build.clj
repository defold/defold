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
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [java.util ArrayDeque HashMap HashSet]))

(defn- batched-pmap [f batches]
  (->> batches
       (pmap (fn [batch] (doall (map f batch))))
       (reduce concat)
       doall))

(defn- available-processors []
  (.availableProcessors (Runtime/getRuntime)))

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
                                            (project/code-transpilers (:basis evaluation-context) project)
                                            evaluation-context)]
     (if (g/error-value? proj-path->dynamic-build-targets)
       proj-path->dynamic-build-targets
       (let [ret (resolve-deps-impl (HashMap.) [] build-targets project proj-path->dynamic-build-targets report-node-id-progress! evaluation-context)]
         (cond-> ret (not (g/error-value? ret)) flatten-build-targets)))))
  ([^HashMap seen stack build-targets project proj-path->dynamic-build-targets report-node-id-progress! evaluation-context]
   (let [ret
         (into
           []
           (comp
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
                            result)))))))
           build-targets)]
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
     (if (g/error-value? build-targets)
       build-targets
       (resolve-deps-impl build-targets project nil evaluation-context)))))

(defn- throw-build-cancelled-exception! []
  (throw (ex-info "Build cancelled." {:ex-type :task-cancelled})))

(defn build-project!
  [project node-id old-artifact-map opts evaluation-context]
  (let [extra-build-targets (:extra-build-targets opts)
        task-cancelled? (or (:task-cancelled? opts)
                            fn/constantly-false)
        supplied-render-progress! (or (:render-progress! opts)
                                      progress/null-render-progress!)

        render-progress!
        (if (= fn/constantly-false task-cancelled?)
          supplied-render-progress!
          (fn render-progress! [progress]
            (if (task-cancelled?)
              (throw-build-cancelled-exception!)
              (supplied-render-progress! progress))))

        steps (atom [])

        collect-tracer-fn
        (fn collect-tracer-fn [state node output-type label]
          (cond
            (task-cancelled?)
            (throw-build-cancelled-exception!)

            (and (= :build-targets label)
                 (= :begin state)
                 (= :output output-type))
            (swap! steps conj node)))

        _ (g/node-value node-id :build-targets (assoc evaluation-context :dry-run true :tracer collect-tracer-fn))
        node-id->resource-path (set/map-invert (g/node-value project :nodes-by-resource-path evaluation-context))
        progress-message-fn (partial compiling-progress-message node-id->resource-path)
        step-count (count @steps)
        progress-tracer (project/make-progress-tracer :build-targets step-count progress-message-fn (progress/nest-render-progress render-progress! (progress/make localization/empty-message 10 0 true) 5))
        evaluation-context-with-progress-trace (assoc evaluation-context :tracer progress-tracer)
        _ (doseq [node-id (rseq @steps)]
            (try
              (g/node-value node-id :build-targets evaluation-context-with-progress-trace)
              (catch Throwable error
                (throw (pipeline/decorate-build-exception error :compile node-id nil evaluation-context)))))
        #_#_#_#_
        prewarm-partitions (partition-all (max (quot step-count (+ (available-processors) 2)) 1000) (rseq @steps))
        _ (batched-pmap (fn [node-id] (g/node-value node-id :build-targets evaluation-context-with-progress-trace)) prewarm-partitions)
        node-build-targets (g/node-value node-id :build-targets evaluation-context)
        build-targets (resolve-deps-impl
                        [node-build-targets extra-build-targets]
                        project
                        (fn report-resolve-deps-node-id-progress! [node-id]
                          (when-let [path (node-id->resource-path node-id)]
                            (render-progress! (progress/make-indeterminate (localization/message "progress.resolving-resource" {"resource" path})))))
                        evaluation-context)]
    (if (g/error? build-targets)
      {:error build-targets}
      (let [build-dir (workspace/build-path (project/workspace project evaluation-context))]
        (pipeline/build! build-targets build-dir old-artifact-map evaluation-context (progress/nest-render-progress render-progress! (progress/make localization/empty-message 10 5 true) 5))))))
