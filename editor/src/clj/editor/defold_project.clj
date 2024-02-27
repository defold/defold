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

(ns editor.defold-project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.script-intelligence :as si]
            [editor.collision-groups :as collision-groups]
            [editor.core :as core]
            [editor.game-project-core :as gpc]
            [editor.gl :as gl]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.library :as library]
            [editor.lsp :as lsp]
            [editor.placeholder-resource :as placeholder-resource]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.resource-update :as resource-update]
            [editor.settings-core :as settings-core]
            [editor.ui :as ui]
            [editor.util :as util]
            [editor.workspace :as workspace]
            [internal.util :as iutil]
            [schema.core :as s]
            [service.log :as log]
            [util.coll :refer [pair]]
            [util.debug-util :as du]
            [util.fn :as fn]
            [util.thread-util :as thread-util])
  (:import [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)

(def ^:dynamic *load-cache* nil)

(defonce load-metrics-atom (du/when-metrics (atom nil)))
(defonce resource-change-metrics-atom (du/when-metrics (atom nil)))

(def ^:private TBreakpoint
  {:resource s/Any
   :row Long
   (s/optional-key :condition) String})

(g/deftype Breakpoints [TBreakpoint])

(defn graph [project]
  (g/node-id->graph-id project))

(defn- resource-type->node-type [resource-type]
  (or (:node-type resource-type)
      placeholder-resource/PlaceholderResourceNode))

(def resource-node-type (comp resource-type->node-type resource/resource-type))

(defn- mark-node-file-not-found [node-id resource]
  (let [node-type (resource-node-type resource)
        error-value (resource-io/file-not-found-error node-id nil :fatal resource)]
    (g/mark-defective node-id node-type error-value)))

(defn- mark-node-invalid-content [node-id resource exception]
  (let [node-type (resource-node-type resource)
        error-value (resource-io/invalid-content-error node-id nil :fatal resource exception)]
    (g/mark-defective node-id node-type error-value)))

(defn- load-resource-node [project resource-node-id resource source-value]
  (try
    (if (or (= :folder (resource/source-type resource))
            (not (resource/exists? resource)))
      (do
        ;; TODO(save-value): This shouldn't be able to happen anymore, right?
        (assert (and (not= :folder (resource/source-type resource))
                     (resource/exists? resource)))
        (mark-node-file-not-found resource-node-id resource))
      (try
        (let [{:keys [read-fn load-fn] :as resource-type} (resource/resource-type resource)]
          (cond-> []

                  load-fn
                  (into (flatten
                          (if (nil? read-fn)
                            (load-fn project resource-node-id resource)
                            (load-fn project resource-node-id resource source-value))))

                  (and (resource/file-resource? resource)
                       (resource/editable? resource)
                       (:auto-connect-save-data? resource-type))
                  (into (g/connect resource-node-id :save-data project :save-data))

                  :always
                  not-empty))
        (catch Exception exception
          (log/warn :msg (format "Unable to load resource '%s'" (resource/proj-path resource)) :exception exception)
          (mark-node-invalid-content resource-node-id resource exception))))
    (catch Throwable throwable
      (let [proj-path (resource/proj-path resource)]
        (throw (ex-info (format "Error when loading resource '%s'" proj-path)
                        {:node-type (resource-node-type resource)
                         :proj-path proj-path}
                        throwable))))))

(defn load-embedded-resource-node [project embedded-resource-node-id embedded-resource source-value]
  (let [embedded-resource-type (resource/resource-type embedded-resource)
        load-fn (:load-fn embedded-resource-type)]
    (load-fn project embedded-resource-node-id embedded-resource source-value)))

(defn- node-load-dependencies
  "Returns the node-ids that are immediate dependencies of specified node-id.

  `resource-node-ids-by-proj-path` is a map from proj-path to:
    * new node-id if the resource is being reloaded.
    * old node-id if the resource is not being reloaded.

  `old-resource-node-dependencies` is a function from an already loaded node-id
  to the vector of proj-paths it currently depends on.

  `new-resource-node-dependencies` is a function from a not-yet-loaded node-id
  to the vector of proj-paths it depends on in the file contents on disk. If the
  node is not being reloaded, the function must return nil."
  [node-id resource-node-ids-by-proj-path old-resource-node-dependencies new-resource-node-dependencies]
  (into []
        (keep resource-node-ids-by-proj-path)
        (or (new-resource-node-dependencies node-id)
            (old-resource-node-dependencies node-id))))

(defn read-node-load-info [node-id resource resource-metrics]
  (let [{:keys [lazy-loaded read-fn] :as resource-type} (resource/resource-type resource)

        read-result
        (when (and read-fn
                   (not lazy-loaded))
          (try
            (if (or (= :folder (resource/source-type resource))
                    (not (resource/exists? resource)))
              (do
                ;; TODO(save-value): This shouldn't be able to happen anymore, right?
                (assert (and (not= :folder (resource/source-type resource))
                             (resource/exists? resource)))
                (resource-io/file-not-found-error node-id nil :fatal resource))
              (try
                (du/measuring resource-metrics (resource/proj-path resource) :read-source-value
                  (resource/read-source-value+sha256-hex resource read-fn))
                (catch Exception exception
                  (log/warn :msg (format "Unable to read resource '%s'" (resource/proj-path resource)) :exception exception)
                  (resource-io/invalid-content-error node-id nil :fatal resource exception))))
            (catch Throwable throwable
              (let [proj-path (resource/proj-path resource)]
                (throw (ex-info (format "Error when reading resource '%s'" proj-path)
                                {:node-type (:node-type resource-type)
                                 :proj-path proj-path}
                                throwable))))))

        [source-value disk-sha256 read-error]
        (if (g/error-value? read-result)
          [nil nil read-result]
          read-result)

        dependency-proj-paths
        (when (some? source-value)
          (when-let [dependencies-fn (:dependencies-fn resource-type)]
            (try
              (du/measuring resource-metrics (resource/proj-path resource) :find-new-reload-dependencies
                (not-empty (vec (dependencies-fn source-value))))
              (catch Exception exception
                (log/warn :msg (format "Unable to determine dependencies for resource '%s', assuming none."
                                       (resource/proj-path resource))
                          :exception exception)
                nil))))]

    (cond-> {:node-id node-id
             :resource resource}
            read-error (assoc :read-error read-error)
            source-value (assoc :source-value source-value)
            disk-sha256 (assoc :disk-sha256 disk-sha256)
            dependency-proj-paths (assoc :dependency-proj-paths dependency-proj-paths))))

(defn- sort-node-ids-for-loading
  ([node-ids node-id->dependency-node-ids]
   (first (sort-node-ids-for-loading node-ids #{} [] #{} (set node-ids) node-id->dependency-node-ids)))
  ([node-ids in-progress queue queued batch node-id->dependency-node-ids]
   (let [node-id (first node-ids)]
     (if (nil? node-id)
       [queue queued]
       ;; TODO: Handle recursive dependencies properly. Here we treat
       ;; a recurring node-id as "already loaded", which might not be
       ;; correct. Maybe log? Keep information about circular
       ;; dependency to be used in get-resource-node etc?
       (if (or (contains? queued node-id) (contains? in-progress node-id))
         (recur (rest node-ids) in-progress queue queued batch node-id->dependency-node-ids)
         (let [deps (node-id->dependency-node-ids node-id)
               [dep-queue dep-queued] (sort-node-ids-for-loading deps (conj in-progress node-id) queue queued batch node-id->dependency-node-ids)]
           (recur (rest node-ids)
                  in-progress
                  (if (contains? batch node-id) (conj dep-queue node-id) dep-queue)
                  (conj dep-queued node-id)
                  batch
                  node-id->dependency-node-ids)))))))

(defn- node-load-info-tx-data [{:keys [node-id read-error resource] :as node-load-info} project]
  ;; TODO(save-value): This should no longer be true - we should always have a committed resource-node-id.
  ;; Note that resource-node-id here may be temporary (a make-node not
  ;; g/transact'ed) here, so we can't use (node-value resource-node-id :...)
  ;; to inspect it. That's why we pass in node-type, resource.
  (assert (some? *load-cache*))

  ;; TODO(save-value): We shouldn't have duplicate calls anymore, right?
  (assert (not (contains? @*load-cache* node-id)))

  ;; Don't generate duplicate tx-data for nodes that have already been loaded.
  (when-not (contains? @*load-cache* node-id)
    (swap! *load-cache* conj node-id)
    (if read-error
      (let [node-type (resource-node-type resource)]
        (g/mark-defective node-id node-type read-error))
      (let [source-value (:source-value node-load-info)]
        (load-resource-node project node-id resource source-value)))))

(defn- sort-node-load-infos-for-loading [node-load-infos old-resource-node-ids-by-proj-path old-resource-node-dependencies]
  {:pre [(map? old-resource-node-ids-by-proj-path)
         (ifn? old-resource-node-dependencies)]}
  (let [[node-id->node-load-info
         resource-node-ids-by-proj-path]
        (iutil/into-multiple
          [{}
           old-resource-node-ids-by-proj-path]
          [(map (juxt :node-id identity))
           (map (fn [{:keys [node-id resource]}]
                  (let [proj-path (resource/proj-path resource)]
                    (pair proj-path node-id))))]
          node-load-infos)

        new-resource-node-dependencies
        (fn new-resource-node-dependencies [node-id]
          ;; We want to return nil if the node-id is not known to us, and an empty vector otherwise.
          (when-let [node-load-info (node-id->node-load-info node-id)]
            (or (:dependency-proj-paths node-load-info) [])))

        node-id->dependency-node-ids (fn/memoize #(node-load-dependencies % resource-node-ids-by-proj-path old-resource-node-dependencies new-resource-node-dependencies))
        node-id-load-order (sort-node-ids-for-loading (keys node-id->node-load-info) node-id->dependency-node-ids)]
    (mapv node-id->node-load-info node-id-load-order)))

(defn- load-tx-data [node-load-infos project progress-loading! progress-processing! resource-metrics]
  (let [resource-metrics-load-timer
        (du/when-metrics
          (volatile! 0))

        start-resource-metrics-load-timer!
        (du/when-metrics
          (fn []
            (vreset! resource-metrics-load-timer (System/nanoTime))))

        stop-resource-metrics-load-timer!
        (du/when-metrics
          (fn [resource-path]
            (let [end-time (System/nanoTime)
                  ^long start-time @resource-metrics-load-timer]
              (du/update-metrics resource-metrics resource-path :process-load-tx-data (- end-time start-time)))))]
    (doall
      (for [[node-index node-load-info] (map-indexed #(pair (inc %1) %2) node-load-infos)]
        (let [resource (:resource node-load-info)
              proj-path (resource/proj-path resource)]
          (progress-loading! node-index proj-path)
          (du/if-metrics
            [(g/callback progress-processing! node-index proj-path)
             (g/callback start-resource-metrics-load-timer!)
             (du/measuring resource-metrics proj-path :generate-load-tx-data
               (node-load-info-tx-data node-load-info project))
             (g/callback stop-resource-metrics-load-timer! proj-path)]
            [(g/callback progress-processing! node-index proj-path)
             (node-load-info-tx-data node-load-info project)]))))))

(defn- read-node-load-infos [node-ids progress-reading! resource-metrics]
  (let [basis (g/now)]
    (into []
          (map-indexed (fn [node-index node-id]
                         (let [resource (resource-node/resource basis node-id)
                               proj-path (resource/proj-path resource)]
                           (progress-reading! (inc node-index) proj-path)
                           (read-node-load-info node-id resource resource-metrics))))
          node-ids)))

(defn- disk-sha256s-by-node-id [node-load-infos]
  (into {}
        (keep (fn [{:keys [resource] :as node-load-info}]
                (when (and (resource/file-resource? resource)
                           (resource/stateful? resource))
                  (let [{:keys [disk-sha256 node-id]} node-load-info]
                    (pair node-id disk-sha256)))))
        node-load-infos))

(defn- store-loaded-disk-sha256-hashes! [node-load-infos workspace]
  (let [disk-sha256s-by-node-id (disk-sha256s-by-node-id node-load-infos)]
    (g/transact
      (workspace/merge-disk-sha256s workspace disk-sha256s-by-node-id))))

(defn- save-data-tracked-resource? [resource]
  ;; The source-value output will never be evaluated for non-editable or
  ;; stateless resources, so there is no need to store their entries.
  ;; TODO(save-value): PlaceholderResourceNodes connect to :save-data only if non-binary.
  (and (resource/file-resource? resource)
       (resource/editable? resource)
       (resource/stateful? resource)))

(defn- store-loaded-source-values! [node-load-infos]
  (let [node-id+source-value-pairs
        (into []
              (keep (fn [{:keys [node-id read-error resource] :as node-load-info}]
                      (when (save-data-tracked-resource? resource)
                        (pair node-id
                              (or read-error
                                  (let [{:keys [resource source-value]} node-load-info
                                        resource-type (resource/resource-type resource)]
                                    ;; Note: Here, source-value is whatever was
                                    ;; returned by the read-fn, so it's
                                    ;; technically a save-value.
                                    (resource-node/save-value->source-value source-value resource-type)))))))
              node-load-infos)]

    (resource-node/merge-source-values! node-id+source-value-pairs)))

(defn log-cache-info! [cache message]
  ;; Disabled during tests to minimize log spam.
  (when-not (Boolean/getBoolean "defold.tests")
    (let [{:keys [total retained unretained limit]} (g/cache-info cache)]
      (log/info :message message
                :total total
                :retained retained
                :unretained unretained
                :limit limit))))

(defn- cache-loaded-save-data! [node-load-infos project excluded-resource-node-id?]
  (let [basis (g/now)

        cached-resource-node-id?
        (into #{}
              (comp (map first)
                    (remove excluded-resource-node-id?))
              (g/sources-of basis project :save-data))

        endpoint+cached-value-pairs
        (into []
              (mapcat (fn [{:keys [node-id source-value] :as node-load-info}]
                        (when (cached-resource-node-id? node-id)
                          (let [{:keys [read-error resource]} node-load-info

                                save-data-endpoint+cached-value
                                (pair (g/endpoint node-id :save-data)
                                      (or read-error
                                          (resource-node/make-save-data node-id resource source-value false)))

                                is-save-value-output-cached
                                (-> (g/node-type* basis node-id)
                                    (g/cached-outputs)
                                    (contains? :save-value))]

                            (cond-> [save-data-endpoint+cached-value]

                                    is-save-value-output-cached
                                    (conj (pair (g/endpoint node-id :save-value)
                                                (or read-error source-value))))))))
              node-load-infos)]

    (g/cache-output-values! endpoint+cached-value-pairs)
    (log-cache-info! (g/cache) "Cached loaded save data in system cache.")))

(defn- load-nodes-into-graph! [node-load-infos project progress-loading! progress-processing! resource-metrics transaction-metrics]
  (let [tx-data (load-tx-data node-load-infos project progress-loading! progress-processing! resource-metrics)
        tx-opts (du/when-metrics {:metrics transaction-metrics})
        tx-result (g/transact tx-opts tx-data)
        basis (:basis tx-result)]
    ;; Return the set of migrated resource node ids, if any.
    (into #{}
          (keep #(resource-node/owner-resource-node-id basis %))
          (g/migrated-node-ids tx-result))))

(defn- make-progress-fns [task-allocations loaded-node-count render-progress!]
  (let [total-task-size (transduce (map first) + task-allocations)]
    (second
      (reduce (fn [[accumulated-task-size progress-fns] [task-size task-label]]
                (let [parent-progress (progress/make "" total-task-size accumulated-task-size)
                      render-nested-progress! (progress/nest-render-progress render-progress! parent-progress task-size)]
                  (pair (+ accumulated-task-size task-size)
                        (conj progress-fns
                              (fn progress-fn [loaded-node-index proj-path]
                                (render-nested-progress!
                                  (progress/make (str task-label \space proj-path)
                                                 loaded-node-count
                                                 loaded-node-index)))))))
              (pair 0 [])
              task-allocations))))

(declare workspace)

(defn- load-nodes! [project node-ids render-progress! old-resource-node-dependencies resource-metrics transaction-metrics]
  (let [workspace (workspace project)
        old-resource-node-ids-by-proj-path (g/node-value project :nodes-by-resource-path)

        [progress-reading!
         progress-loading!
         progress-processing!]
        (make-progress-fns [[3 "Reading"]
                            [1 "Loading"]
                            [1 "Processing"]]
                           (count node-ids)
                           render-progress!)

        node-load-infos
        (-> node-ids
            (read-node-load-infos progress-reading! resource-metrics)
            (sort-node-load-infos-for-loading old-resource-node-ids-by-proj-path old-resource-node-dependencies))]

    (store-loaded-disk-sha256-hashes! node-load-infos workspace)
    (store-loaded-source-values! node-load-infos)
    (let [migrated-resource-node-ids (load-nodes-into-graph! node-load-infos project progress-loading! progress-processing! resource-metrics transaction-metrics)
          basis (g/now)
          migrated-proj-paths (into (sorted-set)
                                    (map #(resource/proj-path (resource-node/resource basis %)))
                                    migrated-resource-node-ids)]
      (cache-loaded-save-data! node-load-infos project migrated-resource-node-ids)
      (render-progress! progress/done)

      ;; Log any migrated proj-paths.
      ;; Disabled during tests to minimize log spam.
      (when (and (pos? (count migrated-proj-paths))
                 (not (Boolean/getBoolean "defold.tests")))
        (log/info :message "Some files were migrated and will be saved in an updated format." :migrated-proj-paths migrated-proj-paths)))))

(defn connect-if-output [src-type src tgt connections]
  (let [outputs (g/output-labels src-type)]
    (for [[src-label tgt-label] connections
          :when (contains? outputs src-label)]
      (g/connect src src-label tgt tgt-label))))

(defn- make-nodes! [project resources]
  (let [project-graph (graph project)
        file-resources (filter #(= :file (resource/source-type %)) resources)]
    (g/tx-nodes-added
      (g/transact
        (for [[resource-type resources] (group-by resource/resource-type file-resources)
              :let [node-type (resource-type->node-type resource-type)]
              resource resources]
          (g/make-nodes project-graph [node [node-type :resource resource]]
                        (g/connect node :_node-id project :nodes)
                        (g/connect node :node-id+resource project :node-id+resources)))))))

(defn get-resource-node
  ([project path-or-resource]
   (g/with-auto-evaluation-context ec
     (get-resource-node project path-or-resource ec)))
  ([project path-or-resource evaluation-context]
   (when-let [resource (cond
                         (string? path-or-resource) (workspace/find-resource (g/node-value project :workspace evaluation-context) path-or-resource evaluation-context)
                         (satisfies? resource/Resource path-or-resource) path-or-resource
                         :else (assert false (str (type path-or-resource) " is neither a path nor a resource: " (pr-str path-or-resource))))]
     (let [nodes-by-resource-path (g/node-value project :nodes-by-resource-path evaluation-context)]
       (get nodes-by-resource-path (resource/proj-path resource))))))

(defn workspace
  ([project]
   (g/with-auto-evaluation-context evaluation-context
     (workspace project evaluation-context)))
  ([project evaluation-context]
   (g/node-value project :workspace evaluation-context)))

(defn code-preprocessors
  ([project]
   (g/with-auto-evaluation-context evaluation-context
     (code-preprocessors project evaluation-context)))
  ([project evaluation-context]
   (let [workspace (workspace project evaluation-context)]
     (workspace/code-preprocessors workspace evaluation-context))))

(defn script-intelligence
  ([project]
   (g/with-auto-evaluation-context evaluation-context
     (script-intelligence project evaluation-context)))
  ([project evaluation-context]
   (g/node-value project :script-intelligence evaluation-context)))

(defn load-project
  ([project]
   (load-project project (g/node-value project :resources)))
  ([project resources]
   (load-project project resources progress/null-render-progress!))
  ([project resources render-progress!]
   (assert (empty? (g/node-value project :nodes)) "load-project should only be used when loading an empty project")
   (with-bindings {#'*load-cache* (atom (into #{} (g/node-value project :nodes)))}
     ;; Create nodes for all resources in the workspace.
     (let [process-metrics (du/make-metrics-collector)
           resource-metrics (du/make-metrics-collector)
           transaction-metrics (du/make-metrics-collector)
           nodes (du/measuring process-metrics :make-new-nodes
                   (make-nodes! project resources))]

       ;; Make sure the game.project node is property connected before loading
       ;; the resource nodes, since establishing these connections will
       ;; invalidate any dependent outputs in the cache.
       (when-let [game-project (get-resource-node project "/game.project")]
         (let [script-intel (script-intelligence project)]
           (g/transact
             (concat
               (g/connect script-intel :build-errors game-project :build-errors)
               (g/connect game-project :display-profiles-data project :display-profiles)
               (g/connect game-project :texture-profiles-data project :texture-profiles)
               (g/connect game-project :settings-map project :settings)))))

       ;; Load the resource nodes. Referenced nodes will be loaded prior to
       ;; nodes that refer to them, provided the :dependencies-fn reports the
       ;; referenced proj-paths correctly.
       ;;
       ;; TODO(save-value-cleanup): There are implicit dependencies between
       ;; texture profiles and image resources. We probably want to ensure the
       ;; texture profiles are loaded before anything that makes implicit use of
       ;; them to avoid potentially costly cache invalidation.
       (du/measuring process-metrics :load-new-nodes
         (load-nodes! project nodes render-progress! {} resource-metrics transaction-metrics))
       (du/when-metrics
         (reset! load-metrics-atom
                 {:new-nodes-by-path (g/node-value project :nodes-by-resource-path)
                  :process-metrics @process-metrics
                  :resource-metrics @resource-metrics
                  :transaction-metrics @transaction-metrics}))
       project))))

(defn make-embedded-resource [project editability ext data]
  (let [workspace (g/node-value project :workspace)]
    (workspace/make-embedded-resource workspace editability ext data)))

(defn all-save-data
  ([project]
   (g/node-value project :save-data))
  ([project evaluation-context]
   (g/node-value project :save-data evaluation-context)))

(defn dirty-save-data
  ([project]
   (g/node-value project :dirty-save-data))
  ([project evaluation-context]
   (g/node-value project :dirty-save-data evaluation-context)))

(declare make-count-progress-steps-tracer make-progress-tracer)

(defn save-data-with-progress [project evaluation-context save-data-fn render-progress!]
  {:pre [(ifn? save-data-fn)
         (ifn? render-progress!)]}
  (ui/with-progress [render-progress! render-progress!]
    (let [step-count (AtomicLong.)
          step-count-tracer (make-count-progress-steps-tracer :save-data step-count)
          progress-message-fn (constantly "Saving...")]
      (render-progress! (progress/make "Saving..."))
      (save-data-fn project (assoc evaluation-context :dry-run true :tracer step-count-tracer))
      (let [progress-tracer (make-progress-tracer :save-data (.get step-count) progress-message-fn render-progress!)]
        (save-data-fn project (assoc evaluation-context :tracer progress-tracer))))))

(defn make-count-progress-steps-tracer [watched-label ^AtomicLong step-count]
  (fn [state node output-type label]
    (when (and (= label watched-label) (= state :begin) (= output-type :output))
      (.getAndIncrement step-count))))

(defn make-progress-tracer [watched-label step-count progress-message-fn render-progress!]
  (let [steps-done (atom #{})
        initial-progress-message (progress-message-fn nil)
        progress (atom (progress/make initial-progress-message step-count))]
    (fn [state node output-type label]
      (when (and (= label watched-label) (= output-type :output))
        (case state
          :begin
          (let [progress-message (progress-message-fn node)]
            (render-progress! (swap! progress
                                     #(progress/with-message % (or progress-message
                                                                   (progress/message %)
                                                                   "")))))

          :end
          (let [already-done (loop []
                               (let [old @steps-done
                                     new (conj old node)]
                                 (if (compare-and-set! steps-done old new)
                                   (get old node)
                                   (recur))))]
            (when-not already-done
              (render-progress! (swap! progress progress/advance 1))))

          :fail
          nil)))))

(handler/defhandler :undo :global
  (enabled? [project-graph] (g/has-undo? project-graph))
  (run [project-graph]
    (g/undo! project-graph)
    (lsp/check-if-polled-resources-are-modified! (lsp/get-graph-lsp project-graph))))

(handler/defhandler :redo :global
  (enabled? [project-graph] (g/has-redo? project-graph))
  (run [project-graph]
    (g/redo! project-graph)
    (lsp/check-if-polled-resources-are-modified! (lsp/get-graph-lsp project-graph))))

(def ^:private bundle-targets
  (into []
        (concat (when (util/is-mac-os?) [[:ios "iOS Application..."]]) ; macOS is required to sign iOS ipa.
                [[:android "Android Application..."]
                 [:macos   "macOS Application..."]
                 [:windows "Windows Application..."]
                 [:linux   "Linux Application..."]
                 [:html5   "HTML5 Application..."]])))

(handler/register-menu! ::menubar :editor.app-view/view
  [{:label "Project"
    :id ::project
    :children [{:label "Build"
                :command :build}
               {:label "Rebuild"
                :command :rebuild}
               {:label "Build HTML5"
                :command :build-html5}
               {:label "Bundle"
                :children (mapv (fn [[platform label]]
                                  {:label label
                                   :command :bundle
                                   :user-data {:platform platform}})
                                bundle-targets)}
               {:label "Rebundle"
                :command :rebundle}
               {:label "Fetch Libraries"
                :command :fetch-libraries}
               {:label "Reload Editor Scripts"
                :command :reload-extensions}
               {:label :separator}
               {:label "Shared Editor Settings"
                :command :shared-editor-settings}
               {:label "Live Update Settings"
                :command :live-update-settings}
               {:label :separator
                :id ::project-end}]}])

(defn- update-selection [s open-resource-nodes active-resource-node selection-value]
  (->> (assoc s active-resource-node selection-value)
    (filter (comp (set open-resource-nodes) first))
    (into {})))

(defn- perform-selection [project all-selections]
  (let [all-node-ids (->> all-selections
                       vals
                       (reduce into [])
                       distinct
                       vec)
        old-all-selections (g/node-value project :all-selections)]
    (when-not (= old-all-selections all-selections)
      (concat
        (g/set-property project :all-selections all-selections)
        (for [[node-id label] (g/sources-of project :all-selected-node-ids)]
          (g/disconnect node-id label project :all-selected-node-ids))
        (for [[node-id label] (g/sources-of project :all-selected-node-properties)]
          (g/disconnect node-id label project :all-selected-node-properties))
        (for [node-id all-node-ids]
          (concat
            (g/connect node-id :_node-id    project :all-selected-node-ids)
            (g/connect node-id :_properties project :all-selected-node-properties)))))))

(defn select
  ([project resource-node node-ids open-resource-nodes]
   (assert (every? some? node-ids) "Attempting to select nil values")
   (let [node-ids (if (seq node-ids)
                    (-> node-ids distinct vec)
                    [resource-node])
         all-selections (-> (g/node-value project :all-selections)
                            (update-selection open-resource-nodes resource-node node-ids))]
     (perform-selection project all-selections))))

(defn- perform-sub-selection
  ([project all-sub-selections]
   (g/set-property project :all-sub-selections all-sub-selections)))

(defn sub-select
  ([project resource-node sub-selection open-resource-nodes]
   (g/update-property project :all-sub-selections update-selection open-resource-nodes resource-node sub-selection)))

(defn- remap-selection [m key-m val-fn]
  (reduce (fn [m [old new]]
            (if-let [v (get m old)]
              (-> m
                (dissoc old)
                (assoc new (val-fn [new v])))
              m))
    m key-m))

(def ^:private make-resource-nodes-by-path-map
  (partial into {} (map (juxt (comp resource/proj-path second) first))))

(defn- perform-resource-change-plan [plan project render-progress!]
  (binding [*load-cache* (atom (into #{} (g/node-value project :nodes)))]
    (let [process-metrics (du/make-metrics-collector)
          resource-metrics (du/make-metrics-collector)
          transaction-metrics (du/make-metrics-collector)
          transact-opts (du/when-metrics {:metrics transaction-metrics})

          collected-properties-by-resource
          (du/measuring process-metrics :collect-overridden-properties
            (g/with-auto-evaluation-context evaluation-context
              (into {}
                    (map (fn [[resource old-node-id]]
                           [resource
                            (du/measuring resource-metrics (resource/proj-path resource) :collect-overridden-properties
                              (g/collect-overridden-properties old-node-id evaluation-context))]))
                    (:transfer-overrides plan))))

          old-nodes-by-path (g/node-value project :nodes-by-resource-path)
          rn-dependencies-evaluation-context (g/make-evaluation-context)
          old-resource-node-dependencies (fn/memoize
                                           (fn old-resource-node-dependencies [node-id]
                                             (let [resource (g/node-value node-id :resource rn-dependencies-evaluation-context)]
                                               (du/measuring resource-metrics (resource/proj-path resource) :find-old-reload-dependencies
                                                 (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
                                                   (let [save-value (g/node-value node-id :save-value rn-dependencies-evaluation-context)]
                                                     (when-not (g/error? save-value)
                                                       (dependencies-fn save-value))))))))
          resource->old-node (comp old-nodes-by-path resource/proj-path)
          new-nodes (du/measuring process-metrics :make-new-nodes
                      (make-nodes! project (:new plan)))
          resource-path->new-node (g/with-auto-evaluation-context evaluation-context
                                    (into {}
                                          (map (fn [resource-node]
                                                 (let [resource (g/node-value resource-node :resource evaluation-context)]
                                                   [(resource/proj-path resource) resource-node])))
                                          new-nodes))
          resource->new-node (comp resource-path->new-node resource/proj-path)
          ;; when transferring overrides and arcs, the target is either a newly created or already (still!)
          ;; existing node.
          resource->node (fn [resource]
                           (or (resource->new-node resource)
                               (resource->old-node resource)))]
      ;; Transfer of overrides must happen before we delete the original nodes below.
      ;; The new target nodes do not need to be loaded. When loading the new targets,
      ;; corresponding override-nodes for the incoming connections will be created in the
      ;; overrides.
      (du/measuring process-metrics :transfer-overrides
        (g/transact transact-opts
          (du/if-metrics
            ;; Doing metrics - Submit separate transaction steps for each resource.
            (for [[resource old-node-id] (:transfer-overrides plan)]
              (g/transfer-overrides {old-node-id (resource->node resource)}))

            ;; Not doing metrics - submit as a single transaction step.
            (g/transfer-overrides
              (into {}
                    (map (fn [[resource old-node-id]]
                           [old-node-id (resource->node resource)]))
                    (:transfer-overrides plan))))))

      ;; must delete old versions of resource nodes before loading to avoid
      ;; load functions finding these when doing lookups of dependencies...
      (du/measuring process-metrics :delete-old-nodes
        (g/transact transact-opts
          (for [node (:delete plan)]
            (g/delete-node node))))

      (du/measuring process-metrics :load-new-nodes
        (load-nodes! project new-nodes render-progress! old-resource-node-dependencies resource-metrics transaction-metrics))

      (du/measuring process-metrics :update-cache
        (g/update-cache-from-evaluation-context! rn-dependencies-evaluation-context))

      (du/measuring process-metrics :transfer-outgoing-arcs
        (g/transact transact-opts
          (for [[source-resource output-arcs] (:transfer-outgoing-arcs plan)]
            (let [source-node (resource->node source-resource)
                  existing-arcs (set (gu/explicit-outputs source-node))]
              (for [[source-label [target-node target-label]] (remove existing-arcs output-arcs)]
                ;; if (g/node-by-id target-node), the target of the outgoing arc
                ;; has not been deleted above - implying it has not been replaced by a
                ;; new version.
                ;; Otherwise, the target-node has probably been replaced by another version
                ;; (reloaded) and that should have reestablished any incoming arcs to it already
                (when (g/node-by-id target-node)
                  (g/connect source-node source-label target-node target-label)))))))

      (du/measuring process-metrics :redirect
        (g/transact transact-opts
          (for [[resource-node new-resource] (:redirect plan)]
            (g/set-property resource-node :resource new-resource))))

      (du/measuring process-metrics :mark-deleted
        (let [basis (g/now)]
          (g/transact transact-opts
            (for [node-id (:mark-deleted plan)]
              (let [flaw (resource-io/file-not-found-error node-id nil :fatal (resource-node/resource basis node-id))]
                (g/mark-defective node-id flaw))))))

      ;; invalidate outputs.
      (du/measuring process-metrics :invalidate-outputs
        (let [basis (g/now)]
          (du/if-metrics
            (doseq [node-id (:invalidate-outputs plan)]
              (du/measuring resource-metrics (resource/proj-path (resource-node/resource basis node-id)) :invalidate-outputs
                (g/invalidate-outputs! (mapv (fn [[_ src-label]]
                                               (g/endpoint node-id src-label))
                                             (g/explicit-outputs basis node-id)))))
            (g/invalidate-outputs! (into []
                                         (mapcat (fn [node-id]
                                                   (map (fn [[_ src-label]]
                                                          (g/endpoint node-id src-label))
                                                        (g/explicit-outputs basis node-id))))
                                         (:invalidate-outputs plan))))))

      ;; restore overridden properties.
      (du/measuring process-metrics :restore-overridden-properties
        (du/if-metrics
          (g/with-auto-evaluation-context evaluation-context
            (doseq [[resource collected-properties] collected-properties-by-resource]
              (when-some [new-node-id (resource->new-node resource)]
                (du/measuring resource-metrics (resource/proj-path resource) :restore-overridden-properties
                  (let [restore-properties-tx-data (g/restore-overridden-properties new-node-id collected-properties evaluation-context)]
                    (when (seq restore-properties-tx-data)
                      (g/transact transact-opts restore-properties-tx-data)))))))
          (let [restore-properties-tx-data
                (g/with-auto-evaluation-context evaluation-context
                  (into []
                        (mapcat (fn [[resource collected-properties]]
                                  (when-some [new-node-id (resource->new-node resource)]
                                    (g/restore-overridden-properties new-node-id collected-properties evaluation-context))))
                        collected-properties-by-resource))]
            (when (seq restore-properties-tx-data)
              (g/transact transact-opts
                restore-properties-tx-data)))))

      (du/measuring process-metrics :update-selection
        (let [old->new (into {}
                             (map (fn [[p n]]
                                    [(old-nodes-by-path p) n]))
                             resource-path->new-node)
              dissoc-deleted (fn [x] (apply dissoc x (:mark-deleted plan)))]
          (g/transact transact-opts
            (concat
              (let [all-selections (-> (g/node-value project :all-selections)
                                       (dissoc-deleted)
                                       (remap-selection old->new (comp vector first)))]
                (perform-selection project all-selections))
              (let [all-sub-selections (-> (g/node-value project :all-sub-selections)
                                           (dissoc-deleted)
                                           (remap-selection old->new (constantly [])))]
                (perform-sub-selection project all-sub-selections))))))

      ;; Invalidating outputs is the only change that does not reset the undo
      ;; history. This is a quick way to find out if we have any significant
      ;; changes, but we must take care to also exclude non-change information
      ;; such as the list of :kept resources from this check.
      (when (some seq (vals (dissoc plan :invalidate-outputs :kept)))
        (g/reset-undo! (graph project)))

      (du/when-metrics
        (reset! resource-change-metrics-atom
                {:old-nodes-by-path old-nodes-by-path
                 :new-nodes-by-path resource-path->new-node
                 :process-metrics @process-metrics
                 :resource-metrics @resource-metrics
                 :transaction-metrics @transaction-metrics})))))

(defn- handle-resource-changes [project changes render-progress!]
  (let [[old-nodes-by-path old-node->old-disk-sha256]
        (g/with-auto-evaluation-context evaluation-context
          (let [workspace (workspace project evaluation-context)
                old-nodes-by-path (g/node-value project :nodes-by-resource-path evaluation-context)
                old-node->old-disk-sha256 (g/node-value workspace :disk-sha256s-by-node-id evaluation-context)]
            [old-nodes-by-path old-node->old-disk-sha256]))

        resource-change-plan
        (du/metrics-time "Generate resource change plan"
          (resource-update/resource-change-plan old-nodes-by-path old-node->old-disk-sha256 changes))]

    ;; For debugging resource loading / reloading issues:
    ;; (resource-update/print-plan resource-change-plan)
    (du/metrics-time "Perform resource change plan" (perform-resource-change-plan resource-change-plan project render-progress!))
    (lsp/apply-resource-changes! (lsp/get-node-lsp project) changes)))

(g/defnk produce-collision-groups-data
  [collision-group-nodes]
  (collision-groups/make-collision-groups-data collision-group-nodes))

(defn parse-filter-param
  [_node-id ^String s]
  (cond
    (.equalsIgnoreCase "nearest" s) gl/nearest
    (.equalsIgnoreCase "linear" s) gl/linear
    (.equalsIgnoreCase "nearest_mipmap_nearest" s) gl/nearest-mipmap-nearest
    (.equalsIgnoreCase "nearest_mipmap_linear" s) gl/nearest-mipmap-linear
    (.equalsIgnoreCase "linear_mipmap_nearest" s) gl/linear-mipmap-nearest
    (.equalsIgnoreCase "linear_mipmap_linear" s) gl/linear-mipmap-linear
    :else (g/error-fatal (format "Invalid value for filter param: '%s'" s))))

(g/defnk produce-default-tex-params
  [_node-id settings]
  (let [min (parse-filter-param _node-id (get settings ["graphics" "default_texture_min_filter"]))
        mag (parse-filter-param _node-id (get settings ["graphics" "default_texture_mag_filter"]))
        errors (filter g/error? [min mag])]
    (if (seq errors)
      (g/error-aggregate errors)
      {:min-filter min
       :mag-filter mag})))

(g/defnode Project
  (inherits core/Scope)

  (property workspace g/Any)

  (property all-selections g/Any)
  (property all-sub-selections g/Any)

  (input script-intelligence g/NodeID :cascade-delete)
  (input editor-extensions g/NodeID :cascade-delete)
  (input all-selected-node-ids g/Any :array)
  (input all-selected-node-properties g/Any :array)
  (input resources g/Any)
  (input resource-map g/Any)
  (input save-data g/Any :array :substitute gu/array-subst-remove-errors)
  (input node-id+resources g/Any :array)
  (input settings g/Any :substitute (constantly (gpc/default-settings)))
  (input display-profiles g/Any)
  (input texture-profiles g/Any)
  (input collision-group-nodes g/Any :array :substitute gu/array-subst-remove-errors)
  (input build-settings g/Any)
  (input breakpoints Breakpoints :array :substitute gu/array-subst-remove-errors)

  (output selected-node-ids-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-selections]
                                                             (let [selected-node-id-set (set all-selected-node-ids)]
                                                               (->> all-selections
                                                                 (map (fn [[key vals]] [key (filterv selected-node-id-set vals)]))
                                                                 (into {})))))
  (output selected-node-properties-by-resource-node g/Any :cached (g/fnk [all-selected-node-properties all-selections]
                                                                    (let [props (->> all-selected-node-properties
                                                                                  (map (fn [p] [(:node-id p) p]))
                                                                                  (into {}))]
                                                                      (->> all-selections
                                                                        (map (fn [[key vals]] [key (vec (keep props vals))]))
                                                                        (into {})))))
  (output sub-selections-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-sub-selections]
                                                               (let [selected-node-id-set (set all-selected-node-ids)]
                                                                 (->> all-sub-selections
                                                                   (map (fn [[key vals]] [key (filterv (comp selected-node-id-set first) vals)]))
                                                                   (into {})))))
  (output resource-map g/Any (gu/passthrough resource-map))
  (output nodes-by-resource-path g/Any :cached (g/fnk [node-id+resources] (make-resource-nodes-by-path-map node-id+resources)))
  (output save-data g/Any :cached (g/fnk [save-data] (filterv :save-value save-data)))
  (output dirty-save-data g/Any :cached (g/fnk [save-data]
                                          (filterv (fn [save-data]
                                                     (and (:dirty save-data)
                                                          (let [resource (:resource save-data)]
                                                            (and (resource/editable? resource)
                                                                 (not (resource/read-only? resource))))))
                                                   save-data)))
  (output settings g/Any :cached (gu/passthrough settings))
  (output display-profiles g/Any :cached (gu/passthrough display-profiles))
  (output texture-profiles g/Any :cached (gu/passthrough texture-profiles))
  (output nil-resource resource/Resource (g/constantly nil))
  (output collision-groups-data g/Any :cached produce-collision-groups-data)
  (output default-tex-params g/Any :cached produce-default-tex-params)
  (output build-settings g/Any (gu/passthrough build-settings))
  (output breakpoints Breakpoints :cached (g/fnk [breakpoints] (into [] cat breakpoints))))

(defn get-project
  ([node]
   (get-project (g/now) node))
  ([basis node]
   (g/graph-value basis (g/node-id->graph-id node) :project-id)))

(defn find-resources [project query]
  (let [resource-path-to-node (g/node-value project :nodes-by-resource-path)
        resources        (resource/filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-path-to-node (resource/proj-path r))]) resources)))

(defn settings [project]
  (g/node-value project :settings))

(defn project-dependencies [project]
  (when-let [settings (settings project)]
    (settings ["project" "dependencies"])))

(defn shared-script-state? [project]
  (some-> (settings project) (get ["script" "shared_state"])))

(defn project-title [project]
  (some-> project
    (settings)
    (get ["project" "title"])))

(defn- disconnect-from-inputs [basis src tgt connections]
  (let [outputs (set (g/output-labels (g/node-type* basis src)))
        inputs (set (g/input-labels (g/node-type* basis tgt)))]
    (for [[src-label tgt-label] connections
          :when (and (outputs src-label) (inputs tgt-label))]
      (g/disconnect src src-label tgt tgt-label))))

(defn resolve-path-or-resource [project path-or-resource evaluation-context]
  (if (string? path-or-resource)
    (workspace/resolve-workspace-resource (workspace project evaluation-context) path-or-resource evaluation-context)
    path-or-resource))

(defn disconnect-resource-node [evaluation-context project path-or-resource consumer-node connections]
  (let [basis (:basis evaluation-context)
        resource (resolve-path-or-resource project path-or-resource evaluation-context)
        node (get-resource-node project resource evaluation-context)]
    (disconnect-from-inputs basis node consumer-node connections)))

(defn- ensure-resource-node-created [tx-data-context-map project resource]
  (assert (satisfies? resource/Resource resource))
  (if-some [[_ pending-resource-node-id] (find (:created-resource-nodes tx-data-context-map) resource)]
    [tx-data-context-map pending-resource-node-id nil]
    (let [graph-id (g/node-id->graph-id project)
          node-type (resource-node-type resource)
          creation-tx-data (g/make-nodes graph-id [resource-node-id [node-type :resource resource]]
                                         (g/connect resource-node-id :_node-id project :nodes)
                                         (g/connect resource-node-id :node-id+resource project :node-id+resources))
          created-resource-node-id (first (g/tx-data-nodes-added creation-tx-data))
          tx-data-context' (assoc-in tx-data-context-map [:created-resource-nodes resource] created-resource-node-id)]
      [tx-data-context' created-resource-node-id creation-tx-data])))

(defn connect-resource-node
  "Creates transaction steps for creating `connections` between the
  corresponding node for `path-or-resource` and `consumer-node`. If
  there is no corresponding node for `path-or-resource`, transactions
  for creating and loading the node will be included. Returns a map with
  following keys:
    :tx-data          transactions steps
    :node-id          node id corresponding to `path-or-resource`
    :created-in-tx    boolean indicating if this node will be created after
                      applying this transaction and does not exist yet in the
                      system, such nodes can't be used for `g/node-value` calls"
  [evaluation-context project path-or-resource consumer-node connections]
  ;; TODO: This is typically run from a property setter, where currently the
  ;; evaluation-context does not contain a cache. This makes resource lookups
  ;; very costly as they need to produce the lookup maps every time.
  ;; In large projects, this has a huge impact on load time. To work around
  ;; this, we use the default, cached evaluation-context to resolve resources.
  ;; This has been reported as DEFEDIT-1411.
  (g/with-auto-evaluation-context default-evaluation-context
    (when-some [resource (resolve-path-or-resource project path-or-resource default-evaluation-context)]
      (let [existing-resource-node-id (get-resource-node project resource default-evaluation-context)
            [node-id creation-tx-data] (if existing-resource-node-id
                                         [existing-resource-node-id nil]
                                         (thread-util/swap-rest! (:tx-data-context evaluation-context) ensure-resource-node-created project resource))
            node-type (resource-node-type resource)

            load-tx-data
            (cond
              ;; If we just created the resource node, that means the referenced
              ;; resource does not exist, or it would have already been created
              ;; during resource-sync.
              (some? creation-tx-data)
              (do
                (when (and *load-cache* (not (contains? @*load-cache* node-id)))
                  (swap! *load-cache* conj node-id))
                (mark-node-file-not-found node-id resource))

              ;; If we're in the process of loading, we can verify that the
              ;; referenced resource node has been loaded before the node that
              ;; refers to it. If it hasn't been loaded yet, this means the
              ;; :dependencies-fn of the consumer-node is not reporting it as a
              ;; dependency. This is a programming error, so we throw an Error
              ;; instead of an Exception.
              (and *load-cache* (not (contains? @*load-cache* node-id)))
              (throw (Error. (format "Node of type %s refers to '%s', but it hasn't been loaded yet. Check its :dependencies-fn."
                                     (g/node-type-kw (:basis evaluation-context) consumer-node)
                                     (resource/proj-path resource)))))]
        {:node-id node-id
         :created-in-tx (nil? existing-resource-node-id)
         :tx-data (vec
                    (flatten
                      (concat
                        creation-tx-data
                        load-tx-data
                        (connect-if-output node-type node-id consumer-node connections))))}))))

(deftype ProjectResourceListener [project-id]
  resource/ResourceListener
  (handle-changes [this changes render-progress!]
    (handle-resource-changes project-id changes render-progress!)))

(defn make-project [graph workspace-id extensions]
  (let [project-id
        (second
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph
                  [script-intelligence si/ScriptIntelligenceNode
                   project [Project :workspace workspace-id]]
                (g/connect extensions :_node-id project :editor-extensions)
                (g/connect script-intelligence :_node-id project :script-intelligence)
                (g/connect workspace-id :build-settings project :build-settings)
                (g/connect workspace-id :resource-list project :resources)
                (g/connect workspace-id :resource-map project :resource-map)
                (g/set-graph-value graph :project-id project)
                (g/set-graph-value graph :lsp (lsp/make project get-resource-node))))))]
    (workspace/add-resource-listener! workspace-id 1 (ProjectResourceListener. project-id))
    project-id))

(defn read-dependencies [game-project-resource]
  (with-open [game-project-reader (io/reader game-project-resource)]
    (-> (settings-core/parse-settings game-project-reader)
        (settings-core/get-setting ["project" "dependencies"])
        (library/parse-library-uris))))

(defn- project-resource-node? [basis node-id]
  (if-some [resource (resource-node/as-resource-original basis node-id)]
    (some? (resource/proj-path resource))
    false))

(defn- project-file-resource-node? [basis node-id]
  (if-some [resource (resource-node/as-resource-original basis node-id)]
    (some? (resource/abs-path resource))
    false))

(defn cache-retain?
  ([endpoint]
   (case (g/endpoint-label endpoint)
     (:build-targets) (project-resource-node? (g/now) (g/endpoint-node-id endpoint))
     (:save-data :save-value) (project-file-resource-node? (g/now) (g/endpoint-node-id endpoint))
     false))
  ([basis endpoint]
   (case (g/endpoint-label endpoint)
     (:build-targets) (project-resource-node? basis (g/endpoint-node-id endpoint))
     (:save-data :save-value) (project-file-resource-node? basis (g/endpoint-node-id endpoint))
     false)))

(defn- cached-build-target-output? [node-id label evaluation-context]
  (case label
    (:build-targets) (project-resource-node? (:basis evaluation-context) node-id)
    false))

(defn- cached-save-data-output? [node-id label evaluation-context]
  (case label
    (:save-data :save-value) (project-file-resource-node? (:basis evaluation-context) node-id)
    false))

(defn- update-system-cache-from-pruned-evaluation-context! [cache-entry-pred evaluation-context]
  ;; To avoid cache churn, we only transfer the most important entries to the system cache.
  (let [pruned-evaluation-context (g/pruned-evaluation-context evaluation-context cache-entry-pred)]
    (g/update-cache-from-evaluation-context! pruned-evaluation-context)))

(defn update-system-cache-build-targets! [evaluation-context]
  (update-system-cache-from-pruned-evaluation-context! cached-build-target-output? evaluation-context))

(defn update-system-cache-save-data! [evaluation-context]
  (update-system-cache-from-pruned-evaluation-context! cached-save-data-output? evaluation-context))

(defn open-project! [graph extensions workspace-id game-project-resource render-progress!]
  (let [dependencies (read-dependencies game-project-resource)
        progress (atom (progress/make "Updating dependencies..." 13 0))]
    (render-progress! @progress)

    ;; Fetch+install libs if we have network, otherwise fallback to disk state
    (if (workspace/dependencies-reachable? dependencies)
      (->> (workspace/fetch-and-validate-libraries workspace-id dependencies (progress/nest-render-progress render-progress! @progress 4))
           (workspace/install-validated-libraries! workspace-id))
      (workspace/set-project-dependencies! workspace-id (library/current-library-state (workspace/project-path workspace-id) dependencies)))

    (render-progress! (swap! progress progress/advance 4 "Syncing resources..."))
    (workspace/resource-sync! workspace-id [] (progress/nest-render-progress render-progress! @progress))
    (render-progress! (swap! progress progress/advance 1 "Loading project..."))
    (let [project (make-project graph workspace-id extensions)
          populated-project (load-project project (g/node-value project :resources) (progress/nest-render-progress render-progress! @progress 8))]
      ;; Prime the auto completion cache
      (g/node-value (script-intelligence project) :lua-completions)
      populated-project)))

(defn resource-setter [evaluation-context self old-value new-value & connections]
  (let [project (get-project (:basis evaluation-context) self)]
    (concat
      (when old-value (disconnect-resource-node evaluation-context project old-value self connections))
      (when new-value (:tx-data (connect-resource-node evaluation-context project new-value self connections))))))
