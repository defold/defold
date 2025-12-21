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

(ns editor.defold-project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.preprocessors :as code.preprocessors]
            [editor.code.resource :as code.resource]
            [editor.code.script-annotations :as script-annotations]
            [editor.code.script-intelligence :as si]
            [editor.code.transpilers :as code.transpilers]
            [editor.collision-groups :as collision-groups]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.editor-localization-bundle :as editor-localization-bundle]
            [editor.game-project-core :as gpc]
            [editor.gl :as gl]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.library :as library]
            [editor.localization :as localization]
            [editor.lsp :as lsp]
            [editor.notifications :as notifications]
            [editor.placeholder-resource :as placeholder-resource]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.resource-update :as resource-update]
            [editor.settings-core :as settings-core]
            [editor.system :as system]
            [editor.texture.engine :as texture.engine]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.java :as java]
            [internal.util :as util]
            [internal.util :as iutil]
            [schema.core :as s]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.debug-util :as du]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.thread-util :as thread-util])
  (:import [java.io File]
           [java.util.concurrent.atomic AtomicLong]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defonce load-metrics-atom (du/when-metrics (atom nil)))
(defonce resource-change-metrics-atom (du/when-metrics (atom nil)))

(def ^:private TBreakpoint
  {:resource s/Any
   :row Long
   :enabled Boolean
   (s/optional-key :condition) String})

(g/deftype Breakpoints [TBreakpoint])

(defn graph [project]
  (g/node-id->graph-id project))

(defn code-transpilers
  ([project]
   (code-transpilers (g/now) project))
  ([basis project]
   (g/graph-value basis (g/node-id->graph-id project) :code-transpilers)))

(defn- resource-type->node-type [resource-type]
  (or (:node-type resource-type)
      placeholder-resource/PlaceholderResourceNode))

(def resource-node-type (comp resource-type->node-type resource/resource-type))

(defn- load-resource-node [project resource-node-id resource source-value transpiler-tx-data-fn]
  (try
    ;; TODO(save-value-cleanup): This shouldn't be able to happen anymore. Remove this check after some time in the wild.
    (assert (and (not= :folder (resource/source-type resource))
                 (resource/exists? resource)))
    (let [{:keys [read-fn load-fn] :as resource-type} (resource/resource-type resource)
          transpiler-tx-data (transpiler-tx-data-fn resource-node-id resource)]
      (cond-> []

              load-fn
              (into (flatten
                      (if (nil? read-fn)
                        (load-fn project resource-node-id resource)
                        (load-fn project resource-node-id resource source-value))))

              (and (:auto-connect-save-data? resource-type)
                   (resource/save-tracked? resource))
              (into (g/connect resource-node-id :save-data project :save-data))

              transpiler-tx-data
              (into transpiler-tx-data)

              :always
              not-empty))
    (catch Exception exception
      (log/warn :msg (format "Unable to load resource '%s'" (resource/proj-path resource)) :exception exception)
      (let [node-type (resource-node-type resource)
            invalid-content-error (resource-io/invalid-content-error resource-node-id nil :fatal resource exception)]
        (g/mark-defective resource-node-id node-type invalid-content-error)))
    (catch Throwable throwable
      (let [node-type (resource-node-type resource)
            proj-path (resource/proj-path resource)]
        (throw (ex-info (format "Error when loading resource '%s'" proj-path)
                        {:node-type node-type
                         :proj-path proj-path}
                        throwable))))))

(defn load-embedded-resource-node [project embedded-resource-node-id embedded-resource source-value]
  (let [embedded-resource-type (resource/resource-type embedded-resource)
        load-fn (:load-fn embedded-resource-type)]
    (load-fn project embedded-resource-node-id embedded-resource source-value)))

(defn- make-file-not-found-node-load-info [node-id resource]
  {:node-id node-id
   :resource resource
   :read-error (resource-io/file-not-found-error node-id nil :fatal resource)})

(defn read-node-load-info [node-id resource resource-metrics]
  {:pre [(g/node-id? node-id)]}
  (let [{:keys [lazy-loaded read-fn] :as resource-type} (resource/resource-type resource)

        read-result
        (when (and read-fn
                   (not lazy-loaded))
          (try
            ;; TODO(save-value-cleanup): This shouldn't be able to happen anymore. Remove this check after some time in the wild.
            (assert (and (not= :folder (resource/source-type resource))
                         (resource/exists? resource)))
            (du/measuring resource-metrics (resource/proj-path resource) :read-source-value
              (resource/read-source-value+sha256-hex resource read-fn))
            (catch Exception exception
              (log/warn :msg (format "Unable to read resource '%s'" (resource/proj-path resource)) :exception exception)
              (resource-io/invalid-content-error node-id nil :fatal resource exception))
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

(defn- sort-node-ids-for-loading-impl
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
               [dep-queue dep-queued] (sort-node-ids-for-loading-impl deps (conj in-progress node-id) queue queued batch node-id->dependency-node-ids)]
           (recur (rest node-ids)
                  in-progress
                  (if (contains? batch node-id) (conj dep-queue node-id) dep-queue)
                  (conj dep-queued node-id)
                  batch
                  node-id->dependency-node-ids)))))))

(defn- sort-node-ids-for-loading [node-ids node-id->dependency-node-ids]
  (first (sort-node-ids-for-loading-impl node-ids #{} [] #{} (set node-ids) node-id->dependency-node-ids)))

(defn ^{:dynamic (system/defold-dev?)} node-load-info-tx-data [{:keys [node-id read-error resource] :as node-load-info} project transpiler-tx-data-fn]
  ;; At this point, the node-id refers to a created node in the graph.
  (e/cons
    (g/set-property node-id :loaded true)
    (if read-error
      (let [node-type (resource-node-type resource)]
        (g/mark-defective node-id node-type read-error))
      (let [source-value (:source-value node-load-info)]
        (load-resource-node project node-id resource source-value transpiler-tx-data-fn)))))

(defn- sort-node-load-infos-for-loading
  "Sorts the node-load-infos so that referenced nodes are loaded before the
  referencing nodes. If the node-load-infos depends on proj-paths that are not
  among the supplied node-load-infos, you must provide information about the old
  nodes. This is because a loaded node can refer to an old node that in turn
  refers to another loaded node that should be loaded first.

  Args:
    node-load-infos
      The node-load-infos that are about to be loaded into the graph. Typically
      obtained from the read-node-load-infos function.

    old-node-ids-by-proj-path
      A map of proj-paths to the resource node-id that corresponded to that
      proj-path before the resource-sync. May contain entries for nodes that
      have been deleted earlier in the resource-sync.

    old-node-id->dependency-proj-paths
      A function from an old resource node-id in the old-node-ids-by-proj-path
      map to any proj-paths it remains dependent on after the resource-sync. If
      the node-id provided to it is unknown, or was deleted at earlier stage of
      the resource-sync, it should return an empty sequence."
  [node-load-infos old-node-ids-by-proj-path old-node-id->dependency-proj-paths]
  {:pre [(map? old-node-ids-by-proj-path)
         (ifn? old-node-id->dependency-proj-paths)]}
  (let [[node-load-infos-by-node-id
         node-ids-by-proj-path]
        (iutil/into-multiple
          [{}
           old-node-ids-by-proj-path]
          [(map (coll/pair-fn :node-id))
           (map (fn [{:keys [node-id resource]}]
                  (let [proj-path (resource/proj-path resource)]
                    (pair proj-path node-id))))]
          node-load-infos)

        new-node-id->dependency-proj-paths
        (fn new-node-id->dependency-proj-paths [node-id]
          ;; We want to return nil if the node-id is not known to us, and an
          ;; empty vector otherwise.
          (when-let [node-load-info (node-load-infos-by-node-id node-id)]
            (or (:dependency-proj-paths node-load-info) [])))

        node-id->dependency-node-ids
        (fn/memoize
          (fn node-id->dependency-node-ids [node-id]
            ;; The returned dependency-node-ids will contain the new-node-id
            ;; when the dependency-proj-path matches both an old and a new node.
            (into []
                  (keep node-ids-by-proj-path)
                  (or (new-node-id->dependency-proj-paths node-id)
                      (old-node-id->dependency-proj-paths node-id)))))

        node-id-load-order
        (sort-node-ids-for-loading (keys node-load-infos-by-node-id) node-id->dependency-node-ids)]

    (into []
          (keep node-load-infos-by-node-id)
          node-id-load-order)))

(defn- get-transpiler-tx-data-fn! [project evaluation-context]
  (g/tx-cached-value! evaluation-context [:transpiler-tx-data-fn]
    (let [basis (:basis evaluation-context)
          code-transpilers (code-transpilers basis project)]
      (code.transpilers/make-resource-load-tx-data-fn code-transpilers evaluation-context))))

(defn load-nodes-tx-data
  [project node-load-infos render-generate-tx-data-progress! render-apply-tx-data-progress! resource-metrics]
  {:pre [(ifn? render-generate-tx-data-progress!)
         (ifn? render-apply-tx-data-progress!)]}
  (let [node-count (count node-load-infos)

        resource-metrics-load-timer
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
              (du/update-metrics resource-metrics resource-path :process-load-tx-data (- end-time start-time)))))

        transpiler-tx-data-fn
        (g/with-auto-evaluation-context evaluation-context
          (get-transpiler-tx-data-fn! project evaluation-context))

        node-load-info-tx-data-fn
        (if (identical? progress/null-render-progress! render-generate-tx-data-progress!)
          (fn node-load-info-tx-data-fn [node-load-info _progress]
            (node-load-info-tx-data node-load-info project transpiler-tx-data-fn))
          (fn node-load-info-tx-data-fn [node-load-info progress]
            (render-generate-tx-data-progress! (update progress :message localization/set-message-key "progress.processing-resource"))
            (node-load-info-tx-data node-load-info project transpiler-tx-data-fn)))]

    (coll/transfer node-load-infos :eduction
      (coll/mapcat-indexed
        (fn [^long node-index node-load-info]
          (let [resource (:resource node-load-info)
                proj-path (resource/proj-path resource)
                progress-message (localization/message "progress.loading-resource" {"resource" proj-path})
                progress (progress/make progress-message node-count (inc node-index))]
            (e/concat
              (g/callback render-apply-tx-data-progress! progress)
              (du/when-metrics
                (g/callback start-resource-metrics-load-timer!))
              (du/measuring resource-metrics proj-path :generate-load-tx-data
                (node-load-info-tx-data-fn node-load-info progress))
              (du/when-metrics
                (g/callback stop-resource-metrics-load-timer! proj-path)))))))))

