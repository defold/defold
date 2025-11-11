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

(ns editor.command-requests
  (:require [cljfx.api :as fx]
            [clojure.data.json :as json]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.disk :as disk]
            [editor.future :as future]
            [editor.ui :as ui]
            [service.log :as log]
            [util.http-server :as http-server]))

(set! *warn-on-reflection* true)

(defonce ^:const url-prefix "/command")

(def ^:private supported-commands
  ;; Notable exclusions:
  ;; :save-all, :quit, anything that would open a modal dialog.
  {:asset-portal
   {:ui-handler :help.open-asset-portal
    :help "Open the Asset Portal in a web browser."}

   :build
   {:ui-handler :project.build
    :help "Build and run the project."
    :resource-sync true}

   :build-html5
   {:ui-handler :project.build-html5
    :help "Build the project for HTML5 and open it in a web browser."
    :resource-sync true}

   :debugger-break
   {:ui-handler :debugger.break
    :help "Break into the debugger."}

   :debugger-continue
   {:ui-handler :debugger.continue
    :help "Resume execution in the debugger."}

   :debugger-detach
   {:ui-handler :debugger.detach
    :help "Detach the debugger from the running project."}

   :debugger-start
   {:ui-handler :debugger.start
    :help "Start the project with the debugger, or attach the debugger to the running project."
    :resource-sync true}

   :debugger-step-into
   {:ui-handler :debugger.step-into
    :help "Step into the current expression in the debugger."}

   :debugger-step-out
   {:ui-handler :debugger.step-out
    :help "Step out of the current expression in the debugger."}

   :debugger-step-over
   {:ui-handler :debugger.step-over
    :help "Step over the current expression in the debugger."}

   :debugger-stop
   {:ui-handler :debugger.stop
    :help "Stop the debugger and the running project."}

   :documentation
   {:ui-handler :help.open-documentation
    :help "Open the Defold documentation in a web browser."}

   :donate-page
   {:ui-handler :help.open-donations
    :help "Open the Donate to Defold page in a web browser."}

   :editor-logs
   {:ui-handler :help.open-logs
    :help "Show the directory containing the editor logs."}

   :engine-profiler
   {:ui-handler :run.open-profiler
    :help "Open the Engine Profiler in a web browser."}

   :engine-resource-profiler
   {:ui-handler :run.open-resource-profiler
    :help "Open the Engine Resource Profiler in a web browser."}

   :fetch-libraries
   {:ui-handler :project.fetch-libraries
    :help "Download the latest version of the project library dependencies."
    :resource-sync true}

   :hot-reload
   {:ui-handler :run.hot-reload
    :help "Hot-reload all modified files into the running project."
    :resource-sync true}

   :issues
   {:ui-handler :help.open-issues
    :help "Open the Defold Issue Tracker in a web browser."}

   :rebuild
   {:ui-handler :project.clean-build
    :help "Rebuild and run the project."
    :resource-sync true}

   :rebundle
   {:ui-handler :project.rebundle
    :help "Re-bundle the project using the previous Bundle dialog settings."
    :resource-sync true}

   :reload-extensions
   {:ui-handler :project.reload-editor-scripts
    :help "Reload editor extensions."
    :resource-sync true}

   :reload-stylesheets
   {:ui-handler :dev.reload-css
    :help "Reload editor stylesheets."}

   :report-issue
   {:ui-handler :help.report-issue
    :help "Open the Report Issue page in a web browser."}

   :report-suggestion
   {:ui-handler :help.report-suggestion
    :help "Open the Report Suggestion page in a web browser."}

   :show-build-errors
   {:ui-handler :window.show-build-errors
    :help "Show the Build Errors tab."}

   :show-console
   {:ui-handler :window.show-console
    :help "Show the Console tab."}

   :show-curve-editor
   {:ui-handler :window.show-curve-editor
    :help "Show the Curve Editor tab."}

   :support-forum
   {:ui-handler :help.open-forum
    :help "Open the Defold Support Forum in a web browser."}

   :toggle-pane-bottom
   {:ui-handler :window.toggle-bottom-pane
    :help "Toggle visibility of the bottom editor pane."}

   :toggle-pane-left
   {:ui-handler :window.toggle-left-pane
    :help "Toggle visibility of the left editor pane."}

   :toggle-pane-right
   {:ui-handler :window.toggle-right-pane
    :help "Toggle visibility of the right editor pane."}})

(def ^:private api-response
  (let [longest-command-length
        (reduce (fn [result [command]]
                  (max result (count (name command))))
                Long/MIN_VALUE
                supported-commands)

        command-format-string
        (str "  %-" (+ longest-command-length 2) "s : %s")

        help-json-string
        (str "{\n"
             (->> supported-commands
                  (sort-by key)
                  (map (fn [[command {:keys [help]}]]
                         (format command-format-string
                                 (json/write-str (name command))
                                 (json/write-str help))))
                  (string/join ",\n"))
             "\n}\n")]
    (http-server/response 200 {"content-type" (http-server/ext->content-type "json")} help-json-string)))

(defn- resolve-ui-handler-ctx [ui-node ui-handler user-data]
  {:pre [(ui/node? ui-node)
         (keyword? ui-handler)
         (map? user-data)]}
  @(fx/on-fx-thread
     (let [command-contexts (ui/node-contexts ui-node true)]
       (ui/resolve-handler-ctx command-contexts ui-handler user-data))))

(defn router [ui-node render-reload-progress!]
  {"/command" {"GET" (constantly api-response)}
   "/command/{command}"
   {"POST" (bound-fn [request]
             (let [command (-> request :path-params :command keyword)]
               (if-let [{:keys [ui-handler resource-sync]} (supported-commands command)]
                 (let [ui-handler-ctx (resolve-ui-handler-ctx ui-node ui-handler {})]
                   (case ui-handler-ctx
                     (::ui/not-active ::ui/not-enabled) http-server/forbidden
                     (let [{:keys [changes-view workspace]} (:env (second ui-handler-ctx))
                           result-future (future/make)
                           execute-command!
                           (bound-fn execute-command! []
                             (try
                               (ui/execute-handler-ctx ui-handler-ctx)
                               (future/complete! result-future http-server/accepted)
                               (catch Exception error
                                 (log/error :msg "Failed to handle command request"
                                            :request request
                                            :exception error)
                                 (future/complete! result-future http-server/internal-server-error))))]
                       (assert (g/node-id? changes-view))
                       (assert (g/node-id? workspace))
                       (when-not (Boolean/getBoolean "defold.tests")
                         (log/info :msg "Processing request" :command command))
                       (if-not resource-sync
                         (ui/run-later (execute-command!))
                         (disk/async-reload!
                           render-reload-progress! workspace [] changes-view
                           (fn async-reload-continuation [success]
                             ;; This callback is executed on the ui thread.
                             (if success
                               (execute-command!)
                               (do
                                 ;; Explicitly refresh the UI after the reload, since
                                 ;; the ui-handler will not have done so on failure.
                                 (ui/user-data! (ui/scene ui-node) ::ui/refresh-requested? true)
                                 (future/complete! result-future http-server/internal-server-error))))))
                       result-future)))
                 http-server/not-found)))}})
