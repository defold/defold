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
  (:import [java.io InputStream]
           [java.net URI]
           [java.net.http HttpClient HttpClient$Redirect HttpRequest HttpRequest$BodyPublishers HttpRequest$Builder HttpResponse HttpResponse$BodyHandlers]
           [java.util List]))

(set! *warn-on-reflection* true)

(def ^:private ^HttpClient client
  (-> (HttpClient/newBuilder)
      (.followRedirects HttpClient$Redirect/NORMAL)
      (.build)))

(defn request
  "Send HTTP request

  Optional kv-args:
    :method     request method string, defaults to GET
    :headers    request headers map, string->string
    :body       request body, either string, InputStream or a byte array
    :as         response body, either :string, :input-stream or :byte-array

  Returns a CompletableFuture that will receive a map with the following keys:
    :status     HTTP status code, e.g. 200
    :headers    response headers map, string->(string|string[])
    :body       the key is present only if :as was provided; either string,
                InputStream or byte array"
  [url & {:keys [method body headers as]
          :or {method "GET"}}]
  {:pre [(string? url)]}
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
                   :headers (into {}
                                  (map (fn [[k v]]
                                         [k (if (< 1 (count v)) (vec v) (.get ^List v 0))]))
                                  (.map (.headers response)))}
                  (.body response)
                  (assoc :body (.body response)))))))
