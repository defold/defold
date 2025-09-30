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

(ns editor.disk
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.changes-view :as changes-view]
            [editor.defold-project :as project]
            [editor.disk-availability :as disk-availability]
            [editor.editor-extensions :as extensions]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.error-reporting :as error-reporting]
            [editor.localization :as localization]
            [editor.lsp :as lsp]
            [editor.pipeline.bob :as bob]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest]
            [util.text-util :as text-util]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Helpers
;; -----------------------------------------------------------------------------

(defn- async-job! [callback! job-atom start-job! & args]
  (disk-availability/push-busy!)
  (let [job (swap! job-atom #(or % (apply start-job! args)))]
    (future
      (let [successful? (try
                          (deref job)
                          (catch Throwable error
                            (error-reporting/report-exception! error)
                            false))]
        (if (nil? callback!)
          (disk-availability/pop-busy!)
          (ui/run-later
            (try
              (callback! successful?)
              (finally
                (disk-availability/pop-busy!)))))))
    nil))

(defn- blocking-job! [job-atom start-job! & args]
  ;; Calling this from the ui thread will cause a deadlock, since
  ;; the jobs rely on run-later. Use the async variants instead.
  (assert (not (ui/on-ui-thread?)))
  (let [successful? (deref (swap! job-atom #(or % (apply start-job! args))))]
    successful?))

;; -----------------------------------------------------------------------------
;; Reload
;; -----------------------------------------------------------------------------

(defonce ^:private reload-job-atom (atom nil))

(defn- start-reload-job! [render-progress! workspace moved-files changes-view]
  (let [project-directory (workspace/project-directory workspace)
        dependencies (workspace/dependencies workspace)
        snapshot-cache (workspace/snapshot-cache workspace)
        success-promise (promise)
        complete! (fn [successful?]
                    (render-progress! progress/done)
                    (reset! reload-job-atom nil)
                    (deliver success-promise successful?))
        fail! (fn [error]
                (error-reporting/report-exception! error)
                (complete! false))]
    (future
      (try
        (render-progress! (progress/make-indeterminate (localization/message "progress.loading-external-changes")))
        (let [snapshot-info (workspace/make-snapshot-info workspace project-directory dependencies snapshot-cache)]
          (render-progress! progress/done)
          (ui/run-later
            (try
              (workspace/update-snapshot-cache! workspace (:snapshot-cache snapshot-info))
              (let [diff (workspace/resource-sync! workspace moved-files render-progress! (:snapshot snapshot-info) (:map snapshot-info))]
                (when (some? changes-view)
                  ;; The call to resource-sync! will refresh the changes view if
                  ;; it detected changes, but committing a file from the command
                  ;; line will not actually change the file as far as
                  ;; resource-sync! is concerned. To ensure the changed files
                  ;; view reflects the current Git state, we explicitly refresh
                  ;; the changes view here if the call to resource-sync! would
                  ;; not have already done so.
                  (when (resource-watch/empty-diff? diff)
                    (changes-view/refresh! changes-view))))
              (complete! true)
              (catch Throwable error
                (fail! error)))))
        (catch Throwable error
          (fail! error))))
    success-promise))

(defn async-reload!
  ([render-progress! workspace moved-files changes-view]
   (async-reload! render-progress! workspace moved-files changes-view nil))
  ([render-progress! workspace moved-files changes-view callback!]
   (async-job! callback! reload-job-atom start-reload-job! render-progress! workspace moved-files changes-view)))

(def ^:private blocking-reload! (partial blocking-job! reload-job-atom start-reload-job!))

(defn await-current-reload
  "If a reload is in progress, blocks until done; otherwise returns immediately"
  []
  (some-> @reload-job-atom deref))

;; -----------------------------------------------------------------------------
;; Save
;; -----------------------------------------------------------------------------

(defonce ^:private save-job-atom (atom nil))
(def ^:private save-data-status-map-entry (comp resource-watch/file-resource-status-map-entry :resource))

(defn make-post-save-actions [written-save-datas written-disk-sha256s snapshot-invalidate-counters]
  {:pre [(vector? written-save-datas)
         (vector? written-disk-sha256s)
         (= (count written-save-datas) (count written-disk-sha256s))
         (or (nil? snapshot-invalidate-counters) (map? snapshot-invalidate-counters))]}
  (into {:snapshot-invalidate-counters snapshot-invalidate-counters
         :written-save-datas written-save-datas}
        (zipmap
          [:written-file-resource-status-map-entries
           :written-source-values-by-node-id
           :written-disk-sha256s-by-node-id]
          (util/into-multiple
            [[] {} {}]
            [(map save-data-status-map-entry)
             (map (fn [{:keys [node-id resource save-value]}]
                    (let [resource-type (resource/resource-type resource)
                          value (resource-node/save-value->source-value save-value resource-type)]
                      (pair node-id value))))
             (map-indexed (fn [index {:keys [node-id]}]
                            (let [disk-sha256 (written-disk-sha256s index)]
                              (pair node-id disk-sha256))))]
            written-save-datas))))

(defn process-post-save-actions! [workspace post-save-actions]
  (g/transact
    (concat
      (g/update-property workspace :resource-snapshot resource-watch/update-snapshot-status (:written-file-resource-status-map-entries post-save-actions))
      (workspace/merge-disk-sha256s workspace (:written-disk-sha256s-by-node-id post-save-actions))))
  (let [endpoint-invalidated-since-snapshot? (g/endpoint-invalidated-pred (:snapshot-invalidate-counters post-save-actions))]
    (resource-node/merge-source-values! (:written-source-values-by-node-id post-save-actions))
    (g/cache-output-values!
      (into []
            (keep (fn [{:keys [node-id] :as save-data}]
                    ;; It's possible the user might have edited a resource
                    ;; while we were saving on a background thread. We need to
                    ;; make sure we don't add a stale save-data entry to the
                    ;; cache.
                    (let [save-data-endpoint (g/endpoint node-id :save-data)]
                      (when-not (endpoint-invalidated-since-snapshot? save-data-endpoint)
                        (pair save-data-endpoint
                              (assoc save-data :dirty false))))))
            (:written-save-datas post-save-actions)))
    (project/log-cache-info! (g/cache) "Cached written save data in system cache.")))

(defn- write-message-fn [save-data]
  (when-let [resource (:resource save-data)]
    (localization/message "progress.writing" {"resource" (resource/resource->proj-path resource)})))

(defn write-save-data-to-disk!
  [save-datas snapshot-invalidate-counters {:keys [render-progress!]
                                            :or {render-progress! progress/null-render-progress!}
                                            :as _opts}]
  "Write the supplied sequence of save-datas to disk. Returns post-save-actions
  that must later be supplied to the process-post-save-actions! function, called
  from the main thread."
  (render-progress! (progress/make (localization/message "progress.writing-files")))
  (if (g/error? save-datas)
    (throw (Exception. (g/error-message save-datas)))
    (let [written-save-datas
          (filterv (fn [{:keys [resource]}]
                     (not (resource/read-only? resource)))
                   save-datas)

          written-disk-sha256s
          (progress/progress-mapv
            (fn [save-data _progress]
              (let [resource (:resource save-data)
                    content (resource-node/save-data-content save-data)

                    ;; If the file is non-binary, convert line endings to the
                    ;; type used by the existing file.
                    ;; TODO(save-value-cleanup): Could we achieve this using a FilterOutputStream and avoid allocating new strings?
                    ^String written-content
                    (if (and (resource/exists? resource)
                             (resource/textual? resource)
                             (= :crlf (text-util/guess-line-endings (io/make-reader resource nil))))
                      (text-util/lf->crlf content)
                      content)

                    digest-output-stream
                    (-> resource
                        (io/output-stream)
                        (digest/make-digest-output-stream "SHA-256"))]
                (spit digest-output-stream written-content) ; This will flush and close the stream.
                (digest/completed-stream->hex digest-output-stream)))
            written-save-datas
            render-progress!
            write-message-fn)]
      (make-post-save-actions written-save-datas written-disk-sha256s snapshot-invalidate-counters))))

(defn- start-save-job! [render-reload-progress! render-save-progress! save-data-fn project changes-view]
  {:pre [(ifn? render-reload-progress!)
         (ifn? render-save-progress!)
         (ifn? save-data-fn)
         (g/node-id? project)
         (or (nil? changes-view) (g/node-id? changes-view))]}
  (let [workspace (project/workspace project)
        success-promise (promise)
        complete! (fn [successful?]
                    (render-save-progress! progress/done)
                    (reset! save-job-atom nil)
                    (deliver success-promise successful?))
        fail! (fn [error]
                (error-reporting/report-exception! error)
                (complete! false))]
    (future
      (try
        ;; It is safe to save any dirty save-datas without performing a reload
        ;; first, because files are only considered dirty if their save-value
        ;; differs from the value we last loaded or saved ourselves. If instead,
        ;; we considered a file dirty when its save-value differs from the value
        ;; on disk at the time of saving, we'd have to first perform a reload to
        ;; ensure we do not overwrite any external changes with our un-edited
        ;; save-values.
        (let [evaluation-context (g/make-evaluation-context)
              snapshot-invalidate-counters (g/evaluation-context-invalidate-counters evaluation-context)
              save-data (project/save-data-with-progress project evaluation-context save-data-fn render-save-progress!)
              post-save-actions (write-save-data-to-disk! save-data snapshot-invalidate-counters {:render-progress! render-save-progress!})
              written-resources (into #{} (map :resource) save-data)
              reload-required (some #(= "/.defignore" (resource/proj-path %)) written-resources)]
          (render-save-progress! (progress/make-indeterminate (localization/message "progress.caching-save-results")))
          (ui/run-later
            (try
              (project/update-system-cache-save-data! evaluation-context)
              (process-post-save-actions! workspace post-save-actions)
              (future
                (try
                  (render-save-progress! (progress/make-indeterminate (localization/message "progress.reading-timestamps")))
                  (project/reload-plugins! project written-resources)
                  (lsp/touch-resources! (lsp/get-node-lsp project) written-resources)
                  (cond
                    reload-required
                    (complete! (blocking-reload! render-reload-progress! workspace [] changes-view))

                    (and changes-view (coll/not-empty written-resources))
                    (do
                      (changes-view/refresh! changes-view)
                      (complete! true))

                    :else
                    (complete! true))
                  (catch Throwable error
                    (fail! error))))
              (catch Throwable error
                (fail! error)))))
        (catch Throwable error
          (fail! error))))
    success-promise))

(defn async-save!
  ([render-reload-progress! render-save-progress! save-data-fn project changes-view]
   (async-save! render-reload-progress! render-save-progress! save-data-fn project changes-view nil))
  ([render-reload-progress! render-save-progress! save-data-fn project changes-view callback!]
   {:pre [(ifn? render-reload-progress!)
          (ifn? render-save-progress!)
          (ifn? save-data-fn)
          (g/node-id? project)
          (or (nil? changes-view) (g/node-id? changes-view))
          (or (nil? callback!) (ifn? callback!))]}
   (async-job! callback! save-job-atom start-save-job! render-reload-progress! render-save-progress! save-data-fn project changes-view)))

;; -----------------------------------------------------------------------------
;; Bob build
;; -----------------------------------------------------------------------------

(defn- handle-bob-error! [render-error! project evaluation-context {:keys [error exception] :as _result}]
  (cond
    error
    (do (render-error! error)
        true)

    exception
    (do (render-error! (engine-build-errors/exception->error-value exception project evaluation-context))
        true)))

(defn async-bob-build! [render-reload-progress! render-save-progress! render-build-progress! log-output-stream task-cancelled? render-build-error! bob-commands bob-options project changes-view callback!]
  (disk-availability/push-busy!)
  (future
    (try
      (let [invoke-bundle-hooks (boolean (some #(= "bundle" %) bob-commands))
            hook-opts {:output-directory (or (get bob-options "bundle-output")
                                             (get bob-options "output")
                                             "build/default")
                       :platform (get bob-options "platform")
                       :variant (get bob-options "variant" "release")}]
        (render-reload-progress! (progress/make-indeterminate (localization/message "progress.executing-bundle-hook")))
        (if-let [extension-error (when invoke-bundle-hooks
                                   @(extensions/execute-hook! project
                                                              :on_bundle_started
                                                              hook-opts
                                                              :exception-policy :as-error))]
          (try
            (when invoke-bundle-hooks
              @(extensions/execute-hook! project
                                         :on_bundle_finished
                                         (assoc hook-opts :success false)
                                         :exception-policy :ignore))
            (ui/run-later
              (try
                (handle-bob-error! render-build-error! project (g/make-evaluation-context) {:error extension-error})
                (when (some? callback!) (callback! false))
                (finally
                  (disk-availability/pop-busy!)
                  (render-reload-progress! progress/done))))
            (catch Throwable error
              (disk-availability/pop-busy!)
              (render-reload-progress! progress/done)
              (throw error)))
          (do
            (render-reload-progress! progress/done)

            ;; We need to save because bob reads from FS.
            (async-save!
              render-reload-progress! render-save-progress! project/dirty-save-data project changes-view
              (fn [successful?]
                (if-not successful?
                  (try
                    (when (some? callback!)
                      (callback! false))
                    (finally
                      (disk-availability/pop-busy!)))
                  (try
                    (render-build-progress! (progress/make-cancellable-indeterminate (localization/message "progress.building")))
                    ;; evaluation-context below is used to map
                    ;; project paths to resource node id:s. To be
                    ;; strictly correct, we should probably re-use
                    ;; the ec created when saving - so the graph
                    ;; state in the ec corresponds with the state
                    ;; bob sees on disk.
                    (let [evaluation-context (g/make-evaluation-context)]
                      (future
                        (try
                          (let [result (bob/invoke! project bob-options bob-commands
                                                    :task-cancelled? task-cancelled?
                                                    :render-progress! render-build-progress!
                                                    :evaluation-context evaluation-context
                                                    :log-output-stream log-output-stream)]
                            (when invoke-bundle-hooks
                              @(extensions/execute-hook!
                                 project
                                 :on_bundle_finished
                                 (assoc hook-opts
                                   :success (not (or (:error result)
                                                     (:exception result))))
                                 :exception-policy :ignore))
                            (render-build-progress! progress/done)
                            (ui/run-later
                              (try
                                (let [successful? (not (handle-bob-error! render-build-error! project evaluation-context result))]
                                  (when (some? callback!)
                                    (callback! successful?)))
                                (finally
                                  (disk-availability/pop-busy!)
                                  (g/update-cache-from-evaluation-context! evaluation-context)))))
                          (catch Throwable error
                            (disk-availability/pop-busy!)
                            (render-build-progress! progress/done)
                            (error-reporting/report-exception! error)))))
                    (catch Throwable error
                      (disk-availability/pop-busy!)
                      (throw error)))))))))
      (catch Throwable error
        (disk-availability/pop-busy!)
        (render-build-progress! progress/done)
        (error-reporting/report-exception! error)))))
