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
  (:require [clojure.string :as string]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.http-server :as http-server]
            [util.path :as path])
  (:import [java.net URI URISyntaxException]))

(set! *warn-on-reflection* true)

(def ^:const url-prefix "/build")
(def ^:const verify-etags-url-prefix "/__verify_etags__")

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
                                              (coll/join-to-string "\n"))))}}))