(defn read-node-load-infos [node-id+resource-pairs ^long progress-size render-progress! resource-metrics]
  {:pre [(or (nil? node-id+resource-pairs) (counted? node-id+resource-pairs))]}
  (let [progress-fn
        (if (pos? progress-size)
          (fn progress-fn [progress-message ^long node-index]
            (progress/make progress-message progress-size (inc node-index)))
          (fn indeterminate-progress-fn [progress-message ^long _node-index]
            (progress/make-indeterminate progress-message)))]
    (->> node-id+resource-pairs
         (map-indexed vector)
         (pmap (fn [[node-index [node-id resource]]]
                 (let [proj-path (resource/proj-path resource)
                       progress-message (localization/message "progress.reading-resource" {"resource" proj-path})
                       progress (progress-fn progress-message node-index)]
                   (render-progress! progress)
                   (read-node-load-info node-id resource resource-metrics))))
         (into []))))

(defn node-load-infos->stored-disk-state [node-load-infos]
  (let [[disk-sha256s-by-node-id
         node-id+source-value-pairs]
        (util/into-multiple
          [{} []]
          [(keep (fn [{:keys [resource] :as node-load-info}]
                   (when (and (resource/file-resource? resource)
                              (resource/stateful? resource))
                     (let [{:keys [disk-sha256 node-id]} node-load-info]
                       (pair node-id disk-sha256)))))
           (keep (fn [{:keys [node-id read-error resource source-value] :as _node-load-info}]
                   ;; The source-value output will never be evaluated for
                   ;; non-editable or stateless resources, so there is no need
                   ;; to store their entries.
                   (when (and (some? source-value)
                              (resource/editable? resource)
                              (resource/stateful? resource))
                     (pair node-id
                           (or read-error
                               (let [resource-type (resource/resource-type resource)]
                                 ;; Note: Here, source-value is whatever was
                                 ;; returned by the read-fn, so it's technically
                                 ;; a save-value.
                                 (resource-node/save-value->source-value source-value resource-type)))))))]
          node-load-infos)]
    {:disk-sha256s-by-node-id disk-sha256s-by-node-id
     :node-id+source-value-pairs node-id+source-value-pairs}))

(defn log-cache-info! [cache message]
  ;; Disabled during tests to minimize log spam.
  (when-not (Boolean/getBoolean "defold.tests")
    (let [{:keys [total retained unretained limit]} (g/cache-info cache)]
      (log/info :message message
                :total total
                :retained retained
                :unretained unretained
                :limit limit))))

