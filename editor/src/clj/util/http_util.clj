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

(ns util.http-util
  (:import [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)

(def make-status-response
  (memoize
    (fn make-status-response [code description]
      {:pre [(integer? code)
             (string? (not-empty description))]}
      {:code code
       :body (.getBytes (str code \space description \newline) StandardCharsets/UTF_8)})))

(defn make-json-response [^String json-string]
  (let [utf8-bytes (.getBytes ^String json-string StandardCharsets/UTF_8)]
    {:code 200
     :headers {"Content-Type" "application/json"
               "Content-Length" (str (count utf8-bytes))}
     :body utf8-bytes}))


(def accepted-response (make-status-response 202 "Accepted"))
(def bad-request-response (make-status-response 400 "Bad Request"))
(def forbidden-response (make-status-response 403 "Forbidden"))
(def not-found-response (make-status-response 404 "Not Found"))
(def method-not-allowed-response (make-status-response 405 "Method Not Allowed"))
(def internal-server-error-response (make-status-response 500 "Internal Server Error"))

(def only-get-allowed-response
  (assoc method-not-allowed-response
    :headers {"Allow" "GET"}))

(def only-post-allowed-response
  (assoc method-not-allowed-response
    :headers {"Allow" "POST"}))
