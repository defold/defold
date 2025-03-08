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
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.future :as future]
            [editor.util :as util]
            [reitit.core :as reitit]
            [service.log :as log])
  (:import [com.sun.net.httpserver Headers HttpHandler HttpServer]
           [java.io File IOException]
           [java.lang AutoCloseable]
           [java.net InetSocketAddress URI URL]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files Path]
           [java.util List]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.io.output ByteArrayOutputStream]))

(set! *warn-on-reflection* true)

(def ext->content-type
  ;; see also: https://svn.apache.org/repos/asf/httpd/httpd/trunk/docs/conf/mime.types
  {"aac" "audio/aac"
   "apng" "image/apng"
   "avif" "image/avif"
   "bmp" "image/bmp"
   "clj" "text/plain"
   "css" "text/css"
   "cur" "image/x-icon"
   "gif" "image/gif"
   "glsl" "text/plain"
   "htm" "text/html"
   "html" "text/html"
   "ico" "image/x-icon"
   "jfif" "image/jpeg"
   "jpeg" "image/jpeg"
   "jpg" "image/jpeg"
   "js" "text/javascript"
   "json" "application/json"
   "m4a" "audio/mp4"
   "m4v" "video/x-m4v"
   "mp3" "audio/mp3"
   "mp4" "video/mp4"
   "oga" "audio/ogg"
   "ogg" "audio/ogg"
   "ogv" "video/ogg"
   "pjp" "image/jpeg"
   "pjpeg" "image/jpeg"
   "png" "image/png"
   "shtml" "text/html"
   "svg" "image/svg+xml"
   "tif" "image/tif"
   "tiff" "image/tiff"
   "ttf" "font/ttf"
   "txt" "text/plain"
   "wasm" "application/wasm"
   "wav" "audio/wav"
   "webm" "video/webm"
   "webmanifest" "application/manifest+json"
   "webp" "image/webp"
   "woff" "font/woff"
   "woff2" "font/woff2"
   "xhtml" "application/xhtml+xml"
   "xml" "text/xml"
   "zip" "application/zip"})

(defn- path-content-type [s]
  (ext->content-type (FilenameUtils/getExtension s)))

;; We want to support a use-case where responses are created and then stored
;; somewhere until they are returned by some handler. Such use-case has 2
;; aspects:
;; - efficient representation of the response. For example, if we want to
;;   respond with String content, we immediately convert it to bytes and infer
;;   its content-type and content-length
;; - meeting the expectations when responding with resources. For example, if
;;   we create a response with a file Path and cache it, we expect that changing
;;   the file content would still be recognized by the server that responds with
;;   the cached response.
;; To achieve this, we use these protocols:
(defprotocol ContentType (content-type [body] "content-type string or nil if unknown; default nil"))
(defprotocol ContentData (content-data [body] "convert body response argument to reusable data; default identity"))
(defprotocol DataLength (data-length [data] "data length in bytes (long) or nil if unknown; default nil"))
(defprotocol DataConnection (data-connection [data] "convert data to connection that might know content-type and data-length when sent, should be io/IOFactory, could be AutoCloseable; default identity"))
;; During response creation, if content-length and content-type weren't
;; explicitly provided, we try to infer them. We use `content-type` fn on a
;; provided body, then we convert it to reusable data using `content-data`, and
;; finally we try to get the content length by invoking `data-length` on data.
;; This prepares the immutable part of the response.
;; Then comes the dynamic part: when sending the response, we open the
;; connection to data. This connection might know its type and length when it's
;; established. For example, if response body is an HTTP URL/URI, the editor
;; server performs an HTTP request to a remote server that may respond to the
;; editor server with content-type and content-length headers. So, after opening
;; the connection, if we still don't know the content length and type of the
;; response, we ask the connection for its `content-type` and `data-length`.

(extend-protocol ContentType
  String (content-type [_] "text/plain; charset=utf-8")
  File (content-type [file] (path-content-type (str file)))
  Path (content-type [path] (path-content-type (str path)))
  URL (content-type [url]
        (case (.getProtocol url)
          ("file" "jar") (path-content-type (.getPath url))
          nil))
  Object (content-type [_])
  nil (content-type [_]))

(extend-protocol ContentData
  String (content-data [s] (.getBytes s StandardCharsets/UTF_8))
  File (content-data [file] (.toPath file))
  Object (content-data [x] x)
  nil (content-data [x] x))

(extend-protocol DataLength
  byte/1 (data-length [bytes] (count bytes))
  Object (data-length [_])
  nil (data-length [_]))

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
               satisfies io/IOFactory (e.g. InputStream, File, Path, etc.)"
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
         data (content-data body)
         ;; Infer length if possible to do immediately
         length (data-length data)
         headers (-> headers
                     (provide-header "content-type" (content-type body))
                     (provide-header "content-length" length))]
     (cond-> {:status status}
             headers (assoc :headers headers)
             (and data (or (not length) (pos? length))) (assoc :body data)))))

(defn json-response [json-value]
  (let [baos (ByteArrayOutputStream.)]
    (with-open [writer (io/writer baos)]
      (json/write json-value writer))
    (response 200 {"content-type" "application/json"} (.toByteArray baos))))

(defn- make-status-response [status body]
  (response status (str status \space body \newline)))

(def accepted (make-status-response 202 "Accepted"))
(def forbidden (make-status-response 403 "Forbidden"))
(def not-found (make-status-response 404 "Not Found"))
(def method-not-allowed (make-status-response 405 "Method Not Allowed"))
(def internal-server-error (make-status-response 500 "Internal Server Error"))
(defn redirect [location] (response 302 {"location" location} nil))

(defn port [^HttpServer server]
  (.getPort (.getAddress server)))

