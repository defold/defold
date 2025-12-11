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

(ns editor.lsp.jsonrpc
  (:require [clojure.core.async :as a :refer [<! >!]]
            [clojure.data.json :as json]
            [editor.error-reporting :as error-reporting]
            [editor.lsp.async :as lsp.async]))

(set! *warn-on-reflection* true)

(defn- response? [message]
  (and (map? message)
       (contains? message :id)
       (or (contains? message :result)
           (contains? message :error))))

(defn- notification? [message]
  (and (contains? message :method)
       (not (contains? message :id))))

(defn- request? [message]
  (and (contains? message :method)
       (contains? message :id)))

(defn- respond [request-handlers message]
  (try
    {:jsonrpc "2.0"
     :id (:id message)
     :result (let [handler (or (request-handlers (:method message))
                               (throw (ex-info "Method not found" {:jsonrpc/code -32601 :method (:method message)})))]
               (handler (:params message)))}
    (catch Throwable e
      (when-not (Boolean/getBoolean "defold.tests")
        (error-reporting/report-exception! e))
      {:jsonrpc "2.0"
       :id (:id message)
       :error {:code (:jsonrpc/code (ex-data e) -32603)
               :message (or (ex-message e) "Internal Error")}})))

(defn- invalid-request-response [message]
  {:jsonrpc "2.0"
   :id (:id message)
   :error {:code -32600 :message "Invalid Request"}})

(defn- server-message-processor [message-string request-handlers base-sink]
  (fn [state]
    (a/go
      (let [message (try (json/read-str message-string :key-fn keyword) (catch Throwable _ nil))]
        (cond
          (response? message)
          (let [id (:id message)
                response-ch (get-in state [:requests id])]
            (a/put! response-ch (select-keys message [:result :error]))
            (a/close! response-ch)
            (update state :requests dissoc id))

          (notification? message)
          (do (when-let [handler (request-handlers (:method message))]
                (try
                  (handler (:params message))
                  (catch Throwable e (error-reporting/report-exception! e))))
              state)

          (request? message)
          (do (>! base-sink (json/write-str (respond request-handlers message)))
              state)

          :else
          (do (>! base-sink (json/write-str (invalid-request-response message)))
              state))))))

(defn- client-message-processor [value base-sink]
  (fn [state]
    (a/go
      (let [request (cond-> {:jsonrpc "2.0"
                             :method (:method value)}
                            (contains? value :params)
                            (assoc :params (:params value)))]
        (if-let [response-ch (:response-ch value)]
          (let [id (:next-id state)
                request (assoc request :id id)]
            (>! base-sink (json/write-str request))
            (-> state
                (assoc :next-id (inc id))
                (update :requests assoc id response-ch)))
          (do (>! base-sink (json/write-str request))
              state))))))

(defn make
  "Create JSON-RPC communication protocol as defined by LSP specification

  LSP-flavored JSON-RPC protocol is bidirectional — both client and server can
  send requests and notifications to each other. Batching is not supported.

  Args:
    request-handlers    a 1-arg fn (e.g. a map) from method name to handler fn
                        that will receive request params; used both for
                        notifications (return value is ignored) and requests
                        (return value is send back to server); shouldn't block
    base-source         input LSP base layer channel that is used to receive
                        string messages from the language server
    base-sink           output LSP base layer channel that is used to send
                        string messages to the language server, will be closed
                        when both the returned channel and the source channel
                        close

  Returns a message sink: a chan to put messages to send to the language server,
  a message is a map with the following keys:
    :method         server's JSON-RPC method name, a string, required
    :params         server's JSON-RPC method parameter map, optional
    :response-ch    a channel that might eventually receive a response and
                    then will be closed. A response is a map with either of the
                    keys:
                      :result    successful result
                      :error     an error description as per JSON-RPC spec

  See also:
    https://www.jsonrpc.org/specification"
  [request-handlers base-source base-sink]
  (let [client (a/chan 128)]
    (a/go
      (<! (lsp.async/reduce-async
            (fn [state processor]
              (processor state))
            {:next-id 0
             :requests {}}
            (a/merge [(a/map #(client-message-processor % base-sink) [client])
                      (a/map #(server-message-processor % request-handlers base-sink) [base-source])])))
      (a/close! base-sink))
    client))

(def ^:const error-code-response-timeout -30000)
(def ^:const error-code-server-not-running -30001)

(defn error
  "Create JSON-RPC error response"
  ([code message]
   (error code message nil))
  ([code message data]
   {:pre [(int? code) (string? message)]}
   {:error (cond-> {:code code
                    :message message}
                   (some? data)
                   (assoc :data data))}))

(defn notification->request [notification response-ch]
  (assoc notification :response-ch response-ch))

(defn request!
  "Send a request to a JSON-RPC message sink

  Args:
    jsonrpc       JSON-RPC communication channel returned by [[make]]
    method        server's JSON-RPC method name, a string
    params        server's JSON-RPC method parameter map, optional
    timeout-ms    response timeout

  Returns a channel that will receive a response map with either of the keys:
    :result    successful result
    :error     an error description as per JSON-RPC spec"
  ([jsonrpc method timeout-ms]
   (request! jsonrpc method nil timeout-ms))
  ([jsonrpc method params timeout-ms]
   {:pre [(string? method)
          (or (nil? params)
              (map? params))
          (pos-int? timeout-ms)]}
   (a/go
     (let [ch (a/chan 1)]
       (a/alt!
         (a/go
           (>! jsonrpc (cond-> {:method method
                                :response-ch ch}
                               (some? params)
                               (assoc :params params)))
           (<! ch))
         ([response] response)

         (a/timeout timeout-ms)
         (error error-code-response-timeout
                (str "Client response timeout: " method)
                (cond-> {:timeout-ms timeout-ms
                         :method method}
                        (some? params)
                        (assoc :params params))))))))

(defn notification
  "Create a notification that can be submitted to a JSON-RPC message sink

  Args:
    method     server's JSON-RPC method name, a string
    params     server's JSON-RPC method parameter map, optional

  Returns a value that can be submitted to jsonrpc message sink"
  ([method]
   (notification method nil))
  ([method params]
   (cond-> {:method method}
           (some? params)
           (assoc :params params))))

(defn unwrap-response
  "Given a response, returns result on success or throws exception on error

  Response is a map with either of the keys:
    :result    successful result
    :error     an error description as per JSON-RPC spec"
  [{:keys [result error]}]
  (if error
    (throw (ex-info (or (str (:message error))
                        (condp = (:code error)
                          -32700 "Parse error"
                          -32600 "Invalid Request"
                          -32601 "Method not found"
                          -32602 "Invalid params"
                          -32603 "Internal error"
                          "Unknown error"))
                    error))
    result))
