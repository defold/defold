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

(ns editor.boot
  (:require [clojure.java.io :as io]
            [clojure.stacktrace :as stack]
            [clojure.tools.cli :as cli]
            [editor.analytics :as analytics]
            [editor.connection-properties :refer [connection-properties]]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.gl :as gl]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [editor.welcome :as welcome]
            [service.log :as log]
            [util.repo :as repo])
  (:import [com.defold.editor Shutdown]
           [com.dynamo.bob.archive EngineVersion]
           [java.util Arrays]
           [javax.imageio ImageIO]))

(set! *warn-on-reflection* true)

(def namespace-counter (atom 0))
(def namespace-progress-reporter (atom nil))

(alter-var-root (var clojure.core/load-lib)
                (fn [f]
                  (fn [prefix lib & options]
                    (swap! namespace-counter inc)
                    (when @namespace-progress-reporter
                      (@namespace-progress-reporter
                       #(progress/jump %
                                       @namespace-counter
                                       (str "Initializing editor " (if prefix
                                                                     (str prefix "." lib)
                                                                     (str lib))))))
                    (apply f prefix lib options))))

(defn- open-project-with-progress-dialog
  [namespace-loader user-prefs project updater newly-created?]
  (dialogs/make-load-project-dialog
    (fn [render-progress!]
      (let [namespace-progress (progress/make "Loading editor" 1471) ; Magic number from printing namespace-counter after load. Connecting a REPL skews the result!
            render-namespace-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 0) 1)
            render-project-progress! (progress/nest-render-progress render-progress! (progress/make "Loading" 5 1) 4)
            project-file (io/file project)
            project-dir (.getParentFile project-file)
            project-prefs (doto (prefs/project project-dir user-prefs) prefs/migrate-project-prefs!)]
        (welcome/add-recent-project! project-prefs project-file)
        (reset! namespace-progress-reporter #(render-namespace-progress! (% namespace-progress)))

        ;; Ensure that namespace loading has completed.
        @namespace-loader

        ;; Initialize the system and load the project.
        (let [system-config (apply (var-get (ns-resolve 'editor.shared-editor-settings 'load-project-system-config)) [project-dir])]
          (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-systems!)) [project-prefs])
          (apply (var-get (ns-resolve 'editor.boot-open-project 'initialize-project!)) [system-config])
          (apply (var-get (ns-resolve 'editor.boot-open-project 'open-project!)) [project-file project-prefs render-project-progress! updater newly-created?])
          (reset! namespace-progress-reporter nil))))))

(defn- select-project-from-welcome
  [namespace-loader prefs updater]
  (ui/run-later
    (welcome/show-welcome-dialog! prefs updater
                                  (fn [project newly-created?]
                                    (open-project-with-progress-dialog namespace-loader prefs project updater newly-created?)))))

(defn notify-user
  [ex-map sentry-id-promise]
  (when (.isShowing (ui/main-stage))
    (ui/run-now
      (dialogs/make-unexpected-error-dialog ex-map sentry-id-promise))))

(defn- set-sha1-revisions-from-repo! []
  ;; Use the sha1 of the HEAD commit as the editor revision.
  (when (empty? (system/defold-editor-sha1))
    (when-some [editor-sha1 (repo/detect-editor-sha1)]
      (system/set-defold-editor-sha1! editor-sha1)))

  ;; If we don't have an engine sha1 specified, use the sha1 from Bob.
  (when (empty? (system/defold-engine-sha1))
    (system/set-defold-engine-sha1! EngineVersion/sha1)))

(defn disable-imageio-cache!
  []
  ;; Disabling ImageIO cache speeds up reading images from disk significantly
  (ImageIO/setUseCache false))

(def cli-options
  ;; Path to preference file, mainly used for testing
  [["-prefs" "--preferences PATH" "Path to preferences file"]])

;; Entry point from java EditorApplication is in editor.bootloader/main, which calls this function.
(defn main [args namespace-loader]
  (when (system/defold-dev?)
    (set-sha1-revisions-from-repo!))
  (error-reporting/setup-error-reporting! {:notifier {:notify-fn notify-user}
                                           :sentry (get connection-properties :sentry)})
  (disable-imageio-cache!)

  (when-let [support-error (gl/gl-support-error)]
    (when (= (dialogs/make-gl-support-error-dialog support-error) :quit)
      (System/exit -1)))

  (let [args (Arrays/asList args)
        opts (cli/parse-opts args cli-options)
        prefs (if-let [prefs-path (get-in opts [:options :preferences])]
                (prefs/global prefs-path)
                (doto (prefs/global) prefs/migrate-global-prefs!))
        updater (updater/start!)
        analytics-url (get connection-properties :analytics-url)
        analytics-send-interval 300]
    (when (some? updater)
      (updater/delete-backup-files! updater))
    (analytics/start! analytics-url analytics-send-interval)
    (Shutdown/addShutdownAction analytics/shutdown!)
    (try
      (let [game-project-path (get-in opts [:arguments 0])]
        (if (and game-project-path
                 (.exists (io/file game-project-path)))
          (open-project-with-progress-dialog namespace-loader prefs game-project-path updater false)
          (select-project-from-welcome namespace-loader prefs updater)))
      (catch Throwable t
        (log/error :exception t)
        (stack/print-stack-trace t)
        (.flush *out*)
        ;; note - i'm not sure System/exit is a good idea here. it
        ;; means that failing to open one project causes the whole
        ;; editor to quit, maybe losing unsaved work in other open
        ;; projects.
        (System/exit -1)))))
