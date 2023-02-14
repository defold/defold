;; Copyright 2020-2023 The Defold Foundation
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
            [service.log :as log])
  (:import [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)

(defonce ^:const url-prefix "/command")

(defonce ^:private supported-commands
  ;; Notable exclusions:
  ;; :save-all, :quit, anything that would open a modal dialog.
  {:asset-portal
   {:ui-handler :asset-portal
    :help "Open the Asset Portal in a web browser."}

   :async-reload
   {:ui-handler :async-reload
    :help "Reload all modified files from disk."}

   :build
   {:ui-handler :build
    :help "Build and run the project."}

   :build-html5
   {:ui-handler :build-html5
    :help "Build the project for HTML5 and open it in a web browser."}

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
    :help "Start the project with the debugger attached."}

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

   :fetch-libraries
   {:ui-handler :fetch-libraries
    :help "Download the latest version of the project library dependencies."}

   :hot-reload
   {:ui-handler :hot-reload
    :help "Hot-reload all modified files into the running project."}

   :rebuild
   {:ui-handler :rebuild
    :help "Rebuild and run the project."}

   :rebundle
   {:ui-handler :rebundle
    :help "Re-bundle the project using the previous Bundle dialog settings."}

   :reload-extensions
   {:ui-handler :reload-extensions
    :help "Reload editor extensions."}

   :reload-stylesheets
   {:ui-handler :reload-stylesheet
    :help "Reload editor stylesheets."}

   :report-issue
   {:ui-handler :report-issue
    :help "Open the Report Issue page in a web browser."}

   :report-suggestion
   {:ui-handler :report-suggestion
    :help "Open the Report Suggestion page in a web browser."}

   :issues
   {:ui-handler :search-issues
    :help "Open the Defold Issue Tracker in a web browser."}

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

(defonce ^:private help-response
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
                                 (str \" (name command) \")
                                 (json/write-str help))))
                  (string/join ",\n"))
             "\n}\n")

        utf8-bytes
        (.getBytes help-json-string StandardCharsets/UTF_8)]
    {:code 200
     :headers {"Content-Type" "text/plain;charset=UTF-8"
               "Content-Length" (str (count utf8-bytes))}
     :body utf8-bytes}))

(defonce ^:private malformed-request-response
  {:code 400
   :body "400 Bad Request"})

(defonce ^:private command-not-supported-response
  {:code 404
   :body "404 Not Found"})

(defonce ^:private command-not-enabled-response
  {:code 405
   :body "405 Method Not Allowed"})

(defonce ^:private internal-server-error-response
  {:code 500
   :body "500 Internal Server Error"})

(defonce ^:private command-accepted-response
  {:code 202
   :body "202 Accepted"})

(defn- parse-command [request]
  (let [url-string (:url request)]
    (case url-string
      ("/command" "/command/") ::api
      (try
        (keyword (subs url-string 9))
        (catch Exception error
          (log/error :msg "Failed to parse command request"
                     :request request
                     :exception error)
          nil)))))

(defn- resolve-ui-handler-ctx [ui-node command user-data]
  {:pre [(ui/node? ui-node)
         (keyword? command)
         (map? user-data)]}
  (ui/run-now
    (let [command-contexts (ui/node-contexts ui-node true)]
      (ui/resolve-handler-ctx command-contexts command user-data))))

(defn make-request-handler [ui-node render-reload-progress!]
  {:pre [(ui/node? ui-node)
         (fn? render-reload-progress!)]}
  (fn request-handler [request]
    (when (= "GET" (:method request))
      (let [command (parse-command request)
            ui-handler (some-> command supported-commands :ui-handler)]
        (cond
          (= ::api command)
          help-response

          (nil? command)
          malformed-request-response

          (nil? ui-handler)
          command-not-supported-response

          :else
          (let [ui-handler-ctx (resolve-ui-handler-ctx ui-node ui-handler {})]
            (case ui-handler-ctx
              ::ui/not-active command-not-supported-response
              ::ui/not-enabled command-not-enabled-response
              (let [{:keys [changes-view workspace]} (:env (second ui-handler-ctx))]
                (assert (g/node-id? changes-view))
                (assert (g/node-id? workspace))
                (log/info :msg "Processing request"
                          :command command)
                (fn request-handler-continuation [post-response!]
                  {:pre [(fn? post-response!)]}
                  (disk/async-reload!
                    render-reload-progress! workspace [] changes-view
                    (fn async-reload-continuation [success]
                      (if success
                        (try
                          (ui/execute-handler-ctx ui-handler-ctx)
                          (post-response! command-accepted-response)
                          (catch Exception error
                            (log/error :msg "Failed to handle command request"
                                       :request request
                                       :exception error)
                            (post-response! internal-server-error-response)))
                        (do
                          (ui/user-data! (ui/scene ui-node) ::ui/refresh-requested? true)
                          (post-response! internal-server-error-response))))))))))))))