(defn local-url [server]
  (format "http://localhost:%d" (port server)))

(defn url [^HttpServer server]
  (let [address (.getAddress server)]
    (format "http://%s:%d" (.getHostString address) (.getPort address))))

(extend-protocol DataConnection
  Path (data-connection [path]
         (reify
           DataLength
           (data-length [_] (Files/size path))
           io/IOFactory
           (make-input-stream [_ opts] (io/make-input-stream path opts))
           (make-output-stream [_ opts] (io/make-output-stream path opts))
           (make-reader [_ opts] (io/make-reader path opts))
           (make-writer [_ opts] (io/make-writer path opts))))
  URL (data-connection [url]
        (let [connection (.openConnection url)]
          (reify
            DataLength
            (data-length [_]
              (let [len (.getContentLengthLong connection)]
                (when-not (= -1 len) len)))
            ContentType
            (content-type [_]
              (.getContentType connection))
            io/IOFactory
            (make-input-stream [_ opts] (io/make-input-stream (.getInputStream connection) opts))
            (make-output-stream [_ opts] (io/make-output-stream (.getOutputStream connection) opts))
            (make-reader [_ opts] (io/make-reader (.getInputStream connection) opts))
            (make-writer [_ opts] (io/make-writer (.getOutputStream connection) opts))
            AutoCloseable
            (close [_]
              (.close (.getInputStream connection))))))
  URI (data-connection [uri] (data-connection (.toURL uri)))
  Object (data-connection [x] x)
  nil (data-connection [x] x))

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
                             content, or anything that satisfies io/IOFactory
                             (e.g. InputStream, File, Path etc.). If body is
                             AutoCloseable, it will be closed even if it's not
                             written (which might happen when responding to HEAD
                             requests)
               Use [[response]] fn to produce the response

  Kv-args (all optional):
    :host    HTTP server host, string
    :port    HTTP server port, integer. If not provided, the server will get a
             random available port"
  ^HttpServer [handler & {:keys [host port]
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
                                                (let [k (key e)
                                                      v (val e)]
                                                  (assoc! acc (util/lower-case* k) (if (< 1 (count v)) (vec v) (.get ^List v 0)))))
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
                    (error-reporting/report-exception! e)
                    internal-server-error))
                (future/then
                  (fn [response]
                    (future/io
                      (try
                        (let [{:keys [status headers body]} response
                              connection (data-connection body)
                              ;; Infer content-length and content-type for responses that
                              ;; can't do that during response creation. For example, we
                              ;; might cache a response with a particular file, but
                              ;; the file itself might change between requests
                              headers (-> headers
                                          (provide-header "content-type" (content-type connection))
                                          (provide-header "content-length" (data-length connection)))

                              connection (if (or (= "HEAD" request-method)
                                                 (= 304 status))
                                           (do
                                             (when (instance? AutoCloseable connection)
                                               (.close ^AutoCloseable connection))
                                             nil)
                                           connection)]
                          (when-not (integer? status)
                            (throw (ex-info (str "Invalid response status: " status) {:status status})))
                          (when-not (or (nil? headers) (map? headers))
                            (throw (ex-info (str "Invalid response headers: " headers) {:headers headers})))
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
                            (try
                              (with-open [is (io/input-stream connection)]
                                (try
                                  (io/copy is (.getResponseBody exchange))
                                  ;; A browser or remote http client can close
                                  ;; the connection before we finished sending
                                  ;; the response: ignore such exceptions since
                                  ;; it's not an issue.
                                  (catch IOException _)))
                              (finally
                                (when (instance? AutoCloseable connection)
                                  (.close ^AutoCloseable connection))))))
                        (catch Throwable e
                          (error-reporting/report-exception! e))
                        (finally
                          (.close exchange)))))))))))
    (.start server)
    (when-not (Boolean/getBoolean "defold.tests")
      (log/info :msg "Http server running" :local-url (local-url server)))
    server))

(defn stop!
  ^HttpServer
  ([server] (stop! server 2))
  ([^HttpServer server delay-seconds]
   (doto server (.stop delay-seconds))))

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
                content, or anything that satisfies io/IOFactory
                (e.g. InputStream, File, Path etc.). If body is AutoCloseable,
                it will be closed even if it's not written (which might happen
                when responding to HEAD requests)
  See also:
    https://cljdoc.org/d/metosin/reitit-core/0.8.0-alpha1/doc/introduction"
  [routes]
  (let [allowed-methods (fn allowed-methods [method->handler]
                          (let [method-set (-> method->handler keys set (conj "OPTIONS"))
                                method-set (cond-> method-set (contains? method-set "GET") (conj "HEAD"))]
                            (string/join ", " (sort method-set))))
        invoke-handler (fn invoke-handler [handler request match]
                         (handler (assoc request :path-params (:path-params match))))
        router (reitit/router routes {:syntax :bracket})]
    (fn route-request [request]
      (let [path (:path request)]
        (if-let [match (or (reitit/match-by-path router path)
                           (if (string/ends-with? path "/")
                             (reitit/match-by-path router (subs path 0 (dec (count path))))
                             (reitit/match-by-path router (str path "/"))))]
          (let [method->handler (:data match)
                method (:method request)]
            (if-let [handler (method->handler method)]
              (invoke-handler handler request match)
              (case method
                "OPTIONS" (response
                            200
                            {"access-control-allow-origin" "*"
                             "access-control-allow-methods" (allowed-methods method->handler)}
                            nil)
                "HEAD" (if-let [get-handler (method->handler "GET")]
                         (invoke-handler get-handler request match) ;; http server will strip the body
                         (update method-not-allowed :headers assoc "allow" (allowed-methods method->handler)))
                (update method-not-allowed :headers assoc "allow" (allowed-methods method->handler)))))
          not-found)))))
