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

(ns util.http-server
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.util :as util]
            [reitit.core :as reitit]
            [service.log :as log]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.http-server.types :as http-server.types]
            [util.path :as path])
  (:import [com.sun.net.httpserver Headers HttpHandler HttpServer]
           [java.io Closeable File IOException]
           [java.net InetSocketAddress URI URL]
           [java.nio.charset StandardCharsets]
           [java.nio.file Path]
           [java.util List]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.io.output ByteArrayOutputStream]))

(set! *warn-on-reflection* true)

(defn- path-content-type [s]
  (http-server.types/ext->content-type (FilenameUtils/getExtension s)))

(extend-protocol http-server.types/->Data
  String (->data [s] (.getBytes s StandardCharsets/UTF_8))
  File (->data [f] (.toPath f))
  Object (->data [x] x)
  nil (->data [x] x))

(extend-protocol http-server.types/ContentType
  String (content-type [_] "text/plain; charset=utf-8")
  byte/1 (content-type [_] "application/octet-stream")
  Path (content-type [path] (path-content-type (str path)))
  URL (content-type [url]
        (case (.getProtocol url)
          ("file" "jar") (path-content-type (.getPath url))
          nil))
  Object (content-type [_])
  nil (content-type [_]))

(extend-protocol http-server.types/ContentLength
  byte/1 (content-length [bytes] (count bytes))
  Object (content-length [_])
  nil (content-length [_]))

