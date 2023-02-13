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
  (:require [editor.ui :as ui]
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

(defonce ^:private unhandled-exception-response
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

(defn- run-command! [ui-node command]
  (let [command-contexts (ui/node-contexts ui-node false)]
    (ui/execute-command command-contexts command {})))

(defn request-handler [ui-node request]
  (if-some [command (parse-command request)]
    (ui/run-now
      (try
        (case (run-command! ui-node command)
          ::ui/not-active command-not-active-response
          ::ui/not-enabled command-not-enabled-response
          command-accepted-response)
        (catch Exception error
          (log/error :msg "Failed to handle command request"
                     :request request
                     :exception error)
          unhandled-exception-response)))
    malformed-request-response))
