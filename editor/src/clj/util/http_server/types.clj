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

(ns util.http-server.types
  (:require [util.defonce :as defonce]))

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
(defonce/protocol ContentType (content-type [body] "static content-type string or nil if unknown; default nil"))
(defonce/protocol ContentLength (content-length [body] "static content-length in bytes (long) or nil if unknown; default nil"))
(defonce/protocol ->Data (->data [body] "convert body to reusable immutable data"))
(defonce/protocol ->Connection (->connection [data] "open connection to HTTP response data before sending; the connection may know its content-type and content-length at send time, is written via connection-write!, and may be Closeable; default identity"))
(defonce/protocol ConnectionContentType (connection-content-type [connection] "dynamic content-type string or nil if unknown; default nil"))
(defonce/protocol ConnectionContentLength (connection-content-length [connection] "dynamic content-length in bytes (long) or nil if unknown; default nil"))
(defonce/protocol ConnectionWrite (connection-write! [connection output-stream] "write HTTP response body directly to output-stream"))
;; During response creation, if content-length and content-type weren't
;; explicitly provided, we try to infer them. We use `content-type` fn on a
;; provided body, and then try it on the data produced using `->data`. We
;; similarly try to get the content length by invoking `content-length` on body
;; and data. This prepares the immutable part of the response.
;; Then comes the dynamic part: when sending the response, we open the
;; connection to data. This connection might know its type and length when it's
;; established. For example, if response body is an HTTP URL/URI, the editor
;; server performs an HTTP request to a remote server that may respond to the
;; editor server with content-type and content-length headers. So, after opening
;; the connection, if we still don't know the content length and type of the
;; response, we ask the connection for its `connection-content-type` and
;; `connection-content-length`. We don't reuse the `content-type` and
;; `content-length` protocol fns here to make the developer aware that the
;; functions serve different purposes: `content-type` and `content-length` are
;; essentially static and don't change for a given response body, while
;; `connection-content-type` and `connection-content-length` are dynamic and may
;; change between responses given the same body.
