;; Copyright 2020-2022 The Defold Foundation
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
  (:require [dynamo.graph :as g]
            [editor.changes-view :as changes-view]
            [editor.defold-project :as project]
            [editor.disk-availability :as disk-availability]
            [editor.editor-extensions :as extensions]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.error-reporting :as error-reporting]
            [editor.pipeline.bob :as bob]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.workspace :as workspace]))

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

(def ^:private reload-job-atom (atom nil))

(defn- start-reload-job! [render-progress! workspace moved-files changes-view]
  (let [project-path (workspace/project-path workspace)
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
        (render-progress! (progress/make-indeterminate "Loading external changes..."))
        (let [snapshot-info (workspace/make-snapshot-info workspace project-path dependencies snapshot-cache)]
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
                    (changes-view/refresh! changes-view render-progress!))))
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

;; -----------------------------------------------------------------------------
;; Save
;; -----------------------------------------------------------------------------

(def ^:private save-job-atom (atom nil))
(def ^:private save-data-status-map-entry (comp resource-watch/file-resource-status-map-entry :resource))

(defn- start-save-job! [render-reload-progress! render-save-progress! project changes-view]
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
        ;; Reload any external changes first, so these will not
        ;; be overwritten if we have not detected them yet.
        (if-not (blocking-reload! render-reload-progress! workspace [] nil)
          (complete! false) ; Errors were already reported by blocking-reload!
          (let [evaluation-context (g/make-evaluation-context)
                save-data (project/dirty-save-data-with-progress project evaluation-context render-save-progress!)]
            (project/write-save-data-to-disk! save-data {:render-progress! render-save-progress!})
            (if (and (some #(= "/.defignore" (resource/proj-path (:resource %))) save-data)
                     (not (blocking-reload! render-reload-progress! workspace [] nil)))
              (complete! false)
              (do
                (render-save-progress! (progress/make-indeterminate "Reading timestamps..."))
                (let [updated-file-resource-status-map-entries (mapv save-data-status-map-entry save-data)]
                  (render-save-progress! progress/done)
                  (ui/run-later
                    (try
                      (project/update-system-cache-save-data! evaluation-context)
                      (g/update-property! workspace :resource-snapshot resource-watch/update-snapshot-status updated-file-resource-status-map-entries)
                      (project/invalidate-save-data-source-values! save-data)
                      (when (some? changes-view)
                        (changes-view/refresh! changes-view render-reload-progress!))
                      (complete! true)
                      (catch Throwable error
                        (fail! error)))))))))
        (catch Throwable error
          (fail! error))))
    success-promise))

(defn async-save!
  ([render-reload-progress! render-save-progress! project changes-view]
   (async-save! render-reload-progress! render-save-progress! project changes-view nil))
  ([render-reload-progress! render-save-progress! project changes-view callback!]
   (async-job! callback! save-job-atom start-save-job! render-reload-progress! render-save-progress! project changes-view)))

;; -----------------------------------------------------------------------------
;; Bob build
;; -----------------------------------------------------------------------------

(defn- handle-bob-error! [render-error! project evaluation-context {:keys [error exception] :as _result}]
  (cond
    error
    (do (render-error! error)
        true)

    exception
    (do (engine-build-errors/handle-build-error! render-error! project evaluation-context exception)
        true)))

(defn async-bob-build! [render-reload-progress! render-save-progress! render-build-progress! show-build-log-stream! task-cancelled? render-build-error! bob-commands bob-args project changes-view callback!]
  (disk-availability/push-busy!)
  (future
    (try
      (let [hook-opts {:output-directory (get bob-args "bundle-output")
                       :platform (get bob-args "platform")
                       :variant (get bob-args "variant")}]
        (render-reload-progress! (progress/make-indeterminate "Executing bundle hook..."))
        (if-let [extension-error (extensions/execute-hook! project :on-bundle-started
                                                           {:exception-policy :as-error
                                                            :opts hook-opts})]
          (try
            (extensions/execute-hook! project :on-bundle-finished {:exception-policy :ignore
                                                                   :opts (assoc hook-opts :success false)})
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
          ;; We need to save because bob reads from FS.
          (async-save! render-reload-progress! render-save-progress! project changes-view
                       (fn [successful?]
                         (if-not successful?
                           (try
                             (when (some? callback!)
                               (callback! false))
                             (finally
                               (disk-availability/pop-busy!)))
                           (try
                             (render-build-progress! (progress/make-cancellable-indeterminate "Building..."))
                             ;; evaluation-context below is used to map
                             ;; project paths to resource node id:s. To be
                             ;; strictly correct, we should probably re-use
                             ;; the ec created when saving - so the graph
                             ;; state in the ec corresponds with the state
                             ;; bob sees on disk.
                             (let [evaluation-context (g/make-evaluation-context)]
                               (future
                                 (try
                                   (let [result (bob/bob-build! project evaluation-context bob-commands bob-args render-build-progress! show-build-log-stream! task-cancelled?)]
                                     (extensions/execute-hook!
                                       project
                                       :on-bundle-finished
                                       {:exception-policy :ignore
                                        :opts (assoc hook-opts
                                                :success (not (or (:error result)
                                                                  (:exception result))))})
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
                               (throw error))))))))
      (catch Throwable error
        (disk-availability/pop-busy!)
        (render-build-progress! progress/done)
        (error-reporting/report-exception! error)))))
