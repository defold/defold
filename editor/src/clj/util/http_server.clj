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

(ns util.http-server
  (:require [clojure.java.io :as io]
            [editor.error-reporting :as error-reporting]
            [service.log :as log]
            [util.http-util :as http-util])
  (:import [com.sun.net.httpserver HttpExchange HttpHandler HttpServer]
           [java.io ByteArrayOutputStream InputStream]
           [java.net InetAddress InetSocketAddress URLDecoder]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def ^:const default-handlers {"/" (fn [request]
                                     (log/info :msg (format "No handler for '%s'" (:url request)))
                                     http-util/not-found-response)})

(defn- get-request [^HttpExchange e]
  {:headers (.getRequestHeaders e)
   :method (.getRequestMethod e)
   :body (let [os (ByteArrayOutputStream.)]
           (IOUtils/copy (.getRequestBody e) os)
           (.toByteArray os))
   :url (URLDecoder/decode (.toString (.getRequestURI e)))})

(defn- send-response! [^HttpExchange e response]
  (let [code       (:code response 200)
        body       (:body response)
        headers    (.getResponseHeaders e)
        in-headers (:headers response {})
        length     (if (nil? body)
                     -1
                     (if-let [content-length (get in-headers "Content-Length")]
                       (Long/parseLong content-length)
                       0))]
    (doseq [[key value] in-headers]
      (.add headers key value))
    (.sendResponseHeaders e code length)
    (when body
      (with-open [^InputStream in (io/input-stream (if (string? body) (.getBytes ^String body "UTF-8") body))
                  out (.getResponseBody e)]
        (IOUtils/copy in out)))
    (.close e)))

(defn- setup-server!
  [^HttpServer server handlers]
  (doseq [[path handler] (merge default-handlers handlers)]
    (.createContext server path (reify HttpHandler
                                  (handle [_this http-exchange]
                                    (try
                                      (let [request (get-request http-exchange)
                                            handler-result (handler request)]
                                        (if (fn? handler-result)
                                          (handler-result #(send-response! http-exchange %))
                                          (send-response! http-exchange handler-result)))
                                      (catch Throwable error
                                        (send-response! http-exchange http-util/internal-server-error-response)
                                        (error-reporting/report-exception! error)))))))
  (.setExecutor server nil)
  server)

(defn ->server
  ([port handlers]
   (-> (HttpServer/create (InetSocketAddress. port) 0)
       (setup-server! handlers)))
  ([address port handlers]
   (let [inet-socket-address
         (cond
           (instance? String address)
           (InetSocketAddress. ^String address ^int port)

           (instance? InetAddress address)
           (InetSocketAddress. ^InetAddress address ^int port))]
     (-> (HttpServer/create inet-socket-address 0)
         (setup-server! handlers)))))

(defn local-url [^HttpServer server]
  (format "http://localhost:%d" (.getPort (.getAddress server))))

(defn url [^HttpServer server]
  (let [address (.getAddress server)]
    (format "http://%s:%d" (.getHostString address) (.getPort address))))

(defn port [^HttpServer server]
  (.getPort (.getAddress server)))

(defn start! [^HttpServer server]
  (.start server)
  (when-not (Boolean/getBoolean "defold.tests")
    (log/info :msg "Http server running"
              :local-url (local-url server)))
  server)

(defn stop! [^HttpServer server]
  (.stop server 2)
  server)

