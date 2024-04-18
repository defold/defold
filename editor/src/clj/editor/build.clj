;; Copyright 2020-2024 The Defold Foundation
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
            [editor.pipeline :as pipeline]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.workspace :as workspace]
            [util.coll :as coll])
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
    "Compiling..."
    (when-some [resource-path (node-id->resource-path node-id)]
      (str "Compiling " resource-path))))

(defn make-collect-progress-steps-tracer [watched-label steps-atom]
  (fn [state node output-type label]
    (when (and (= label watched-label) (= state :begin) (= output-type :output))
      (swap! steps-atom conj node))))

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
  ([build-targets project proj-path->dynamic-build-targets evaluation-context]
   (resolve-deps-impl (HashMap.) [] build-targets project proj-path->dynamic-build-targets evaluation-context))
  ([^HashMap seen stack build-targets project proj-path->dynamic-build-targets evaluation-context]
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
                          (let [result
                                (if (reduce #(if (identical? %2 build-target) (reduced true) %1) false stack)
                                  (let [cycle (into []
                                                    (comp
                                                      (drop-while #(not (identical? % build-target)))
                                                      (keep (comp resource/proj-path :resource :resource)))
                                                    (conj stack build-target))]
                                    (g/map->error {:_node-id (build-target :node-id)
                                                   :_label :build-targets
                                                   :severity :fatal
                                                   :message (str "Dependency cycle detected: " (string/join " -> " cycle))}))
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
                                                        (resource-io/file-not-found-error (build-target :node-id) nil :fatal (project/resolve-path-or-resource project proj-path evaluation-context))))))
                                             dynamic-deps)]
                                          project
                                          proj-path->dynamic-build-targets
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
     (resolve-dependencies build-targets project evaluation-context)))
  ([build-targets project evaluation-context]
   (let [proj-path->dynamic-build-targets (code.transpilers/build-output
                                            (project/code-transpilers (:basis evaluation-context) project)
                                            evaluation-context)]
     (if (g/error-value? proj-path->dynamic-build-targets)
       proj-path->dynamic-build-targets
       (let [ret (resolve-deps-impl build-targets project proj-path->dynamic-build-targets evaluation-context)]
         (cond-> ret (not (g/error-value? ret)) flatten-build-targets))))))

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
   (resolve-dependencies
     (g/node-value node :build-targets evaluation-context)
     project
     evaluation-context)))

(defn build-project!
  [project node evaluation-context extra-build-targets old-artifact-map render-progress!]
  (let [steps (atom [])
        collect-tracer (make-collect-progress-steps-tracer :build-targets steps)
        _ (g/node-value node :build-targets (assoc evaluation-context :dry-run true :tracer collect-tracer))
        progress-message-fn (partial compiling-progress-message (set/map-invert (g/node-value project :nodes-by-resource-path evaluation-context)))
        step-count (count @steps)
        progress-tracer (project/make-progress-tracer :build-targets step-count progress-message-fn (progress/nest-render-progress render-progress! (progress/make "" 10) 5))
        evaluation-context-with-progress-trace (assoc evaluation-context :tracer progress-tracer)
        _ (doseq [node-id (rseq @steps)]
            (g/node-value node-id :build-targets evaluation-context-with-progress-trace))
        #_#_#_#_
        prewarm-partitions (partition-all (max (quot step-count (+ (available-processors) 2)) 1000) (rseq @steps))
        _ (batched-pmap (fn [node-id] (g/node-value node-id :build-targets evaluation-context-with-progress-trace)) prewarm-partitions)
        node-build-targets (g/node-value node :build-targets evaluation-context)
        build-targets (resolve-dependencies [node-build-targets extra-build-targets] project evaluation-context)]
    (if (g/error? build-targets)
      {:error build-targets}
      (let [build-dir (workspace/build-path (project/workspace project))]
        (pipeline/build! build-targets build-dir old-artifact-map (progress/nest-render-progress render-progress! (progress/make "" 10 5) 5))))))
