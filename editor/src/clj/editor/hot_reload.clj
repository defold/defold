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

(ns editor.hot-reload
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.digest :as digest]
            [util.http-server :as http-server]
            [util.http-server.types :as http-server.types]
            [util.path :as path])
  (:import [java.io BufferedOutputStream IOException InputStream OutputStream]
           [java.net URI URISyntaxException URLDecoder URLEncoder]
           [java.nio.charset StandardCharsets]))

(set! *warn-on-reflection* true)

(def ^:const url-prefix "/build")
(def ^:const verify-etags-url-prefix "/__verify_etags__")

;; Keep the wire limits in sync with the HTTP resource provider.
(def ^:private max-batch-file-size (* 1024 1024))
(def ^:private max-batch-size (* 4 1024 1024))
(def ^:private max-batch-files 4096)
(def ^:private max-batch-request-size (* 1024 1024))

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch URISyntaxException _ nil))))

(defn- body->valid-entries [workspace body-string]
  (coll/into-> (string/split-lines body-string) []
    (keep (fn [line]
            (let [[url-string etag] (string/split line #" " 2)]
              (when-let [url (string->url url-string)]
                [url-string url etag]))))
    (keep (fn [[url-string ^URI url etag]]
            (when-let [path (.. url normalize getPath)]
              (when (and (not (string/blank? etag))
                         (string/starts-with? path (str url-prefix "/")))
                (let [proj-path (subs path (count url-prefix))]
                  (when (= etag (workspace/etag workspace proj-path))
                    ;; Preserve the cache key, including origin and URL
                    ;; escaping. The engine validates exact keys.
                    url-string))))))))

(defn- resource-batch-response [workspace build-path ^InputStream body]
  (try
    (let [request-bytes (.readNBytes body (inc max-batch-request-size))]
      (when (> (alength request-bytes) max-batch-request-size)
        (throw (http-server/error (http-server/response 413 "Batch request too large\n"))))
      (let [lines (string/split-lines (String. request-bytes StandardCharsets/UTF_8))
            artifacts (workspace/artifact-map workspace)]
        (when-not (<= 1 (count lines) max-batch-files)
          (throw (http-server/error (http-server/response 400 "Invalid batch file count\n"))))
        (let [entries
              (coll/into-> lines []
                (map (fn [line]
                       (let [[^String encoded-path requested-etag :as parts] (string/split line #"\t" -1)]
                         (when-not (= 2 (count parts))
                           (throw (http-server/error (http-server/response 400 "Invalid batch record\n"))))
                         (let [decoded-path (URLDecoder/decode encoded-path StandardCharsets/UTF_8)]
                           (when-not (string/starts-with? decoded-path (str url-prefix "/"))
                             (throw (http-server/error http-server/not-found)))
                           (let [proj-path (subs decoded-path (count url-prefix))
                                 full-path (path/resolve-normalized build-path (subs proj-path 1))
                                 {:keys [size etag] :as artifact} (get artifacts proj-path)]
                             (when-not (and artifact
                                            (path/starts-with? full-path build-path)
                                            (path/file? full-path))
                               (throw (http-server/error http-server/not-found)))
                             (when (or (not= etag requested-etag)
                                       (not= size (path/byte-size full-path)))
                               (throw (http-server/error (http-server/response 409 "Resource changed\n"))))
                             (when (> size max-batch-file-size)
                               (throw (http-server/error (http-server/response 413 "Resource exceeds batch limit\n"))))
                             {:path full-path
                              :size size
                              :etag etag

                              :header
                              (.getBytes (str encoded-path \tab size \tab etag \newline)
                                         StandardCharsets/UTF_8)}))))))

              payload-size (transduce (map :size) + entries)]
          (when (> payload-size max-batch-size)
            (throw (http-server/error (http-server/response 413 "Batch payload too large\n"))))
          (let [preamble (.getBytes "DM_RESOURCE_BATCH 1\n" StandardCharsets/UTF_8)

                content-length
                (+ payload-size
                   (alength preamble)
                   (transduce (map #(alength ^bytes (:header %))) + entries))]
            (http-server/response
              200
              {"content-type" "application/octet-stream"
               "cache-control" "no-store"}
              (reify
                http-server.types/ContentLength
                (content-length [_] content-length)

                http-server.types/ConnectionWrite
                (connection-write! [_ output-stream]
                  ;; Coalesce small headers and files into bounded writes.
                  (let [out (BufferedOutputStream. ^OutputStream output-stream (* 64 1024))]
                    (.write out preamble)
                    (doseq [{:keys [path size etag header]} entries]
                      ;; Builds rewrite these files in place. Validate a bounded
                      ;; snapshot before emitting any part of its batch entry.
                      ;; Read one extra byte to detect growth.
                      (let [^bytes content
                            (try
                              (let [^bytes content
                                    (with-open [input-stream (io/input-stream path)]
                                      (.readNBytes input-stream (inc (long size))))]
                                (when (or (not= size (alength content))
                                          (not= etag (digest/sha1-hex content)))
                                  (throw (IOException. "Resource changed during batch transfer")))
                                content)
                              (catch IOException error
                                ;; Preserve completed entries.
                                (.flush out)
                                (throw error)))]
                        (.write out ^bytes header)
                        (.write out content)))
                    (.flush out)))))))))
    (catch IllegalArgumentException _
      (http-server/response 400 "Invalid batch path\n"))
    (catch IOException _
      (http-server/response 409 "Resource changed\n"))))

(defn routes [workspace]
  (let [build-path (.toPath (workspace/build-path workspace))]
    {"/build/{*path}"
     {"GET" (bound-fn [{:keys [path-params headers]}]
              (let [^String path (:path path-params)
                    full-path (path/resolve-normalized build-path path)]
                (if (and (path/starts-with? full-path build-path) ;; Avoid going outside the build path with '..'
                         (path/file? full-path))
                  (let [etag (workspace/etag workspace (str "/" path))
                        remote-etag (get headers "if-none-match")]
                    (if (and remote-etag (= etag remote-etag))
                      (http-server/response 304 {"etag" etag} nil)
                      (http-server/response 200 {"etag" etag} full-path)))
                  http-server/not-found)))}

     "/__verify_etags__"
     {"POST" (bound-fn [{:keys [body]}]
               (http-server/response 200 (->> body
                                              slurp
                                              (body->valid-entries workspace)
                                              (coll/join-to-string "\n"))))}

     "/__resource_index__"
     {"GET" (bound-fn [_]
              ;; One build snapshot supplies both sizes and ETags, without
              ;; opening every resource to answer individual HEAD requests.
              (http-server/response
                200
                (str "DM_RESOURCE_INDEX 1\n"
                     (coll/join-to-string
                       (eduction
                         (map (fn [[proj-path {:keys [size etag]}]]
                                (str (URLEncoder/encode (str url-prefix proj-path)
                                                        StandardCharsets/UTF_8)
                                     \tab size \tab etag \newline)))
                         (workspace/artifact-map workspace))))))}

     "/__resource_batch__"
     {"POST" (bound-fn [{:keys [body]}]
               (resource-batch-response workspace build-path body))}}))
