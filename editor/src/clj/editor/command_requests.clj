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
  (:require [clojure.data.json :as json]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.disk :as disk]
            [editor.ui :as ui]
            [service.log :as log]
            [util.http-util :as http-util]))

(set! *warn-on-reflection* true)

(defonce ^:const url-prefix "/command")

(def ^:private supported-commands
  ;; Notable exclusions:
  ;; :save-all, :quit, anything that would open a modal dialog.
  {:asset-portal
   {:ui-handler :asset-portal
    :help "Open the Asset Portal in a web browser."}

   :build
   {:ui-handler :build
    :help "Build and run the project."
    :resource-sync true}

   :build-html5
   {:ui-handler :build-html5
    :help "Build the project for HTML5 and open it in a web browser."
    :resource-sync true}

   :debugger-break
   {:ui-handler :break
    :help "Break into the debugger."}

   :debugger-continue
   {:ui-handler :continue
    :help "Resume execution in the debugger."}

   :debugger-detach
   {:ui-handler :detach-debugger
    :help "Detach the debugger from the running project."}

   :debugger-start
   {:ui-handler :start-debugger
    :help "Start the project with the debugger, or attach the debugger to the running project."
    :resource-sync true}

   :debugger-step-into
   {:ui-handler :step-into
    :help "Step into the current expression in the debugger."}

   :debugger-step-out
   {:ui-handler :step-out
    :help "Step out of the current expression in the debugger."}

   :debugger-step-over
   {:ui-handler :step-over
    :help "Step over the current expression in the debugger."}

   :debugger-stop
   {:ui-handler :stop-debugger
    :help "Stop the debugger and the running project."}

   :documentation
   {:ui-handler :documentation
    :help "Open the Defold documentation in a web browser."}

   :donate-page
   {:ui-handler :donate
    :help "Open the Donate to Defold page in a web browser."}

   :editor-logs
   {:ui-handler :show-logs
    :help "Show the directory containing the editor logs."}

   :engine-profiler
   {:ui-handler :engine-profile-show
    :help "Open the Engine Profiler in a web browser."}

   :engine-resource-profiler
   {:ui-handler :engine-resource-profile-show
    :help "Open the Engine Resource Profiler in a web browser."}

   :fetch-libraries
   {:ui-handler :fetch-libraries
    :help "Download the latest version of the project library dependencies."
    :resource-sync true}

   :hot-reload
   {:ui-handler :hot-reload
    :help "Hot-reload all modified files into the running project."
    :resource-sync true}

   :issues
   {:ui-handler :search-issues
    :help "Open the Defold Issue Tracker in a web browser."}

   :rebuild
   {:ui-handler :rebuild
    :help "Rebuild and run the project."
    :resource-sync true}

   :rebundle
   {:ui-handler :rebundle
    :help "Re-bundle the project using the previous Bundle dialog settings."
    :resource-sync true}

   :reload-extensions
   {:ui-handler :reload-extensions
    :help "Reload editor extensions."
    :resource-sync true}

   :reload-stylesheets
   {:ui-handler :reload-stylesheet
    :help "Reload editor stylesheets."}

   :report-issue
   {:ui-handler :report-issue
    :help "Open the Report Issue page in a web browser."}

   :report-suggestion
   {:ui-handler :report-suggestion
    :help "Open the Report Suggestion page in a web browser."}

   :show-build-errors
   {:ui-handler :show-build-errors
    :help "Show the Build Errors tab."}

   :show-console
   {:ui-handler :show-console
    :help "Show the Console tab."}

   :show-curve-editor
   {:ui-handler :show-curve-editor
    :help "Show the Curve Editor tab."}

   :support-forum
   {:ui-handler :support-forum
    :help "Open the Defold Support Forum in a web browser."}

   :toggle-pane-bottom
   {:ui-handler :toggle-pane-bottom
    :help "Toggle visibility of the bottom editor pane."}

   :toggle-pane-left
   {:ui-handler :toggle-pane-left
    :help "Toggle visibility of the left editor pane."}

   :toggle-pane-right
   {:ui-handler :toggle-pane-right
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
    (http-util/make-json-response help-json-string)))

(def ^:private command-accepted-response http-util/accepted-response)
(def ^:private command-not-allowed-response http-util/forbidden-response)
(def ^:private command-not-supported-response http-util/not-found-response)

(defn- parse-command [request]
  (let [url-string (:url request)
        method-string (:method request)]
    (case url-string
      ("/command" "/command/") (if (= "GET" method-string)
                                 ::api
                                 ::only-get-allowed)
      (try
        (let [command (keyword (subs url-string 9))]
          (if (= "POST" method-string)
            command
            ::only-post-allowed))
        (catch Exception error
          (log/error :msg "Failed to parse command request"
                     :request request
                     :exception error)
          ::bad-request)))))

(defn- resolve-ui-handler-ctx [ui-node ui-handler user-data]
  {:pre [(ui/node? ui-node)
         (keyword? ui-handler)
         (map? user-data)]}
  (ui/run-now
    (let [command-contexts (ui/node-contexts ui-node true)]
      (ui/resolve-handler-ctx command-contexts ui-handler user-data))))

(defn- handle-request! [request ui-node render-reload-progress!]
  (let [command (parse-command request)]
    (case command
      ::api api-response
      ::bad-request http-util/bad-request-response
      ::only-get-allowed http-util/only-get-allowed-response
      ::only-post-allowed http-util/only-post-allowed-response
      (let [command-info (some-> command supported-commands)]
        (if (nil? command-info)
          command-not-supported-response
          (let [ui-handler (:ui-handler command-info)
                ui-handler-ctx (resolve-ui-handler-ctx ui-node ui-handler {})]
            (case ui-handler-ctx
              (::ui/not-active ::ui/not-enabled) command-not-allowed-response
              (let [{:keys [changes-view workspace]} (:env (second ui-handler-ctx))
                    log-failure! (fn log-failure! [error]
                                   (log/error :msg "Failed to handle command request"
                                              :request request
                                              :exception error))]
                (assert (g/node-id? changes-view))
                (assert (g/node-id? workspace))
                (log/info :msg "Processing request"
                          :command command)
                (if-not (:resource-sync command-info)
                  (try
                    (ui/run-now
                      (ui/execute-handler-ctx ui-handler-ctx)
                      command-accepted-response)
                    (catch Exception error
                      (log-failure! error)
                      http-util/internal-server-error-response))
                  (fn request-handler-continuation [post-response!]
                    {:pre [(fn? post-response!)]}
                    (disk/async-reload!
                      render-reload-progress! workspace [] changes-view
                      (fn async-reload-continuation [success]
                        ;; This callback is executed on the ui thread.
                        (if success
                          (try
                            (ui/execute-handler-ctx ui-handler-ctx)
                            (post-response! command-accepted-response)
                            (catch Exception error
                              (log-failure! error)
                              (post-response! http-util/internal-server-error-response)))
                          (do
                            ;; Explicitly refresh the UI after the reload, since
                            ;; the ui-handler will not have done so on failure.
                            (ui/user-data! (ui/scene ui-node) ::ui/refresh-requested? true)
                            (post-response! http-util/internal-server-error-response)))))))))))))))

(defn make-request-handler [ui-node render-reload-progress!]
  {:pre [(ui/node? ui-node)
         (fn? render-reload-progress!)]}
  (fn request-handler [request]
    (handle-request! request ui-node render-reload-progress!)))
