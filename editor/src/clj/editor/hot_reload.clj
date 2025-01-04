;; Copyright 2020-2024 The Defold Foundation
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
            [util.http-util :as http-util])
  (:import [java.io IOException]
           [java.net URI]
           [java.nio.file Paths Files]
           [org.apache.commons.io FilenameUtils IOUtils]))

(set! *warn-on-reflection* true)

(def ^:const url-prefix "/build")
(def ^:const verify-etags-url-prefix "/__verify_etags__")

(defn- content->bytes [content]
  (-> content io/input-stream IOUtils/toByteArray))

(defn- handler [workspace _project {:keys [url method headers]}]
  (let [build-path (FilenameUtils/normalize (str (workspace/build-path workspace)))
        path (subs url (count url-prefix))
        full-path (format "%s%s" build-path path)]
    ;; Avoid going outside the build path with '..'
    (if-not (string/starts-with? full-path build-path)
      http-util/not-found-response
      (let [etag (workspace/etag workspace path)
            remote-etag (first (get headers "If-None-Match"))
            is-cached (when remote-etag (= etag remote-etag))
            file (io/file full-path)
            content-length (if is-cached
                             ;; The engine does not support a cached response
                             ;; that contains a content-length other than zero.
                             0
                             ;; Returns zero if not found.
                             (.length file))
            content (when (and (= method "GET") (not is-cached))
                      (try
                        (Files/readAllBytes (.toPath file))
                        (catch IOException _
                          :not-found)))]
        (if (= content :not-found)
          http-util/not-found-response
          (let [response-headers
                (cond-> {"ETag" etag}

                        (or (= method "GET")
                            (= method "HEAD"))
                        (assoc "Content-Length" (str content-length)))]
            (cond-> {:code (if is-cached 304 200)
                     :headers response-headers}

                    (and content
                         (= method "GET"))
                    (assoc :body content))))))))

(defn build-handler [workspace project request]
  (handler workspace project request))

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch java.net.URISyntaxException _
        nil))))

(defn- body->valid-entries [workspace ^bytes body]
  (into []
        (comp
          (keep (fn [line]
                  (let [[url-string etag] (string/split line #" ")]
                    (when-some [url (some-> url-string string->url)]
                      [url etag]))))
          (keep (fn [[^URI url etag]]
                  (let [path (.. url normalize getPath)]
                    (when (string/starts-with? path url-prefix)
                      (let [proj-path (subs path (count url-prefix))]
                        (when (= etag (workspace/etag workspace proj-path))
                          path)))))))
        (string/split-lines (String. body))))

(defn- v-e-handler [workspace project {:keys [url method headers ^bytes body]}]
  (if (not= method "POST")
    http-util/bad-request-response
    (let [body-str (string/join "\n" (body->valid-entries workspace body))
          body (.getBytes body-str "UTF-8")]
      {:code 200
       :headers {"Content-Length" (str (alength ^bytes body))}
       :body body})))

(defn verify-etags-handler [workspace project request]
  (v-e-handler workspace project request))