(defmacro ^:private provide-header [headers key value]
  `(let [k# ~key
         h# ~headers]
     (if (contains? h# k#)
       h#
       (if-let [v# ~value]
         (assoc h# k# (str v#))
         h#))))

(defn response
  "Create HTTP response

  Args:
    status     HTTP status code, e.g. 200
    headers    HTTP headers map, string->string, lower-case keys
    body       response body, either nil, string (text content) or anything that
               the server knows how to write after ->data/->connection
               normalization, typically an io/IOFactory value such as
               InputStream, File, or Path"
  ([]
   (response 200 nil nil))
  ([status]
   (response status nil nil))
  ([status body]
   (response status nil body))
  ([status headers body]
   {:pre [(integer? status) (or (nil? headers) (map? headers))]}
   (let [;; Convert body to reduce unnecessary transformations when sending the
         ;; response in case this response is going to be reused
         data (http-server.types/->data body)
         data-is-body (identical? body data)
         ;; Infer length if possible to do immediately
         length (or (some-> (get headers "content-length") parse-long)
                    (http-server.types/content-length body)
                    (when-not data-is-body (http-server.types/content-length data)))
         headers (-> headers
                     (provide-header "content-type" (or (http-server.types/content-type body)
                                                      (when-not data-is-body (http-server.types/content-type data))))
                     (provide-header "content-length" length))]
     (cond-> {:status status}
       headers (assoc :headers headers)
       (and data (or (not length) (pos? length))) (assoc :body data)))))

(defn json-response
  ([json-value]
   (json-response json-value 200 nil))
  ([json-value status]
   (json-response json-value status nil))
  ([json-value status headers]
   (let [baos (ByteArrayOutputStream.)]
     (with-open [writer (io/writer baos)]
       (json/write json-value writer))
     (response status (provide-header headers "content-type" "application/json") (.toByteArray baos)))))

(defn- make-status-response [status body]
  (response status (str status \space body \newline)))

(def ok (make-status-response 200 "OK"))
(def accepted (make-status-response 202 "Accepted"))
(def forbidden (make-status-response 403 "Forbidden"))
(def not-found (make-status-response 404 "Not Found"))
(def method-not-allowed (make-status-response 405 "Method Not Allowed"))
(def internal-server-error (make-status-response 500 "Internal Server Error"))
(defn redirect [location] (response 302 {"location" location} nil))

(defonce/type ServerWithHandler [^HttpServer server handler]
  Closeable
  (close [_] (.stop server 0)))

(defn port [^ServerWithHandler server]
  (.getPort (.getAddress ^HttpServer (.-server server))))

(defn local-url [server]
  (format "http://localhost:%d" (port server)))

(defn url [^ServerWithHandler server]
  (let [address (.getAddress ^HttpServer (.-server server))]
    (format "http://%s:%d" (.getHostString address) (.getPort address))))

(extend-protocol http-server.types/ConnectionContentLength
  Path (connection-content-length [path] (path/byte-size path))
  Object (connection-content-length [_])
  nil (connection-content-length [_]))

(extend-protocol http-server.types/ConnectionContentType
  Object (connection-content-type [_])
  nil (connection-content-type [_]))

(extend-protocol http-server.types/->Connection
  URL (->connection [url]
        (let [connection (.openConnection url)]
          (reify
            http-server.types/ConnectionContentLength
            (connection-content-length [_]
              (let [len (.getContentLengthLong connection)]
                (when-not (= -1 len) len)))

            http-server.types/ConnectionContentType
            (connection-content-type [_] (.getContentType connection))

            io/IOFactory
            (make-input-stream [_ opts] (io/make-input-stream (.getInputStream connection) opts))
            (make-output-stream [_ opts] (io/make-output-stream (.getOutputStream connection) opts))
            (make-reader [_ opts] (io/make-reader (.getInputStream connection) opts))
            (make-writer [_ opts] (io/make-writer (.getOutputStream connection) opts))

            Closeable
            (close [_] (.close (.getInputStream connection))))))
  URI (->connection [uri] (http-server.types/->connection (.toURL uri)))
  Object (->connection [x] x)
  nil (->connection [x] x))

(extend-protocol http-server.types/ConnectionWrite
  Object (connection-write! [connection output-stream]
           (with-open [input-stream (io/input-stream connection)]
             (io/copy input-stream output-stream)))
  nil (connection-write! [_ _]))

(defn error [response]
  (ex-info "HTTP server error" {::response response}))

(defn start!
  "Start a generic HTTP server

  Args:
    handler    request handler function, receives request and returns either a
               response or a CompletableFuture that will contain the response
               Request is a map with the following keys:
                 :method     HTTP request method string, e.g. \"GET\"
                 :path       URI path part string, e.g. \"/users/1\"
                 :query      optional query string, e.g. \"q=1\"
                 :headers    a map of headers, string->(string|string[]); header
                             names are lower-case
                 :body       request body, InputStream
               Response is a map with the following keys:
                 :status     optional HTTP response status, integer, default 200
                 :headers    optional response headers map, string->string;
                             header names should be lower-case
                 :body       optional response body, could be nil, string
                             content, or anything the server knows how to write
                             after ->data/->connection normalization, typically
                             an io/IOFactory value such as InputStream, File,
                             or Path. If the opened connection is Closeable, it
                             will be closed even if it's not written (which
                             might happen when responding to HEAD requests)
               Use [[response]] fn to produce the response

  Kv-args (all optional):
    :host    HTTP server host, string
    :port    HTTP server port, integer. If not provided, the server will get a
             random available port"
  ^ServerWithHandler [handler & {:keys [host port]
                                 :or {port 0}}]
  (let [server (HttpServer/create
                 (if host
                   (InetSocketAddress. ^String host ^int port)
                   (InetSocketAddress. port))
                 0)]
    (.createContext
      server
      "/"
      (reify HttpHandler
        (handle [_ exchange]
          (let [uri (.getRequestURI exchange)
                query (.getQuery uri)
                request-method (.getRequestMethod exchange)
                request (cond-> {:method request-method
                                 :path (.getPath uri)
                                 :headers (persistent!
                                            (reduce
                                              (fn [acc e]
                                                (let [k (.intern (util/lower-case* (key e)))
                                                      v (val e)
                                                      v (if (< 1 (count v))
                                                          (vec v)
                                                          (.get ^List v 0))]
                                                  (assoc! acc k v)))
                                              (transient {})
                                              (.entrySet (.getRequestHeaders exchange))))
                                 :body (.getRequestBody exchange)}
                          query
                          (assoc :query query))]
            (-> (try
                  (future/wrap (handler request))
                  (catch Throwable e (future/failed e)))
                (future/catch
                  (fn [e]
                    (or (::response (ex-data e))
                        (do (error-reporting/report-exception! e)
                            internal-server-error))))
                (future/then
                  (fn [response]
                    (future/io
                      (try
                        (let [{:keys [status headers body]} response]
                          (when-not (integer? status)
                            (throw (ex-info (str "Invalid response status: " status) {:status status})))
                          (when-not (or (nil? headers) (map? headers))
                            (throw (ex-info (str "Invalid response headers: " headers) {:headers headers})))
                          (let [connection (http-server.types/->connection body)]
                            (try
                              (let [;; Infer content-length and content-type for responses that
                                    ;; can't do that during response creation. For example, we
                                    ;; might cache a response with a particular file, but
                                    ;; the file itself might change between requests
                                    headers (-> headers
                                                (provide-header "content-type" (http-server.types/connection-content-type connection))
                                                (provide-header "content-length" (http-server.types/connection-content-length connection)))
                                    ;; Maybe don't write the data at all (we still need
                                    ;; to create the connection to infer content headers)
                                    connection (if (or (= "HEAD" request-method)
                                                       (= 304 status))
                                                 nil
                                                 connection)]
                                (reduce-kv #(doto ^Headers %1 (.add %2 %3)) (.getResponseHeaders exchange) headers)
                                (.sendResponseHeaders
                                  exchange
                                  status
                                  ;; Response length argument:
                                  ;; -1 means empty response body (0 bytes)
                                  ;; 0 is unknown size (chunked)
                                  ;; positive is exact size
                                  (if (nil? connection)
                                    -1
                                    (if-let [explicit-length (get headers "content-length")]
                                      (let [n (Long/parseUnsignedLong explicit-length)]
                                        (if (zero? n) -1 n))
                                      0)))
                                (when connection
                                  (http-server.types/connection-write! connection (.getResponseBody exchange))))
                              ;; A browser or remote http client can close the
                              ;; connection before we finished sending the response
                              ;; headers/body: we only log such exceptions since
                              ;; they are not an issue that needs to be fixed.
                              (catch IOException e
                                (log/warn :exception e))
                              (finally
                                (when (instance? Closeable connection)
                                  (.close ^Closeable connection))))))
                        (catch Throwable e
                          (error-reporting/report-exception! e))
                        (finally
                          (.close exchange)))))))))))
    (.start server)
    (let [server (->ServerWithHandler server handler)]
      (when-not (Boolean/getBoolean "defold.tests")
        (log/info :msg "Http server running" :local-url (local-url server)))
      server)))

(defn handler
  "Get the server request handler function"
  [^ServerWithHandler server]
  (.-handler server))

(defn stop!
  ^ServerWithHandler
  ([server] (stop! server 2))
  ([^ServerWithHandler server delay-seconds]
   (.stop ^HttpServer (.-server server) delay-seconds)
   server))

(defn- allowed-methods [method->handler]
  (let [method-set (into #{"OPTIONS"} (coll/keys method->handler))
        method-set (cond-> method-set (contains? method-set "GET") (conj "HEAD"))]
    (coll/join-to-string ", " (coll/sort method-set))))

(defn- invoke-handler [handler request router match]
  (handler (assoc request :path-params (:path-params match) :router router)))

(defn router-handler
  "Create HTTP request handler function from reitit routes

  Routes data could be a map of request methods to handler functions, e.g.:

  {\"/status\" {\"GET\" get-status}
   \"/resources/{*path}\" {\"GET\" get-resource}
   \"/command\" {\"GET\" list-commands}}
   \"/command/{command}\" {\"POST\" invoke-command}

  Use {name} in path to extract a single part of the path, or {*name} to extract
  the rest of the path until the end. Returned router:
  - strips or adds trailing slashes if needed to make a match
  - supports OPTIONS (CORS) requests
  - automatically supports HEAD requests when GET handler is defined

  Router handler functions receive an HTTP request and return either a response
  or a CompletableFuture that will contain the response.
  Request is a map with the following keys:
    :method         HTTP request method string, e.g. \"GET\"
    :path           URI path part string, e.g. \"/users/1\"
    :path-params    a possibly empty map of path extractor pattern (keyword) to
                    extracted value (string)
    :query          optional query string, e.g. \"q=1\"
    :headers        a map of headers, string->(string|string[]); header names
                    are lower-case
    :body           request body, InputStream
  Response is a map with the following keys:
    :status     optional HTTP response status, integer, default 200
    :headers    optional response headers map, string->string;
                header names should be lower-case
    :body       optional response body, could be nil, string
                content, or anything the server knows how to write after
                ->data/->connection normalization, typically an io/IOFactory
                value such as InputStream, File, or Path. If the opened
                connection is AutoCloseable, it will be closed even if it's not
                written (which might happen when responding to HEAD requests)
  See also:
    https://cljdoc.org/d/metosin/reitit-core/0.8.0-alpha1/doc/introduction"
  [routes]
  (let [router (reitit/router routes {:syntax :bracket})]
    (fn route-request [request]
      (let [path (:path request)]
        (if-let [match (or (reitit/match-by-path router path)
                           (if (string/ends-with? path "/")
                             (reitit/match-by-path router (subs path 0 (dec (count path))))
                             (reitit/match-by-path router (str path "/"))))]
          (let [method->handler (:data match)
                method (:method request)]
            (if-let [handler (method->handler method)]
              (invoke-handler handler request router match)
              (case method
                "OPTIONS" (response
                            200
                            {"access-control-allow-origin" "*"
                             "access-control-allow-methods" (allowed-methods method->handler)}
                            nil)
                "HEAD" (if-let [get-handler (method->handler "GET")]
                         (invoke-handler get-handler request router match) ;; http server will strip the body
                         (update method-not-allowed :headers assoc "allow" (allowed-methods method->handler)))
                (update method-not-allowed :headers assoc "allow" (allowed-methods method->handler)))))
          not-found)))))