(defn cache-loaded-save-data! [node-load-infos project excluded-resource-node-id?]
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

(defn ^:dynamic report-defunload-issues! [workspace unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths]
  (let [[^File report-file we-created-report-file] (resource/defunload-issues-file)
        report-file-path (.getPath report-file)
        is-appending (not we-created-report-file)
        localization (g/with-auto-evaluation-context evaluation-context
                       (workspace/localization workspace evaluation-context))

        unsafe-references-report-lines
        (coll/transfer (sort-by key unsafe-dependency-proj-paths-by-referencing-proj-path) []
          (map (fn [[referencing-proj-path unsafe-dependency-proj-paths]]
                 (e/cons
                   (localization
                     (localization/message "dialog.defunload-issues.report-line"
                                           {"resource" referencing-proj-path}))
                   (e/map dialogs/indent-with-bullet
                          (sort unsafe-dependency-proj-paths)))))
          (interpose [""])
          cat)]

    ;; Append report to defunload issues file.
    (with-open [writer (io/writer report-file :append is-appending)]
      (binding [*out* writer
                *flush-on-newline* false]
        (when is-appending
          (newline))
        (println (localization (localization/message "dialog.defunload-issues.report.section-loading-resources")))
        (when-not (coll/empty? unsafe-dependency-proj-paths-by-referencing-proj-path)
          (newline)
          (doseq [line unsafe-references-report-lines]
            (println (string/replace line dialogs/indented-bullet "  * "))))
        (when-not (coll/empty? loaded-undesired-proj-paths)
          (newline)
          (println (localization (localization/message "dialog.defunload-issues.report.loaded-undesired")))
          (doseq [proj-path (sort loaded-undesired-proj-paths)]
            (println (str "  * " proj-path))))))

    ;; Also log and show a warning notification if any of the loaded resources
    ;; contain unsafe references to defunloaded resources.
    (when-not (coll/empty? unsafe-references-report-lines)
      (let [summary-line (localization (localization/message "dialog.defunload-issues.summary"))
            advice-line (localization (localization/message "dialog.defunload-issues.advice"))
            header-message (localization/message "dialog.defunload-issues.header")
            preamble-lines [summary-line
                            advice-line
                            ""
                            (localization (localization/message "dialog.defunload-issues.report-path"))
                            report-file-path]]

        (log/warn :message summary-line
                  :detail advice-line
                  :report-file-path report-file-path)
        (ui/run-later
          (let [show-details-dialog!
                (fn show-details-dialog! []
                  (let [dialog-lines
                        (e/concat
                          preamble-lines
                          [""]
                          unsafe-references-report-lines)

                        dialog-message
                        (string/join "\n" dialog-lines)

                        dialog-result
                        (dialogs/make-confirmation-dialog
                          localization
                          {:title (localization/message "dialog.defunload-issues.title")
                           :size :large
                           :icon :icon/triangle-warning
                           :header header-message
                           :content dialog-message
                           :buttons [{:text (localization/message "dialog.defunload-issues.button.ignore")
                                      :result false}
                                     {:text (localization/message "dialog.defunload-issues.button.open-report")
                                      :result true}]})]

                    (when dialog-result
                      (ui/open-file report-file))))]

            (notifications/show!
              (workspace/notifications workspace)
              {:id ::defunload-issues
               :type :warning
               :message header-message
               :actions [{:message (localization/message "notification.defunload-issues.action.show-details")
                          :on-action show-details-dialog!}]})))))))

(defn- node-id+resource-pair->proj-path
  ^String [node-id+resource-pair]
  (let [resource (val node-id+resource-pair)]
    (resource/proj-path resource)))

(defn read-nodes
  "Reads and returns node-load-infos for a sequence of node-id+resource-pairs.
  If a .defunload file exists in the project, any resources that match one of
  the patterns therein will be skipped, and excluded from the list of returned
  node-load-infos. However, we may be forced to read and include them in the
  list if they are unsafely referenced from a loaded resource. Resource types
  can opt in to :allow-unloaded-use to declare themselves safe to reference in
  an unloaded state. Any other references are deemed unsafe and will be loaded,
  along with its recursive dependencies, regardless of if they match a
  defunload pattern or not. Note that these forcibly loaded defunloaded nodes
  may not fully participate in all systems that a regularly loaded node would.

  Args:
    new-node-id+resource-pairs
      A sequence of pairs of [node-id, resource] to return node-load-infos for.
      The node-ids do not need to exist in the graph, but they must be unique
      and not already taken by an existing node.

  Kv-args:
    :render-progress!
      Optional. A function that will be called to report progress as resources
      are read from disk.

    :resource-metrics
      Optional. A metrics-collector (see debug-util/make-metrics-collector) that
      will be updated with timings as resources are read from disk.

    :old-node-ids-by-proj-path
      Required for subsequent calls after the project has been loaded already.
      A map of proj-paths to the resource node-id that corresponded to that
      proj-path before the resource-sync. Used to look up information about the
      previous state of nodes that we may be about to replace with new nodes as
      part of the resource-sync. Should also contain entries for nodes that have
      been deleted earlier in the resource-sync.

    :old-node-id->old-node-state
      Required for subsequent calls after the project has been loaded already.
      A function from an old resource node-id in the old-node-ids-by-proj-path
      map to a keyword reflecting its state prior to the resource-sync. Will be
      used in the event that a .defunload file exists in the project and one of
      the patterns therein matches the proj-path of the read resource.

      Acceptable return values:
        :loaded
          The old node for the defunloaded resource was loaded before the
          resource-sync was triggered. We may have been forced to load it if a
          loaded resource references the defunloaded resource and its
          resource-type does not :allow-unloaded-use. If this ever happens, we
          will keep the node loaded in the graph, and must respond to external
          changes to it, and so on.

        :not-loaded
          The old node for the defunloaded resource had not ever been loaded
          before the resource-sync was triggered.

    :old-node-id->dependency-proj-paths
      Required for subsequent calls after the project has been loaded already.
      A function from an old resource node-id in the old-node-ids-by-proj-path
      map to any proj-paths it remains dependent on after the resource-sync. If
      the node-id provided to it is unknown, or was deleted at earlier stage of
      the resource-sync, it should return an empty sequence."
  [new-node-id+resource-pairs
   & {:keys [old-node-id->dependency-proj-paths
             old-node-id->old-node-state
             old-node-ids-by-proj-path
             render-progress!
             resource-metrics]
      :or {old-node-id->dependency-proj-paths fn/constantly-nil
           old-node-ids-by-proj-path {}
           render-progress! progress/null-render-progress!}
      :as read-nodes-opts}]
  {:pre [(every? #{:old-node-id->dependency-proj-paths
                   :old-node-id->old-node-state
                   :old-node-ids-by-proj-path
                   :render-progress!
                   :resource-metrics}
                 (keys read-nodes-opts))]}
  (let [basis (g/now)

        workspace
        (some-> new-node-id+resource-pairs
                first
                second
                resource/workspace)

        unloaded-proj-path?
        (if workspace
          (g/raw-property-value basis workspace :unloaded-proj-path?) ; Returns fn/constantly-false if there is no .defunload file in the project.
          fn/constantly-false)

        node-load-infos
        (if (identical? fn/constantly-false unloaded-proj-path?)
          ;; Resources cannot be flagged as unloaded. We can safely proceed to
          ;; load all the new resources without considering what has been loaded
          ;; before.
          (read-node-load-infos new-node-id+resource-pairs (count new-node-id+resource-pairs) render-progress! resource-metrics)

          ;; Resources may be unloaded. Check that we don't have unsafe
          ;; references to any unloaded resources from the loaded resources, as
          ;; these may cause issues for the editor. Create a report of which
          ;; unloaded resources were referenced by loaded resources, and show a
          ;; warning dialog to the user. Then, proceed to load the unsafely
          ;; referenced resources even though they match some defunload pattern.
          ;; This process is recursive, as these files can unsafely reference
          ;; new resources that will need to be loaded, and so on.
          ;;
          ;; Terms used:
          ;;
          ;; "safe" refers to referenced dependencies. A resource can be safely
          ;; referenced as long as it does not match a defunload pattern, or its
          ;; resource type explicitly allows unloaded use.
          ;;
          ;; "unsafe" refers to any dependency that does not match the above
          ;; criteria for safe.
          ;;
          ;; "desired" means anything that was in the original collection
          ;; of new-node-id+resource-pairs fed to the read-nodes function,
          ;; but excluding defunloaded entries.
          ;;
          ;; "principal" means something originated from the collection of
          ;; new-node-id+resource-pairs fed to the read-nodes function. It
          ;; may contain defunloaded entries if the resources were
          ;; previously loaded as supplemental before this call to the
          ;; read-nodes function for some reason.
          ;;
          ;; "supplemental" means something that was not in the collection
          ;; of new-node-id+resource-pairs fed to the read-nodes function.
          ;; Typically, these are resources that were deemed necessary to
          ;; load because they were unsafely referenced from one of the
          ;; principal resources or recursively from their dependencies.
          ;;
          ;; "undesired" means any resource that was loaded as either
          ;; principal or supplemental that was not in the desired set.
          (let [proj-path->resource-type
                (let [editable-proj-path?
                      (g/raw-property-value basis workspace :editable-proj-path?)

                      editable->type-ext->resource-type
                      (into {}
                            (map (fn [editable]
                                   (pair editable
                                         (resource/resource-types-by-type-ext basis workspace editable))))
                            (pair true false))]

                  (fn proj-path->resource-type [proj-path]
                    (let [editable (editable-proj-path? proj-path)
                          type-ext (resource/filename->type-ext proj-path)
                          type-ext->resource-type (editable->type-ext->resource-type editable)]
                      (or (type-ext->resource-type type-ext)
                          (type-ext->resource-type resource/placeholder-resource-type-ext)))))

                safe-dependency-proj-path?
                (fn safe-dependency-proj-path? [proj-path]
                  ;; If the proj-path does not match a defunload pattern, it
                  ;; is always safe to reference as a dependency. If it does
                  ;; match a defunload pattern, it might still be safe to
                  ;; reference if its resource type allows unloaded use.
                  (if (unloaded-proj-path? proj-path)
                    (let [resource-type (proj-path->resource-type proj-path)]
                      (:allow-unloaded-use resource-type false))
                    true))

                ;; Note: new-node-id+resource-pairs is sorted by proj-path, so
                ;; principal-node-id+resource-pairs will be as well.
                [new-node-id+resource-pairs-by-proj-path
                 principal-node-id+resource-pairs]
                (->> new-node-id+resource-pairs
                     (e/map (coll/pair-fn node-id+resource-pair->proj-path))
                     (iutil/into-multiple
                       [{} []]
                       [identity
                        (keep (fn [[proj-path new-node-id+resource-pair]]
                                (if-not (unloaded-proj-path? proj-path)
                                  ;; The path is not flagged as unloaded.
                                  ;; We should load it.
                                  new-node-id+resource-pair

                                  ;; The path is flagged as unloaded.
                                  ;; However, we may still want to load it if
                                  ;; we're replacing an old node that was
                                  ;; previously loaded for some reason.
                                  (let [old-node-id (old-node-ids-by-proj-path proj-path)]
                                    (cond
                                      (nil? old-node-id)
                                      nil ; The path is flagged as unloaded, and we're not replacing an old node. We should not load it.

                                      (nil? old-node-id->old-node-state)
                                      (throw (IllegalArgumentException. "Required old-node-id->old-node-state function was not provided."))

                                      :else
                                      (case (old-node-id->old-node-state old-node-id)
                                        :loaded ; The path is flagged as unloaded, but we're replacing an old node which was loaded before. We should also load its replacement.
                                        new-node-id+resource-pair

                                        :not-loaded ; The path is flagged as unloaded, and we're replacing an old node which wasn't loaded before. No need to load its replacement either.
                                        nil))))))]))

                principal-node-load-infos
                (read-node-load-infos principal-node-id+resource-pairs (count principal-node-id+resource-pairs) render-progress! resource-metrics)

                principal-proj-paths
                (coll/transfer principal-node-id+resource-pairs #{}
                  (map node-id+resource-pair->proj-path))

                principal-dependency-proj-paths
                (coll/transfer principal-node-load-infos #{}
                  (mapcat :dependency-proj-paths))

                [loaded-proj-paths loaded-node-load-infos]
                (loop [loaded-proj-paths principal-proj-paths
                       loaded-node-load-infos principal-node-load-infos
                       required-dependency-proj-paths principal-dependency-proj-paths]
                  (let [supplemental-node-id+resource-pairs
                        (sort
                          (coll/transfer required-dependency-proj-paths []
                            (remove loaded-proj-paths)
                            (remove safe-dependency-proj-path?)
                            (keep (fn [required-dependency-proj-path]
                                    ;; The required proj-path is flagged as
                                    ;; unloaded, but is unsafe to reference. We
                                    ;; must load it now, unless it was already
                                    ;; loaded.
                                    (or (new-node-id+resource-pairs-by-proj-path required-dependency-proj-path) ; If it was defunloaded, it might not have made it into the principal set, but now that it is unsafely referenced we do need it.
                                        (let [old-node-id (old-node-ids-by-proj-path required-dependency-proj-path)]
                                          (cond
                                            (nil? old-node-id)
                                            nil ; The proj-path refers to a resource that does not exist. We should exclude it here, and it will eventually become a defective node in the graph as we process the :load-fn of the referencing resource.

                                            (nil? old-node-id->old-node-state)
                                            (throw (IllegalArgumentException. "Required old-node-id->old-node-state function was not provided."))

                                            :else
                                            (case (old-node-id->old-node-state old-node-id)
                                              :loaded ; The referenced node was loaded before the call to read-nodes, and we're not replacing it with a new node. No need to do anything.
                                              nil

                                              :not-loaded ; The referenced node has not yet been loaded. We should load it now.
                                              (let [resource (resource-node/resource basis old-node-id)]
                                                (pair old-node-id resource))))))))))]

                    (if (coll/empty? supplemental-node-id+resource-pairs)
                      [loaded-proj-paths loaded-node-load-infos]
                      (let [supplemental-node-load-infos
                            (read-node-load-infos supplemental-node-id+resource-pairs 0 render-progress! resource-metrics)

                            loaded-proj-paths
                            (into loaded-proj-paths
                                  (map node-id+resource-pair->proj-path)
                                  supplemental-node-id+resource-pairs)

                            loaded-node-load-infos
                            (e/concat loaded-node-load-infos supplemental-node-load-infos)

                            required-dependency-proj-paths
                            (coll/transfer supplemental-node-load-infos #{}
                              (mapcat :dependency-proj-paths))]

                        (recur loaded-proj-paths
                               loaded-node-load-infos
                               required-dependency-proj-paths)))))]

            ;; Write a report of any unsafe references to unloaded resources.
            (let [desired-proj-paths
                  (coll/transfer new-node-id+resource-pairs-by-proj-path #{}
                    (map key)
                    (remove unloaded-proj-path?))

                  loaded-undesired-proj-paths
                  (set/difference loaded-proj-paths desired-proj-paths)

                  desired-node-load-infos-by-proj-path
                  (coll/transfer loaded-node-load-infos {}
                    (keep (fn [{:keys [resource] :as loaded-node-load-info}]
                            (let [proj-path (resource/proj-path resource)]
                              (when (contains? desired-proj-paths proj-path)
                                (pair proj-path loaded-node-load-info))))))

                  unsafe-dependency-proj-paths-by-referencing-proj-path
                  (coll/transfer desired-proj-paths {}
                    (keep (fn [referencing-proj-path]
                            (let [node-load-info
                                  (desired-node-load-infos-by-proj-path referencing-proj-path)

                                  unsafe-dependency-proj-paths
                                  (coll/transfer (:dependency-proj-paths node-load-info) #{}
                                    (remove safe-dependency-proj-path?))]

                              (when-not (coll/empty? unsafe-dependency-proj-paths)
                                (pair referencing-proj-path
                                      unsafe-dependency-proj-paths))))))]

              (when-not (and (coll/empty? loaded-undesired-proj-paths)
                             (coll/empty? unsafe-dependency-proj-paths-by-referencing-proj-path))
                (report-defunload-issues! workspace unsafe-dependency-proj-paths-by-referencing-proj-path loaded-undesired-proj-paths)))

            loaded-node-load-infos))]

    (sort-node-load-infos-for-loading node-load-infos old-node-ids-by-proj-path old-node-id->dependency-proj-paths)))

