;; Copyright 2020-2026 The Defold Foundation
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
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-errors-view :as build-errors-view]
            [editor.disk :as disk]
            [editor.future :as future]
            [editor.library :as library]
            [editor.lsp.server :as lsp.server]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [com.dynamo.bob.util Library$Result]))

(set! *warn-on-reflection* true)

(defn- run-request-user-data [request]
  (case (coll/some (fn [query-part]
                     (let [[name value] (string/split query-part #"=" 2)]
                       (when (= "focus" name)
                         (or value ""))))
                   (string/split (:query request "") #"&"))
    nil {}
    "true" {:focus true}
    "false" {:focus false}
    (throw (http-server/error (http-server/response 400 "Invalid focus value; expected true or false\n")))))

(defn- build-response [{:keys [error warning]} localization-state]
  (let [issues (coll/into-> [error warning] []
                 (filter some?)
                 (mapcat #(:children (build-errors-view/build-resource-tree %)))
                 (mapcat :children)
                 (map (fn [{:keys [message severity parent cursor-range]}]
                        (let [maybe-resource (:resource parent)]
                          (cond-> {:message (localization-state message)
                                   :severity (case severity
                                               :fatal :error
                                               :info :information
                                               severity)}
                            maybe-resource (assoc :resource (resource/proj-path maybe-resource))
                            cursor-range (assoc :range (lsp.server/editor-cursor-range->lsp-range cursor-range)))))))]
    (http-server/json-response {:success (not error) :issues issues} (if error 422 200))))

(defn- fetch-libraries-response [[lib-results reload-succeeded] localization-state]
  (let [success (and reload-succeeded (coll/not-any? Library$Result/.problem lib-results))]
    (http-server/json-response
      {:success success
       :libraries (coll/into-> lib-results []
                    (map (fn [^Library$Result result]
                           (let [problem (.problem result)]
                             (cond-> {:uri (str (.uri result))
                                      :success (not problem)}
                               problem
                               (assoc :message (localization-state (library/result-message result))))))))}
      (cond
        (not reload-succeeded) 500
        success 200
        :else 422))))

(defn- resolve-command-handler [ui-node command user-data]
  {:pre [(ui/node? ui-node)
         (keyword? command)
         (map? user-data)]}
  @(fx/on-fx-thread
     (g/let-ec [command-contexts (ui/node-contexts ui-node true evaluation-context)]
       (ui/resolve-handler-ctx command-contexts command user-data))))

(defn- resolve-command-handler! [ui-node command user-data]
  (let [handler (resolve-command-handler ui-node command user-data)]
    (case handler
      (::ui/not-active ::ui/not-enabled) (throw (http-server/error http-server/forbidden))
      handler)))

(defn- execute-handler! [handler]
  (future/unwrap @(fx/on-fx-thread (ui/execute-handler-ctx handler))))

(defn- execute-command! [ui-node command user-data]
  (execute-handler! (resolve-command-handler! ui-node command user-data)))

(defn- resource-sync! [ui-node render-reload-progress! handler]
  (let [{:keys [changes-view workspace]} (:env (second handler))
        result (promise)]
    (assert (g/node-id? changes-view))
    (assert (g/node-id? workspace))
    (disk/async-reload!
      render-reload-progress! workspace [] changes-view
      (fn async-reload-continuation [success]
        ;; This callback is executed on the ui thread.
        (when-not success
          (ui/user-data! (ui/scene ui-node) ::ui/refresh-requested? true))
        (deliver result success)))
    (when-not @result
      (throw (http-server/error http-server/internal-server-error)))))

(defn router [ui-node localization render-reload-progress!]
  {"/command/asset-portal"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-asset-portal {})
                 http-server/ok))
             {:openapi {:summary "Open the Asset Portal in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   ;; Deprecated compatibility alias. Remove after 2027-07-15.
   "/command/build"
   {"POST" (bound-fn [_request]
             (future/io
               (let [handler (resolve-command-handler! ui-node :project.build {})]
                 (resource-sync! ui-node render-reload-progress! handler)
                 (build-response (execute-handler! handler) @localization))))}

   "/command/build-html5"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.build-html5 {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Build the project for HTML5 and open it in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/clean-build"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.clean-build {:skip-confirmation true})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Clears build caches and rebuilds. Use only if builds fail oddly or miss changes."
                        :responses {"200" {:description "OK"}}}})}

   "/command/compile"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.compile {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Compile the project without running it."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-break"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.break {})
                 http-server/accepted))
             {:openapi {:summary "Break into the debugger."
                        :responses {"202" {:description "Accepted"}}}})}

   "/command/debugger-continue"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.continue {})
                 http-server/ok))
             {:openapi {:summary "Resume execution in the debugger."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-detach"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.detach {})
                 http-server/ok))
             {:openapi {:summary "Detach the debugger from the running project."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-start"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :debugger.start {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Start the project with the debugger, or attach the debugger to the running project."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-step-into"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.step-into {})
                 http-server/ok))
             {:openapi {:summary "Step into the current expression in the debugger."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-step-out"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.step-out {})
                 http-server/ok))
             {:openapi {:summary "Step out of the current expression in the debugger."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-step-over"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.step-over {})
                 http-server/ok))
             {:openapi {:summary "Step over the current expression in the debugger."
                        :responses {"200" {:description "OK"}}}})}

   "/command/debugger-stop"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :debugger.stop {})
                 http-server/ok))
             {:openapi {:summary "Stop the debugger and the running project."
                        :responses {"200" {:description "OK"}}}})}

   "/command/documentation"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-documentation {})
                 http-server/ok))
             {:openapi {:summary "Open the Defold documentation in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/donate-page"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-donations {})
                 http-server/ok))
             {:openapi {:summary "Open the Donate to Defold page in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/editor-logs"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-logs {})
                 http-server/ok))
             {:openapi {:summary "Show the directory containing the editor logs."
                        :responses {"200" {:description "OK"}}}})}

   "/command/engine-profiler"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :run.open-profiler {})
                 http-server/ok))
             {:openapi {:summary "Open the Engine Profiler in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/engine-resource-profiler"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :run.open-resource-profiler {})
                 http-server/ok))
             {:openapi {:summary "Open the Engine Resource Profiler in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/fetch-libraries"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.fetch-libraries {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (fetch-libraries-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Download the latest version of the project library dependencies."
                        :responses {"200" {:description "OK"}}}})}

   "/command/hot-reload"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :run.hot-reload {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Hot-reload all modified files into the running project."
                        :responses {"200" {:description "OK"}}}})}

   "/command/issues"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-issues {})
                 http-server/ok))
             {:openapi {:summary "Open the Defold Issue Tracker in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/rebundle"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.rebundle {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (execute-handler! handler)
                   http-server/ok)))
             {:openapi {:summary "Re-bundle the project using the previous Bundle dialog settings."
                        :responses {"200" {:description "OK"}}}})}

   "/command/reload-extensions"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.reload-editor-scripts {})]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (execute-handler! handler)
                   http-server/ok)))
             {:openapi {:summary "Reload editor extensions."
                        :responses {"200" {:description "OK"}}}})}

   "/command/reload-stylesheets"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :dev.reload-css {})
                 http-server/ok))
             {:openapi {:summary "Reload editor stylesheets."
                        :responses {"200" {:description "OK"}}}})}

   "/command/report-issue"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.report-issue {})
                 http-server/ok))
             {:openapi {:summary "Open the Report Issue page in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/report-suggestion"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.report-suggestion {})
                 http-server/ok))
             {:openapi {:summary "Open the Report Suggestion page in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/run"
   {"POST" (with-meta
             (bound-fn [request]
               (future/io
                 (let [handler (resolve-command-handler! ui-node :project.build (run-request-user-data request))]
                   (resource-sync! ui-node render-reload-progress! handler)
                   (build-response (execute-handler! handler) @localization))))
             {:openapi {:summary "Compile and run the project."
                        :parameters [{:name "focus"
                                      :in "query"
                                      :description "Whether the launched game takes focus."
                                      :schema {:type "boolean"
                                               :default true}}]
                        :responses {"200" {:description "OK"}}}})}

   "/command/show-build-errors"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.show-build-errors {})
                 http-server/ok))
             {:openapi {:summary "Show the Build Errors tab."
                        :responses {"200" {:description "OK"}}}})}

   "/command/show-console"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.show-console {})
                 http-server/ok))
             {:openapi {:summary "Show the Console tab."
                        :responses {"200" {:description "OK"}}}})}

   "/command/show-curve-editor"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.show-curve-editor {})
                 http-server/ok))
             {:openapi {:summary "Show the Curve Editor tab."
                        :responses {"200" {:description "OK"}}}})}

   "/command/support-forum"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :help.open-forum {})
                 http-server/ok))
             {:openapi {:summary "Open the Defold Support Forum in a web browser."
                        :responses {"200" {:description "OK"}}}})}

   "/command/toggle-pane-bottom"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.toggle-bottom-pane {})
                 http-server/ok))
             {:openapi {:summary "Toggle visibility of the bottom editor pane."
                        :responses {"200" {:description "OK"}}}})}

   "/command/toggle-pane-left"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.toggle-left-pane {})
                 http-server/ok))
             {:openapi {:summary "Toggle visibility of the left editor pane."
                        :responses {"200" {:description "OK"}}}})}

   "/command/toggle-pane-right"
   {"POST" (with-meta
             (bound-fn [_request]
               (future/io
                 (execute-command! ui-node :window.toggle-right-pane {})
                 http-server/ok))
             {:openapi {:summary "Toggle visibility of the right editor pane."
                        :responses {"200" {:description "OK"}}}})}})

(comment

  (-> @(util.http-client/request
         (str "http://localhost:"
              (slurp (str (g/node-value (dev/workspace) :root) "/.internal/editor.port"))
              "/command/run")
         :method "POST"
         :as :string)
      :body
      (clojure.data.json/read-str :key-fn keyword))

  #__)
