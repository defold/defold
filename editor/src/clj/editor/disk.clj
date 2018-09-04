(ns editor.disk
  (:require [dynamo.graph :as g]
            [editor.changes-view :as changes-view]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [editor.progress :as progress]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.workspace :as workspace]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Helpers
;; -----------------------------------------------------------------------------

(defn- async-job! [callback! job-atom start-job! & args]
  (let [job (swap! job-atom #(or % (apply start-job! args)))]
    (future
      (let [succeeded? (try
                         (deref job)
                         (catch Throwable error
                           (error-reporting/report-exception! error)
                           false))]
        (when (some? callback!)
          (ui/run-later
            (callback! succeeded?)))))
    nil))

(defn- blocking-job! [job-atom start-job! & args]
  ;; Calling this from the ui thread will cause a deadlock, since
  ;; the jobs rely on run-later. Use the async variants instead.
  (assert (not (ui/on-ui-thread?)))
  (let [succeeded? (deref (swap! job-atom #(or % (apply start-job! args))))]
    succeeded?))

;; -----------------------------------------------------------------------------
;; Reload
;; -----------------------------------------------------------------------------

(def ^:private reload-job-atom (atom nil))

(defn- start-reload-job! [render-progress! workspace moved-files changes-view]
  (let [project-path (workspace/project-path workspace)
        dependencies (workspace/dependencies workspace)
        snapshot-cache (workspace/snapshot-cache workspace)
        success-promise (promise)
        complete! (fn [success?]
                    (render-progress! progress/done)
                    (reset! reload-job-atom nil)
                    (deliver success-promise success?))
        fail! (fn [error]
                (error-reporting/report-exception! error)
                (complete! false))]
    (future
      (try
        (render-progress! (progress/make "Loading external changes..."))
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

(def blocking-reload! (partial blocking-job! reload-job-atom start-reload-job!))

;; -----------------------------------------------------------------------------
;; Save
;; -----------------------------------------------------------------------------

(def ^:private save-job-atom (atom nil))

(defn- start-save-job! [render-reload-progress! render-save-progress! project changes-view]
  (let [workspace (project/workspace project)
        success-promise (promise)
        complete! (fn [success?]
                    (render-save-progress! progress/done)
                    (reset! save-job-atom nil)
                    (deliver success-promise success?))
        fail! (fn [error]
                (error-reporting/report-exception! error)
                (complete! false))]
    (future
      ;; Reload any external changes first, so these will not
      ;; be overwritten if we have not detected them yet.
      (if-not (blocking-reload! render-reload-progress! workspace [] nil)
        (complete! false) ; Errors were already reported by blocking-reload!
        (try
          (render-save-progress! (progress/make "Saving..."))
          (ui/run-later
            (try
              (let [evaluation-context (g/make-evaluation-context)]
                (future
                  (try
                    (let [save-data (project/dirty-save-data project evaluation-context)]
                      (project/write-save-data-to-disk! save-data {:render-progress! render-save-progress!})
                      (render-save-progress! (progress/make "Refreshing file status..."))
                      (let [updated-file-resource-status-entries (into {}
                                                                       (map (comp resource-watch/file-resource-status-map-entry :resource))
                                                                       save-data)]
                        (render-save-progress! progress/done)
                        (ui/run-later
                          (try
                            (g/update-cache-from-evaluation-context! evaluation-context)
                            (g/update-property! workspace :resource-snapshot merge updated-file-resource-status-entries)
                            (project/invalidate-save-data-source-values! save-data)
                            (when (some? changes-view)
                              (changes-view/refresh! changes-view render-reload-progress!))
                            (complete! true)
                            (catch Throwable error
                              (fail! error))))))
                    (catch Throwable error
                      (fail! error)))))
              (catch Throwable error
                (fail! error))))
          (catch Throwable error
            (fail! error)))))
    success-promise))

(defn async-save!
  ([render-reload-progress! render-save-progress! project changes-view]
   (async-save! render-reload-progress! render-save-progress! project changes-view nil))
  ([render-reload-progress! render-save-progress! project changes-view callback!]
   (async-job! callback! save-job-atom start-save-job! render-reload-progress! render-save-progress! project changes-view)))

;; -----------------------------------------------------------------------------
;; General
;; -----------------------------------------------------------------------------

(defn available? []
  (and (nil? @save-job-atom)
       (nil? @reload-job-atom)))
