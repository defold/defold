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

(ns util.http-client-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer [deftest is testing]]
            [util.http-client :as http]
            [util.http-server :as http-server])
  (:import [java.io BufferedReader InputStream]
           [java.net InetAddress ServerSocket SocketException]
           [java.util.concurrent ExecutionException TimeUnit TimeoutException]))

(set! *warn-on-reflection* true)

;; Optional deadlines preserve POST bodies, headers, and HTTP error responses.
(deftest request-test
  (let [request (atom nil)]
    (with-open [server (http-server/start!
                         (fn echo-request! [{:keys [method headers body]}]
                           (reset! request {:method method :content-type (get headers "content-type")})
                           (http-server/response 409 (slurp body :encoding "UTF-8")))
                         :host "127.0.0.1")]
      (doseq [timeout-ms [nil 5000]]
        (let [response @(http/request
                          (http-server/url server)
                          :method "POST"
                          :headers {"Content-Type" "application/x-www-form-urlencoded"}
                          :body "path=%2Ftmp%2Ftrace&energy=0"
                          :as :string
                          :timeout-ms timeout-ms)]
          (is (= {:method "POST" :content-type "application/x-www-form-urlencoded"} @request))
          (is (= 409 (:status response)))
          (is (= "path=%2Ftmp%2Ftrace&energy=0" (:body response))))))))

(defn- read-request-headers! [^BufferedReader reader]
  (loop []
    (when-let [line (.readLine reader)]
      (when-not (= "" line)
        (recur)))))

(defmacro ^:private with-stalled-response
  "Bind response to a stalled HTTP request and verify its connection is closed."
  [send-headers & body]
  `(with-open [server# (doto (ServerSocket. 0 1 (InetAddress/getByName "127.0.0.1"))
                         (.setSoTimeout 10000))]
     (let [send-headers# ~send-headers
           received# (promise)
           disconnected# (future
                           (with-open [socket# (.accept server#)
                                       reader# (io/reader (.getInputStream socket#) :encoding "UTF-8")]
                             (.setSoTimeout socket# 10000)
                             (read-request-headers! reader#)
                             (when send-headers#
                               (let [out# (.getOutputStream socket#)]
                                 (.write out# (.getBytes "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n" "UTF-8"))
                                 (.flush out#)))
                             (deliver received# true)
                             ;; Observe closure of the real connection.
                             (try
                               (.read ^BufferedReader reader#)
                               (catch SocketException _# -1))))
           ~'response
           (http/request
             (str "http://127.0.0.1:" (.getLocalPort server#))
             :as :string
             :timeout-ms 2000)]
       (try
         (is (= true (deref received# 10000 ::not-received)))
         ~@body
         (is (= -1 (deref disconnected# 10000 ::not-disconnected)))
         (finally
           (.cancel ~'response true)
           (future-cancel disconnected#))))))

;; Deadlines cover both missing headers and stalled bodies, and cancel the
;; network exchange instead of only reporting a timeout to the caller.
(deftest request-timeout-test
  (doseq [send-headers [false true]]
    (testing (if send-headers "stalled response body" "missing response headers")
      (with-stalled-response send-headers
        (try
          (.get response 10 TimeUnit/SECONDS)
          (is false "The stalled request should time out")
          (catch ExecutionException error
            (is (instance? TimeoutException (ex-cause error)))))))))

;; Adding a deadline preserves caller cancellation of the HTTP exchange.
(deftest request-cancellation-test
  (with-stalled-response true
    (is (.cancel response true))
    (is (.isCancelled response))))

;; Streaming requests hand the stream to the caller before the body arrives;
;; the deadline wrapper must leave that stream open for the caller to consume.
(deftest streaming-response-test
  (with-open [server (doto (ServerSocket. 0 1 (InetAddress/getByName "127.0.0.1"))
                       (.setSoTimeout 10000))]
    (let [send-body (promise)
          sent (future
                 (with-open [socket (.accept server)
                             reader (io/reader (.getInputStream socket) :encoding "UTF-8")]
                   (.setSoTimeout socket 10000)
                   (read-request-headers! reader)
                   (let [out (.getOutputStream socket)]
                     (.write out (.getBytes "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n" "UTF-8"))
                     (.flush out)
                     (when (= true (deref send-body 10000 ::not-released))
                       (.write out (.getBytes "hello" "UTF-8"))
                       (.flush out)))))]
      (try
        (let [response (.get
                         (http/request
                           (str "http://127.0.0.1:" (.getLocalPort server))
                           :as :input-stream
                           :timeout-ms 2000)
                         10
                         TimeUnit/SECONDS)]
          (is (= 200 (:status response)))
          (with-open [^InputStream body (:body response)]
            (deliver send-body true)
            (is (= "hello" (slurp body :encoding "UTF-8")))))
        (is (nil? (deref sent 10000 ::not-sent)))
        (finally
          (deliver send-body false)
          (future-cancel sent))))))
