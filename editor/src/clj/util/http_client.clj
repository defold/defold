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

(ns util.http-client
  "Clojure HTTP client implementation using modern java.net.http stack"
  (:require [util.coll :as coll])
  (:import [java.io InputStream]
           [java.net URI]
           [java.net.http HttpClient HttpClient$Redirect HttpRequest HttpRequest$BodyPublishers HttpRequest$Builder HttpResponse HttpResponse$BodyHandlers]
           [java.util List]
           [java.util.concurrent CompletableFuture TimeUnit TimeoutException]))

(set! *warn-on-reflection* true)

(def ^:private ^HttpClient client
  (-> (HttpClient/newBuilder)
      (.followRedirects HttpClient$Redirect/NORMAL)
      (.build)))

(defn request
  "Send HTTP request

  Optional kv-args:
    :method        request method string, defaults to GET
    :headers       request headers map, string->string
    :body          request body, either string, InputStream or a byte array
    :as            response body: :string, :input-stream or :byte-array
    :timeout-ms    future timeout in milliseconds, defaults to no timeout

  Returns a CompletableFuture that will receive a map with the following keys:
    :status     HTTP status code, e.g. 200
    :headers    response headers map, string->(string|string[])
    :body       the key is present only if :as was provided; either string,
                InputStream or byte array"
  ^CompletableFuture [url & {:keys [method body headers as timeout-ms]
                             :or {method "GET"}}]
  {:pre [(string? url)
         (or (nil? timeout-ms) (pos-int? timeout-ms))]}
  (let [response-future
        (-> client
            (.sendAsync
              (-> (HttpRequest/newBuilder (URI. url))
                  (as-> $ ^HttpRequest$Builder (reduce-kv HttpRequest$Builder/.header $ headers))
                  (.method method (cond
                                    (nil? body) (HttpRequest$BodyPublishers/noBody)
                                    (string? body) (HttpRequest$BodyPublishers/ofString body)
                                    (instance? InputStream body) (HttpRequest$BodyPublishers/ofInputStream (fn [] body))
                                    (instance? byte/1 body) (HttpRequest$BodyPublishers/ofByteArray body)
                                    :else (throw (IllegalArgumentException. "body should either be nil, string, InputStream or byte array"))))
                  (.build))
              (case as
                nil (HttpResponse$BodyHandlers/discarding)
                :string (HttpResponse$BodyHandlers/ofString)
                :input-stream (HttpResponse$BodyHandlers/ofInputStream)
                :byte-array (HttpResponse$BodyHandlers/ofByteArray)))
            (.thenApply
              (fn [^HttpResponse response]
                (cond-> {:status (.statusCode response)
                         :headers (coll/into-> (.map (.headers response)) {}
                                    (map (fn [[k v]]
                                           [k (if (< 1 (count v)) (vec v) (.get ^List v 0))])))}
                  (.body response)
                  (assoc :body (.body response))))))]
    (if-not timeout-ms
      response-future
      ;; Timing out the HTTP future itself would complete it without cancelling
      ;; the exchange. A separate result lets cancellation reach the client.
      (let [result (CompletableFuture.)]
        (.whenComplete result
                       (fn cancel-request! [_ error]
                         (when (or (.isCancelled result) (instance? TimeoutException error))
                           (.cancel response-future true))))
        (.whenComplete response-future
                       (fn complete-request! [response error]
                         (if error
                           (.completeExceptionally result error)
                           (when-not (.complete result response)
                             ;; A streaming response can arrive concurrently
                             ;; with cancellation. Close it if never handed off.
                             (let [body (:body response)]
                               (when (instance? InputStream body)
                                 (.close ^InputStream body)))))))
        (.orTimeout result timeout-ms TimeUnit/MILLISECONDS)))))