(declare workspace)

(defn load-nodes! [project prelude-tx-data node-load-infos render-progress! resource-metrics transact-opts]
  (let [{:keys [disk-sha256s-by-node-id node-id+source-value-pairs]} (node-load-infos->stored-disk-state node-load-infos)
        workspace (workspace project)]
    (resource-node/merge-source-values! node-id+source-value-pairs)
    (let [{:keys [basis] :as tx-result}
          (g/transact transact-opts
            (e/concat
              prelude-tx-data
              (workspace/merge-disk-sha256s workspace disk-sha256s-by-node-id)
              (load-nodes-tx-data project node-load-infos progress/null-render-progress! render-progress! resource-metrics)
              (g/callback render-progress! (progress/make-indeterminate (localization/message "progress.finalizing")))))

          migrated-resource-node-ids
          (into #{}
                (keep #(resource-node/owner-resource-node-id basis %))
                (g/migrated-node-ids tx-result))

          migrated-proj-paths
          (into (sorted-set)
                (map #(resource/proj-path (resource-node/resource basis %)))
                migrated-resource-node-ids)]

      ;; Log any migrated proj-paths.
      ;; Disabled during tests to minimize log spam.
      (when (and (pos? (count migrated-proj-paths))
                 (not (Boolean/getBoolean "defold.tests")))
        (log/info :message "Some files were migrated and will be saved in an updated format." :migrated-proj-paths migrated-proj-paths))

      migrated-resource-node-ids)))

(defn get-resource-node
  ([project path-or-resource]
   (g/with-auto-evaluation-context evaluation-context
     (get-resource-node project path-or-resource evaluation-context)))
  ([project path-or-resource evaluation-context]
   (when-let [resource (cond
                         (string? path-or-resource) (workspace/find-resource (g/node-value project :workspace evaluation-context) path-or-resource evaluation-context)
                         (resource/resource? path-or-resource) path-or-resource
                         :else (assert false (str (type path-or-resource) " is neither a path nor a resource: " (pr-str path-or-resource))))]
     ;; This is frequently called from property setters, where we don't have a
     ;; cache. In that case, manually cache the evaluated value in the
     ;; :tx-data-context atom of the evaluation-context, since this persists
     ;; throughout the transaction.
     (let [nodes-by-resource-path (g/tx-cached-node-value! project :nodes-by-resource-path evaluation-context)]
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

(defn script-annotations [project evaluation-context]
  (g/node-value project :script-annotations evaluation-context))

(defn editor-localization-bundle [project evaluation-context]
  (g/node-value project :editor-localization-bundle evaluation-context))

(defn make-node-id+resource-pairs [^long graph-id resources]
  ;; Note: We sort the resources by extension and proj-path to achieve a
  ;; deterministic order for the assigned node-ids.
  (let [resources (->> resources
                       (remove resource/folder?)
                       (sort-by (juxt resource/type-ext
                                      resource/proj-path)))
        node-ids (g/take-node-ids graph-id (count resources))]
    (mapv pair
          node-ids
          resources)))

(defn make-resource-node-tx-data [project node-type node-id resource]
  {:pre [(g/node-id? project)
         (g/node-id? node-id)
         (resource/resource? resource)
         (not (resource/folder? resource))]}
  (e/concat
    (g/add-node
      (g/construct node-type
        :_node-id node-id
        :resource resource))
    (g/connect node-id :_node-id project :nodes)
    (g/connect node-id :node-id+resource project :node-id+resources)))

(defn make-resource-nodes-tx-data [project node-id+resource-pairs]
  {:pre [(g/node-id? project)]}
  ;; Note: The node-id+resource-pairs come pre-sorted by extension and proj-path
  ;; from the node-id+resource-pairs function. We now further sort by node-type
  ;; since there isn't a one-to-one relationship between the extension and the
  ;; node-type. The idea is to achieve a deterministic load order for the nodes.
  (->> node-id+resource-pairs
       (iutil/group-into
         (sorted-map-by (comparator :k)) []
         (fn key-fn [[_node-id resource]]
           (resource-node-type resource)))
       (e/mapcat
         (fn [[node-type node-id+resource-pairs]]
           (->> node-id+resource-pairs
                (e/mapcat
                  (fn [[node-id resource]]
                    (make-resource-node-tx-data project node-type node-id resource))))))))

(defn setup-game-project-tx-data [project game-project]
  (when (some? game-project)
    (assert (g/node-id? game-project))
    (let [script-intelligence (script-intelligence project)]
      (e/concat
        (g/connect script-intelligence :build-errors game-project :build-errors)
        (g/connect game-project :display-profiles-data project :display-profiles)
        (g/connect game-project :texture-profiles-data project :texture-profiles)
        (g/connect game-project :settings-map project :settings)))))

(defn load-project!
  ([project]
   (load-project! project progress/null-render-progress!))
  ([project render-progress!]
   (load-project! project render-progress! (g/node-value project :resources)))
  ([project render-progress! resources]
   (assert (empty? (g/node-value project :nodes)) "load-project should only be used when loading an empty project")
   ;; Create nodes for all resources in the workspace.
   (let [process-metrics (du/make-metrics-collector)
         resource-metrics (du/make-metrics-collector)
         transaction-metrics (du/make-metrics-collector)
         project-graph (g/node-id->graph-id project)
         node-id+resource-pairs (make-node-id+resource-pairs project-graph resources)

         game-project-resource
         (g/with-auto-evaluation-context evaluation-context
           (-> project
               (workspace evaluation-context)
               (workspace/find-resource "/game.project" evaluation-context)))

         game-project-node-id
         (when game-project-resource
           (coll/some
             (fn [[node-id resource]]
               (when (identical? game-project-resource resource)
                 node-id))
             node-id+resource-pairs))

         read-progress-span 1
         load-progress-span 3
         total-progress-span (+ read-progress-span load-progress-span)
         total-progress (progress/make localization/empty-message total-progress-span 0)

         node-load-infos
         (let [render-progress! (progress/nest-render-progress render-progress! total-progress read-progress-span)]
           (du/measuring process-metrics :read-new-nodes
             (read-nodes node-id+resource-pairs
               :render-progress! render-progress!)))

         total-progress (progress/advance total-progress read-progress-span)

         ;; We can disable change tracking on the initial load since we have
         ;; nothing in the cache and will reset the undo history afterward.
         change-tracked-transact false

         transact-opts {:metrics transaction-metrics
                        :track-changes change-tracked-transact}

         prelude-tx-data
         (e/concat
           (make-resource-nodes-tx-data project node-id+resource-pairs)

           ;; Make sure the game.project node is property connected before
           ;; loading the resource nodes, since establishing these connections
           ;; will invalidate any dependent outputs in the cache.

           ;; TODO(save-value-cleanup): There are implicit dependencies between
           ;; texture profiles and image resources. We probably want to ensure
           ;; the texture profiles are loaded before anything that makes
           ;; implicit use of them to avoid potentially costly cache
           ;; invalidation.
           (setup-game-project-tx-data project game-project-node-id))

         ;; Load the resource nodes. Referenced nodes will be loaded prior to
         ;; nodes that refer to them, provided the :dependencies-fn reports the
         ;; referenced proj-paths correctly.
         migrated-resource-node-ids
         (let [render-progress! (progress/nest-render-progress render-progress! total-progress load-progress-span)]
           (du/measuring process-metrics :load-new-nodes
             (load-nodes! project prelude-tx-data node-load-infos render-progress! resource-metrics transact-opts)))]

     ;; When we're not tracking changes, we will not evict stale values from the
     ;; system cache. This means subsequent graph queries won't see the changes
     ;; from the transaction if a value was previously cached. To be on the safe
     ;; side, we clear the cache after each transaction we perform with change
     ;; tracking disabled.
     (when-not change-tracked-transact
       (g/clear-system-cache!))

     (cache-loaded-save-data! node-load-infos project migrated-resource-node-ids)
     (render-progress! progress/done)

     (du/when-metrics
       (reset! load-metrics-atom
               {:new-nodes-by-path (g/node-value project :nodes-by-resource-path)
                :process-metrics @process-metrics
                :resource-metrics @resource-metrics
                :transaction-metrics @transaction-metrics}))
     project)))

(defn make-embedded-resource [project editability ext data]
  (let [workspace (g/node-value project :workspace)]
    (workspace/make-memory-resource workspace editability ext data)))

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

(defn upgraded-file-formats-save-data
  ([include-non-editable-directories project]
   (g/with-auto-evaluation-context evaluation-context
     (upgraded-file-formats-save-data include-non-editable-directories project evaluation-context)))
  ([include-non-editable-directories project evaluation-context]
   (let [upgraded-resource-type? (complement code.resource/code-resource-type?)

         upgraded-editable-save-data
         (filterv (comp upgraded-resource-type? resource/resource-type :resource)
                  (all-save-data project evaluation-context))]

     (if-not include-non-editable-directories
       upgraded-editable-save-data
       (let [live-run-evaluation-context (dissoc evaluation-context :dry-run)
             resources-by-proj-path (g/valid-node-value project :resource-map live-run-evaluation-context)
             resource-nodes-by-proj-path (g/valid-node-value project :nodes-by-resource-path live-run-evaluation-context)]
         (into upgraded-editable-save-data
               (keep (fn [[proj-path node-id]]
                       (when-let [resource (resources-by-proj-path proj-path)]
                         (when (and (not (resource/editable? resource))
                                    (resource/file-resource? resource)
                                    (resource/loaded? resource))
                           (let [resource-type (resource/resource-type resource)]
                             (when (and (:write-fn resource-type)
                                        (not (code.resource/code-resource-type? resource-type)))
                               (let [save-value (g/node-value node-id :save-value evaluation-context)]
                                 (when-not (g/error-value? save-value)
                                   (resource-node/make-save-data node-id resource save-value true)))))))))
               resource-nodes-by-proj-path))))))

(declare make-count-progress-steps-tracer make-progress-tracer)

(defn save-data-with-progress [project evaluation-context save-data-fn render-progress!]
  {:pre [(ifn? save-data-fn)
         (ifn? render-progress!)]}
  (ui/with-progress [render-progress! render-progress!]
    (let [step-count (AtomicLong.)
          step-count-tracer (make-count-progress-steps-tracer :save-data step-count)
          progress-message-fn (constantly (localization/message "progress.saving"))]
      (render-progress! (progress/make (localization/message "progress.saving")))
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
                                                                   localization/empty-message)))))

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

(handler/defhandler :edit.undo :global
  (enabled? [project-graph] (g/has-undo? project-graph))
  (run [project-graph]
    (g/undo! project-graph)
    (lsp/check-if-polled-resources-are-modified! (lsp/get-graph-lsp project-graph))))

(handler/defhandler :edit.redo :global
  (enabled? [project-graph] (g/has-redo? project-graph))
  (run [project-graph]
    (g/redo! project-graph)
    (lsp/check-if-polled-resources-are-modified! (lsp/get-graph-lsp project-graph))))

(handler/register-menu! ::menubar :editor.app-view/view
  [{:label (localization/message "menu.project")
    :id ::project
    :children [{:label (localization/message "command.project.build")
                :command :project.build}
               {:label (localization/message "command.project.clean-build")
                :command :project.clean-build}
               {:label (localization/message "command.project.build-html5")
                :command :project.build-html5}
               {:label (localization/message "command.project.clean-build-html5")
                :command :project.clean-build-html5}
               {:label (localization/message "command.project.bundle")
                :id ::bundle
                :command :project.bundle}
               {:label (localization/message "command.project.rebundle")
                :command :project.rebundle}
               {:label (localization/message "command.project.fetch-libraries")
                :command :project.fetch-libraries}
               {:label (localization/message "command.project.reload-editor-scripts")
                :command :project.reload-editor-scripts}
               {:label :separator}
               {:label (localization/message "command.file.open-shared-editor-settings")
                :command :file.open-shared-editor-settings}
               {:label (localization/message "command.file.open-liveupdate-settings")
                :command :file.open-liveupdate-settings}
               {:label :separator
                :id ::targets}
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
  (let [process-metrics (du/make-metrics-collector)
        resource-metrics (du/make-metrics-collector)
        transaction-metrics (du/make-metrics-collector)
        transact-opts (du/when-metrics {:metrics transaction-metrics})

        collected-properties-by-resource
        (du/measuring process-metrics :collect-overridden-properties
          (g/with-auto-evaluation-context evaluation-context
            (into {}
                  (map (fn [[resource old-node-id]]
                         (pair resource
                               (du/measuring resource-metrics (resource/proj-path resource) :collect-overridden-properties
                                 (g/collect-overridden-properties old-node-id evaluation-context)))))
                  (:transfer-overrides plan))))

        old-evaluation-context (g/make-evaluation-context)
        old-basis (:basis old-evaluation-context)
        old-node-ids-by-proj-path (g/valid-node-value project :nodes-by-resource-path old-evaluation-context)
        project-graph (g/node-id->graph-id project)
        new-node-id+resource-pairs (make-node-id+resource-pairs project-graph (:new plan))

        new-node-ids-by-proj-path
        (into {}
              (map (fn [[node-id resource]]
                     (let [proj-path (resource/proj-path resource)]
                       (pair proj-path node-id))))
              new-node-id+resource-pairs)

        resource->new-node-id (comp new-node-ids-by-proj-path resource/proj-path)
        resource->old-node-id (comp old-node-ids-by-proj-path resource/proj-path)

        resource->node-id
        (fn resource->node-id [resource]
          ;; When transferring overrides and arcs, the target is either a newly
          ;; created or already (still!) existing node.
          (or (resource->new-node-id resource)
              (resource->old-node-id resource)))]

    ;; Create the new nodes in the graph.
    (du/measuring process-metrics :make-new-nodes
      (g/transact transact-opts
        (make-resource-nodes-tx-data project new-node-id+resource-pairs)))

    ;; Transfer of overrides must happen before we delete the original nodes below.
    ;; The new target nodes do not need to be loaded. When loading the new targets,
    ;; corresponding override-nodes for the incoming connections will be created in the
    ;; overrides.
    (du/measuring process-metrics :transfer-overrides
      (g/transact transact-opts
        (du/if-metrics
          ;; Doing metrics - Submit separate transaction steps for each resource.
          (for [[resource old-node-id] (:transfer-overrides plan)]
            (g/transfer-overrides {old-node-id (resource->node-id resource)}))

          ;; Not doing metrics - submit as a single transaction step.
          (g/transfer-overrides
            (into {}
                  (map (fn [[resource old-node-id]]
                         (pair old-node-id
                               (resource->node-id resource))))
                  (:transfer-overrides plan))))))

    ;; must delete old versions of resource nodes before loading to avoid
    ;; load functions finding these when doing lookups of dependencies...
    (du/measuring process-metrics :delete-old-nodes
      (g/transact transact-opts
        (for [node-id (:delete plan)]
          (g/delete-node node-id))))

    (let [read-progress-span 1
          load-progress-span 3
          total-progress-span (+ read-progress-span load-progress-span)
          total-progress (progress/make localization/empty-message total-progress-span 0)
          deleted-node-id? (set (:delete plan))

          old-node-id->old-node-state
          (fn old-node-id->old-node-state [old-node-id]
            ;; Returns the state of the supplied old-node-id as it existed in
            ;; the graph before the resource change plan was executed.
            (if (resource-node/loaded? old-basis old-node-id)
              :loaded
              :not-loaded))

          old-node-id->dependency-proj-paths
          (fn/memoize
            (fn old-node-id->dependency-proj-paths [old-node-id]
              (when (and (not (deleted-node-id? old-node-id))
                         (= :loaded (old-node-id->old-node-state old-node-id)))
                (let [resource (g/valid-node-value old-node-id :resource old-evaluation-context)]
                  (du/measuring resource-metrics (resource/proj-path resource) :find-old-reload-dependencies
                    (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
                      (let [save-value (g/node-value old-node-id :save-value old-evaluation-context)]
                        (when-not (g/error? save-value)
                          (dependencies-fn save-value)))))))))

          node-load-infos
          (let [render-progress! (progress/nest-render-progress render-progress! total-progress read-progress-span)]
            (du/measuring process-metrics :read-new-nodes
              (read-nodes new-node-id+resource-pairs
                :old-node-id->dependency-proj-paths old-node-id->dependency-proj-paths
                :old-node-id->old-node-state old-node-id->old-node-state
                :old-node-ids-by-proj-path old-node-ids-by-proj-path
                :render-progress! render-progress!
                :resource-metrics resource-metrics)))

          total-progress (progress/advance total-progress read-progress-span)

          migrated-node-ids
          (let [render-progress! (progress/nest-render-progress render-progress! total-progress load-progress-span)]
            (du/measuring process-metrics :load-new-nodes
              (load-nodes! project nil node-load-infos render-progress! resource-metrics transact-opts)))]

      (du/measuring process-metrics :update-cache-with-dependencies
        ;; Write any cached values created for old non-replaced resource node
        ;; outputs during the process of querying their dependencies.
        (g/update-cache-from-evaluation-context! old-evaluation-context))
      (du/measuring process-metrics :update-cache-with-save-data
        (cache-loaded-save-data! node-load-infos project migrated-node-ids))
      (render-progress! progress/done))

    (du/measuring process-metrics :transfer-outgoing-arcs
      (g/transact transact-opts
        (for [[source-resource output-arcs] (:transfer-outgoing-arcs plan)]
          (let [source-node (resource->node-id source-resource)
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
            (when-some [new-node-id (resource->new-node-id resource)]
              (du/measuring resource-metrics (resource/proj-path resource) :restore-overridden-properties
                (let [restore-properties-tx-data (g/restore-overridden-properties new-node-id collected-properties evaluation-context)]
                  (when (seq restore-properties-tx-data)
                    (g/transact transact-opts restore-properties-tx-data)))))))
        (let [restore-properties-tx-data
              (g/with-auto-evaluation-context evaluation-context
                (into []
                      (mapcat (fn [[resource collected-properties]]
                                (when-some [new-node-id (resource->new-node-id resource)]
                                  (g/restore-overridden-properties new-node-id collected-properties evaluation-context))))
                      collected-properties-by-resource))]
          (when (seq restore-properties-tx-data)
            (g/transact transact-opts
              restore-properties-tx-data)))))

    (du/measuring process-metrics :update-selection
      (let [old->new (into {}
                           (map (fn [[p n]]
                                  [(old-node-ids-by-proj-path p) n]))
                           new-node-ids-by-proj-path)
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
              {:old-nodes-by-path old-node-ids-by-proj-path
               :new-nodes-by-path new-node-ids-by-proj-path
               :process-metrics @process-metrics
               :resource-metrics @resource-metrics
               :transaction-metrics @transaction-metrics}))))

(defn reload-plugins! [project touched-resources]
  (g/with-auto-evaluation-context evaluation-context
    (let [workspace (workspace project evaluation-context)
          localization (workspace/localization workspace evaluation-context)
          code-preprocessors (workspace/code-preprocessors workspace evaluation-context)
          code-transpilers (code-transpilers project)]
      (workspace/unpack-editor-plugins! workspace touched-resources)
      (code.preprocessors/reload-lua-preprocessors! code-preprocessors java/class-loader localization)
      (code.transpilers/reload-lua-transpilers! code-transpilers workspace java/class-loader localization)
      (texture.engine/reload-texture-compressors! java/class-loader localization)
      (workspace/load-clojure-editor-plugins! workspace touched-resources))))

(defn settings
  ([project]
   (g/with-auto-evaluation-context evaluation-context
     (settings project evaluation-context)))
  ([project evaluation-context]
   (g/node-value project :settings evaluation-context)))

(defn project-dependencies
  ([project]
   (g/with-auto-evaluation-context evaluation-context
     (project-dependencies project evaluation-context)))
  ([project evaluation-context]
   (when-let [settings (settings project evaluation-context)]
     (settings ["project" "dependencies"]))))

(defn update-fetch-libraries-notification
  "Create transaction steps for showing or hiding a 'Fetch Libraries' suggestion
  when the project dependency list differs from the currently installed
  dependencies in the workspace."
  [project evaluation-context]
  (when-let [workspace (workspace project evaluation-context)]
    (let [ignored-dep (:default (:element (settings-core/get-meta-setting gpc/meta-settings ["project" "dependencies"])))
          desired-deps (disj (set (project-dependencies project evaluation-context)) ignored-dep)
          installed-deps (set (workspace/dependencies workspace evaluation-context))
          notifications (workspace/notifications workspace evaluation-context)
          notification-id ::dependencies-changed]
      (if (not= desired-deps installed-deps)
        (notifications/show
          notifications
          {:id notification-id
           :type :info
           :message (localization/message "notification.fetch-libraries.dependencies-changed.prompt")
           :actions [{:message (localization/message "notification.fetch-libraries.dependencies-changed.action.fetch")
                      :on-action #(ui/execute-command
                                    (ui/contexts (ui/main-scene))
                                    :project.fetch-libraries
                                    nil)}]})
        (notifications/close notifications notification-id)))))

(defn update-fetch-libraries-notification!
  "Show or hide a 'Fetch Libraries' suggestion when the project dependency list
  differs from the currently installed dependencies in the workspace."
  [project]
  (g/transact
    (g/with-auto-evaluation-context evaluation-context
      (update-fetch-libraries-notification project evaluation-context)))
  nil)

(defn- handle-resource-changes [project changes render-progress!]
  (reload-plugins! project (set/union (set (:added changes)) (set (:changed changes))))
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
    (lsp/apply-resource-changes! (lsp/get-node-lsp project) changes)
    ;; Suggest fetching libraries if dependencies changed externally.
    (update-fetch-libraries-notification! project)))

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

(g/defnk produce-default-sampler-filter-modes
  [default-tex-params]
  ;; Values are keyword equivalents to Material$MaterialDesc$FilterModeMin and Material$MaterialDesc$FilterModeMag.
  {:filter-mode-min-default
   (condp = (:min-filter default-tex-params)
     gl/nearest :filter-mode-min-nearest
     gl/linear :filter-mode-min-linear
     gl/nearest-mipmap-nearest :filter-mode-min-nearest-mipmap-nearest
     gl/nearest-mipmap-linear :filter-mode-min-nearest-mipmap-linear
     gl/linear-mipmap-nearest :filter-mode-min-linear-mipmap-nearest
     gl/linear-mipmap-linear :filter-mode-min-linear-mipmap-linear)

   :filter-mode-mag-default ; We convert the various mipmap filtering variants to non-mipmap ones.
   (condp = (:mag-filter default-tex-params)
     gl/nearest :filter-mode-mag-nearest
     gl/linear :filter-mode-mag-linear
     gl/nearest-mipmap-nearest :filter-mode-mag-nearest
     gl/nearest-mipmap-linear :filter-mode-mag-nearest
     gl/linear-mipmap-nearest :filter-mode-mag-linear
     gl/linear-mipmap-linear :filter-mode-mag-linear)})

(g/defnode Project
  (inherits core/Scope)

  (property workspace g/Any)

  (property all-selections g/Any)
  (property all-sub-selections g/Any)

  (input script-intelligence g/NodeID :cascade-delete)
  (input editor-extensions g/NodeID :cascade-delete)
  (input script-annotations g/NodeID :cascade-delete)
  (input editor-localization-bundle g/NodeID :cascade-delete)
  (input all-selected-node-ids g/Any :array)
  (input all-selected-node-properties g/Any :array)
  (input resources g/Any)
  (input resource-map g/Any)
  (input save-data g/Any :array :substitute gu/array-subst-remove-errors)
  (input node-id+resources g/Any :array)
  (input settings g/Any :substitute nil)
  (input display-profiles g/Any)
  (input texture-profiles g/Any)
  (input collision-group-nodes g/Any :array :substitute gu/array-subst-remove-errors)
  (input build-settings g/Any)
  (input dependencies g/Any)
  (input breakpoints Breakpoints :array :substitute gu/array-subst-remove-errors)
  (input proj-path+meta-info-pairs g/Any :array :substitute gu/array-subst-remove-errors)

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
  (output settings g/Any (g/fnk [settings] (or settings gpc/default-settings)))
  (output display-width g/Num (g/fnk [settings]
                                 (double (or (get settings ["display" "width"]) 0))))
  (output display-height g/Num (g/fnk [settings]
                                  (double (or (get settings ["display" "height"]) 0))))
  (output exclude-gles-sm100 g/Any (g/fnk [settings] (get settings ["shader" "exclude_gles_sm100"])))
  (output display-profiles g/Any :cached (gu/passthrough display-profiles))
  (output texture-profiles g/Any :cached (gu/passthrough texture-profiles))
  (output dependencies g/Any (gu/passthrough dependencies))
  (output nil-resource resource/Resource (g/constantly nil))
  (output collision-groups-data g/Any :cached produce-collision-groups-data)
  (output default-tex-params g/Any :cached produce-default-tex-params)
  (output default-sampler-filter-modes g/Any :cached produce-default-sampler-filter-modes)
  (output build-settings g/Any (gu/passthrough build-settings))
  (output breakpoints Breakpoints :cached (g/fnk [breakpoints] (into [] cat breakpoints)))
  (output meta-infos g/Any :cached
          (g/fnk [proj-path+meta-info-pairs]
            {:ext-meta-info
             (when-not (coll/empty? proj-path+meta-info-pairs)
               (transduce
                 (keep (fn [e]
                         (when (= "ext" (FilenameUtils/getBaseName (key e)))
                           (val e))))
                 settings-core/merge-meta-infos
                 proj-path+meta-info-pairs))

             :game-project-proj-path->additional-meta-info
             (coll/pair-map-by #(str (FilenameUtils/removeExtension (key %)) ".project") val proj-path+meta-info-pairs)})))

(defn get-project
  ([node]
   (get-project (g/now) node))
  ([basis node]
   (g/graph-value basis (g/node-id->graph-id node) :project-id)))

(defn find-resources [project query]
  (let [resource-path-to-node (g/node-value project :nodes-by-resource-path)
        resources        (resource/filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-path-to-node (resource/proj-path r))]) resources)))

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
  {:pre [(resource/resource? resource)]}
  (let [created-resource-nodes (:created-resource-nodes tx-data-context-map)
        pending-resource-node-id (get created-resource-nodes resource ::not-found)]
    (if (not= ::not-found pending-resource-node-id)
      [tx-data-context-map pending-resource-node-id nil]
      (let [graph-id (g/node-id->graph-id project)
            node-type (resource-node-type resource)
            creation-tx-data (g/make-nodes graph-id [resource-node-id [node-type :resource resource]]
                               (g/connect resource-node-id :_node-id project :nodes)
                               (g/connect resource-node-id :node-id+resource project :node-id+resources))
            created-resource-node-id (first (g/tx-data-added-node-ids creation-tx-data))
            created-resource-nodes' (assoc (or created-resource-nodes {}) resource created-resource-node-id)
            tx-data-context-map' (assoc tx-data-context-map :created-resource-nodes created-resource-nodes')]
        [tx-data-context-map' created-resource-node-id creation-tx-data]))))

(defn- ensure-resource-node-loaded [tx-data-context-map project node-id resource transpiler-tx-data-fn]
  {:pre [(resource/resource? resource)]}
  (let [loaded-resources (:loaded-resources tx-data-context-map)]
    (if (contains? loaded-resources resource)
      [tx-data-context-map nil nil]
      (let [workspace (resource/workspace resource)
            node-load-info (read-node-load-info node-id resource nil)
            {:keys [disk-sha256s-by-node-id node-id+source-value-pairs]} (node-load-infos->stored-disk-state [node-load-info])
            load-tx-data (e/concat
                           (workspace/merge-disk-sha256s workspace disk-sha256s-by-node-id)
                           (node-load-info-tx-data node-load-info project transpiler-tx-data-fn))
            loaded-resources' (conj (or loaded-resources #{}) resource)
            tx-data-context-map' (assoc tx-data-context-map :loaded-resources loaded-resources')]
        [tx-data-context-map' node-id+source-value-pairs load-tx-data]))))

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
  (when-some [resource (resolve-path-or-resource project path-or-resource evaluation-context)]
    (let [basis (:basis evaluation-context)
          tx-data-context-atom (:tx-data-context evaluation-context)
          existing-resource-node-id (get-resource-node project resource evaluation-context)
          [node-id creation-tx-data] (if existing-resource-node-id
                                       [existing-resource-node-id nil]
                                       (thread-util/swap-rest! tx-data-context-atom ensure-resource-node-created project resource))
          resource-type (resource/resource-type resource)
          node-type (resource-type->node-type resource-type)

          load-tx-data
          (cond
            ;; If we just created the resource node, that means the referenced
            ;; resource does not exist, or it would have already been created
            ;; during resource-sync. Mark it as loaded and defective.
            creation-tx-data
            (let [node-load-info (make-file-not-found-node-load-info node-id resource)]
              (node-load-info-tx-data node-load-info project nil))

            ;; If we're about to connect a defunloaded resource that does not
            ;; :allow-unloaded-use, ensure it is loaded as part of this
            ;; transaction before connecting it.
            (and existing-resource-node-id
                 (not (resource/loaded? resource))
                 (not (:allow-unloaded-use resource-type))
                 (not (resource-node/loaded? basis existing-resource-node-id)))
            (let [transpiler-tx-data-fn (get-transpiler-tx-data-fn! project evaluation-context)
                  [node-id+source-value-pairs load-tx-data] (thread-util/swap-rest! tx-data-context-atom ensure-resource-node-loaded project node-id resource transpiler-tx-data-fn)]
              (resource-node/merge-source-values! node-id+source-value-pairs)
              load-tx-data))]

      {:node-id node-id
       :created-in-tx (nil? existing-resource-node-id)
       :tx-data (vec
                  (flatten
                    (concat
                      creation-tx-data
                      load-tx-data
                      (gu/connect-existing-outputs node-type node-id consumer-node connections))))})))

(deftype ProjectResourceListener [project-id]
  resource/ResourceListener
  (handle-changes [this changes render-progress!]
    (handle-resource-changes project-id changes render-progress!)))

(defn make-project [graph workspace-id extensions]
  (let [plugin-graph (g/make-graph! :history false :volatility 2)
        code-preprocessors (workspace/code-preprocessors workspace-id)

        transpilers-id
        (first
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes plugin-graph [code-transpilers code.transpilers/CodeTranspilersNode]
                (g/connect code-preprocessors :lua-preprocessors code-transpilers :lua-preprocessors)))))
        project-id
    (second
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph
                  [script-intelligence si/ScriptIntelligenceNode
                   project [Project :workspace workspace-id]
                   script-annotations script-annotations/ScriptAnnotations
                   editor-localization-bundle editor-localization-bundle/EditorLocalizationBundle]
                (g/connect workspace-id :root script-annotations :root)
                (g/connect script-annotations :_node-id project :script-annotations)
                (g/connect editor-localization-bundle :_node-id project :editor-localization-bundle)
                (g/connect extensions :_node-id project :editor-extensions)
                (g/connect script-intelligence :_node-id project :script-intelligence)
                (g/connect workspace-id :build-settings project :build-settings)
                (g/connect workspace-id :dependencies project :dependencies)
                (g/connect workspace-id :resource-list project :resources)
                (g/connect workspace-id :resource-map project :resource-map)
                (g/set-graph-value graph :project-id project)
                (g/set-graph-value graph :lsp (lsp/make project get-resource-node))
                (g/set-graph-value graph :code-transpilers transpilers-id)))))]
    (reload-plugins! project-id (g/node-value project-id :resources))
    (workspace/add-resource-listener! workspace-id 1 (ProjectResourceListener. project-id))
    (g/reset-undo! graph)
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

(defn- cacheable-save-data-endpoints
  ([project]
   (cacheable-save-data-endpoints (g/now) project))
  ([basis project]
   (into []
         (mapcat
           (fn [[node-id]]
             (when-not (g/defective? basis node-id)
               (let [node-type (g/node-type* basis node-id)
                     output-cached? (g/cached-outputs node-type)]
                 (eduction
                   (filter output-cached?)
                   (map #(g/endpoint node-id %))
                   [:save-data :save-value])))))
         (g/sources-of basis project :save-data))))

(defn clear-cached-save-data! [project]
  (g/invalidate-outputs!
    (cacheable-save-data-endpoints project)))

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
        progress (atom (progress/make (localization/message "progress.updating-dependencies") 13 0))]
    (render-progress! @progress)

    ;; Fetch+install libs if we have network, otherwise fallback to disk state
    (if (workspace/dependencies-reachable? dependencies)
      (->> (workspace/fetch-and-validate-libraries workspace-id dependencies (progress/nest-render-progress render-progress! @progress 4))
           (workspace/install-validated-libraries! workspace-id))
      (workspace/set-project-dependencies! workspace-id (library/current-library-state (workspace/project-directory workspace-id) dependencies)))

    (render-progress! (swap! progress progress/advance 4 (localization/message "progress.syncing-resources")))
    (du/log-time "Initial resource sync"
      (workspace/resource-sync! workspace-id [] (progress/nest-render-progress render-progress! @progress)))
    (render-progress! (swap! progress progress/advance 1 (localization/message "progress.loading-project")))
    (let [project (make-project graph workspace-id extensions)
          populated-project (du/log-time "Project loading"
                              (load-project! project (progress/nest-render-progress render-progress! @progress 8)))]
      ;; Prime the auto completion cache
      (g/node-value (script-intelligence project) :lua-completions)
      (du/log-statistics! "Project loaded")
      populated-project)))

(defn resource-setter [evaluation-context self old-value new-value & connections]
  (let [project (get-project (:basis evaluation-context) self)]
    (concat
      (when old-value (disconnect-resource-node evaluation-context project old-value self connections))
      (when new-value (:tx-data (connect-resource-node evaluation-context project new-value self connections))))))

(defn node-refers-to-resource?
  [project node-id resource acc-fn]
  (let [path (resource/proj-path (g/node-value node-id :resource))]
    (loop [resources [resource]
           checked-paths #{}]
      (when-let [resource (first resources)]
        (let [current-path (resource/proj-path resource)]
          (or (= path current-path)
              (let [resources (rest resources)]
                (recur (if (contains? checked-paths current-path)
                         resources
                         (let [target-node (get-resource-node project resource)]
                           (cond-> resources
                             target-node
                             (concat (acc-fn target-node)))))
                       (conj checked-paths current-path)))))))))
