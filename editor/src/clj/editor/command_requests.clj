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
  (:require [dynamo.graph :as g]
            [editor.disk :as disk]
            [editor.ui :as ui]
            [service.log :as log]))

(set! *warn-on-reflection* true)

(defonce ^:const url-prefix "/command/")

(defonce ^:private malformed-request-response
  {:code 400
   :body "400 Bad Request"})

(defonce ^:private command-not-active-response
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
  (try
    (keyword (subs (:url request) 9))
    (catch Exception error
      (log/error :msg "Failed to parse command request"
                 :request request
                 :exception error)
      nil)))

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
      (let [command (parse-command request)]
        (if (nil? command)
          malformed-request-response
          (let [ui-handler-ctx (resolve-ui-handler-ctx ui-node command {})]
            (case ui-handler-ctx
              ::ui/not-active command-not-active-response
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
